#include "framework.h"
#include "Objects/TR5/Entity/tr5_larson.h"

#include "Game/animation.h"
#include "Game/camera.h"
#include "Game/control/box.h"
#include "Game/control/control.h"
#include "Game/control/los.h"
#include "Game/control/lot.h"
#include "Game/effects/effects.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/misc.h"
#include "Game/people.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Specific/level.h"

using namespace TEN::Math;

namespace TEN::Entities::Creatures::TR5
{
	constexpr auto LARSON_ALERT_RANGE = SQUARE(BLOCK(2));

	#define STATE_TR5_LARSON_STOP	1
	#define STATE_TR5_LARSON_WALK	2
	#define STATE_TR5_LARSON_RUN	3
	#define STATE_TR5_LARSON_AIM	4
	#define STATE_TR5_LARSON_DIE	5
	#define STATE_TR5_LARSON_IDLE	6
	#define STATE_TR5_LARSON_ATTACK	7

	#define ANIMATION_TR5_PIERRE_DIE 12
	#define ANIMATION_TR5_LARSON_DIE 15

	#define TR5_LARSON_MIN_HP 40
	#define TR5_LARSON_DISAPPEAR_FRAME_COUNT 15

	const auto LarsonGun	  = CreatureBiteInfo(Vector3(-55, 200, 5), 14);
	const auto PierreGunLeft  = CreatureBiteInfo(Vector3(45, 200, 0), 11);
	const auto PierreGunRight = CreatureBiteInfo(Vector3(-40, 200, 0), 14);

	void InitializeLarson(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		InitializeCreature(itemNumber);
		SetAnimation(item, 0);

		item->ItemFlags[3] = item->TriggerFlags;
	}

	void LarsonControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto* creature = GetCreatureInfo(item);

		short angle = 0;
		short tilt = 0;
		short joint0 = 0;
		short joint1 = 0;
		short joint2 = 0;

		// When Larson's HP is below 40 and his OCB is not 0, he runs away and disappears.
		if (item->HitPoints <= TR5_LARSON_MIN_HP && item->TriggerFlags)
		{
			item->HitPoints = TR5_LARSON_MIN_HP;
			creature->Flags++;
		}

		if (creature->MuzzleFlash[0].Delay != 0)
			creature->MuzzleFlash[0].Delay--;
		if (creature->MuzzleFlash[1].Delay != 0)
			creature->MuzzleFlash[1].Delay--;

		if (item->HitPoints > 0)
		{
			// If Larson is in the process of escaping, force ambush AI flag on him.
			if (creature->Flags)
				item->AIBits |= AMBUSH;

			// HACK: Reset enemy if AI bits were unset, otherwise Larson will run around last used AI object.
			if (!item->AIBits)
				creature->Enemy = nullptr;

			AI_INFO AI;
			GetAITarget(creature);
			CreatureAIInfo(item, &AI);

			// HACK: Reset Lara enemy in case no AI_AMBUSH object is present.
			// Technically it's illegal, but TEN code forces enemy to Lara after calling GetAITarget, resulting
			// in Larson never running away.
			if (item->AIBits & AMBUSH && creature->Enemy->ObjectNumber != GAME_OBJECT_ID::ID_AI_AMBUSH)
				creature->Enemy = nullptr;

			if (AI.ahead)
				joint2 = AI.angle;

			GetCreatureMood(item, &AI, true);
			CreatureMood(item, &AI, true);

			if (!(item->AIBits & AMBUSH))
			{
				// Set Larson to attack if player is moving fast enough and close enough, or if Larson was hit,
				// or if player is directly visible.
				if ((AI.distance < LARSON_ALERT_RANGE && creature->Enemy->Animation.Velocity.z > 20.0f) ||
					(TargetVisible(item, &AI) != 0) ||
					item->HitStatus)
				{
					item->AIBits &= ~GUARD;
					creature->Alerted = true;

					// creature->Enemy will contain AI object when Larson is patrolling or guarding, so we reset it.
					if (!creature->Enemy->IsLara())
						creature->Enemy = nullptr;
				}
			}

			angle = CreatureTurn(item, creature->MaxTurn);

			switch (item->Animation.ActiveState)
			{
			case STATE_TR5_LARSON_STOP:
				joint0 = AI.angle / 2;
				joint2 = AI.angle / 2;

				if (AI.ahead)
					joint1 = AI.xAngle;

				if (item->Animation.RequiredState != NO_VALUE)
					item->Animation.TargetState = item->Animation.RequiredState;
				else if (item->AIBits & AMBUSH)
					item->Animation.TargetState = STATE_TR5_LARSON_RUN;
				else if (Targetable(item, &AI))
					item->Animation.TargetState = STATE_TR5_LARSON_AIM;
				else
				{
					if (item->AIBits & GUARD)
					{
						item->Animation.TargetState = STATE_TR5_LARSON_STOP;
						creature->MaxTurn = 0;

						if (abs(AI.angle) >= ANGLE(2.0f))
						{
							if (AI.angle > 0)
								item->Pose.Orientation.y += ANGLE(2.0f);
							else
								item->Pose.Orientation.y -= ANGLE(2.0f);
						}
						else
							item->Pose.Orientation.y += AI.angle;
					}
					else
					{
						if (creature->Mood != MoodType::Bored)
						{
							if (creature->Mood == MoodType::Escape)
								item->Animation.TargetState = STATE_TR5_LARSON_RUN;
							else
								item->Animation.TargetState = STATE_TR5_LARSON_WALK;
						}
						else
							item->Animation.TargetState = Random::TestProbability(0.997f) ? 2 : 6;
					}
				}

				break;

			case STATE_TR5_LARSON_WALK:
				creature->MaxTurn = ANGLE(7.0f);

				if (AI.ahead)
					joint2 = AI.angle;

				if (creature->Mood == MoodType::Bored && Random::TestProbability(1.0f / 340))
				{
					item->Animation.TargetState = STATE_TR5_LARSON_STOP;
					item->Animation.RequiredState = STATE_TR5_LARSON_IDLE;
					break;
				}

				if (creature->Mood == MoodType::Escape || item->AIBits & AMBUSH)
				{
					item->Animation.TargetState = STATE_TR5_LARSON_STOP;
					item->Animation.RequiredState = STATE_TR5_LARSON_RUN;
				}
				else if (Targetable(item, &AI))
				{
					item->Animation.TargetState = STATE_TR5_LARSON_STOP;
					item->Animation.RequiredState = STATE_TR5_LARSON_AIM;
				}
				else if (!AI.ahead || AI.distance > SQUARE(BLOCK(3)))
				{
					item->Animation.TargetState = STATE_TR5_LARSON_STOP;
					item->Animation.RequiredState = STATE_TR5_LARSON_RUN;
				}

				break;

			case STATE_TR5_LARSON_RUN:
				creature->MaxTurn = ANGLE(11.0f);
				tilt = angle / 2;

				if (AI.ahead)
					joint2 = AI.angle;

				if (creature->ReachedGoal)
					item->Animation.TargetState = STATE_TR5_LARSON_STOP;
				else if (item->AIBits & AMBUSH)
					item->Animation.TargetState = STATE_TR5_LARSON_RUN;
				else if (creature->Mood != MoodType::Bored || Random::TestProbability(0.997f))
				{
					if (Targetable(item, &AI))
					{
						item->Animation.TargetState = STATE_TR5_LARSON_STOP;
						item->Animation.RequiredState = STATE_TR5_LARSON_AIM;
					}
					else if (AI.ahead)
					{
						if (AI.distance <= SQUARE(BLOCK(3)))
						{
							item->Animation.TargetState = STATE_TR5_LARSON_STOP;
							item->Animation.RequiredState = STATE_TR5_LARSON_WALK;
						}
					}
				}
				else
				{
					item->Animation.TargetState = STATE_TR5_LARSON_STOP;
					item->Animation.RequiredState = STATE_TR5_LARSON_IDLE;
				}

				break;

			case STATE_TR5_LARSON_AIM:
				creature->MaxTurn = 0;
				joint0 = AI.angle / 2;
				joint2 = AI.angle / 2;

				if (AI.ahead)
					joint1 = AI.xAngle;

				if (abs(AI.angle) >= ANGLE(2.0f))
				{
					if (AI.angle > 0)
						item->Pose.Orientation.y += ANGLE(2.0f);
					else
						item->Pose.Orientation.y -= ANGLE(2.0f);
				}
				else
					item->Pose.Orientation.y += AI.angle;

				if (Targetable(item, &AI))
					item->Animation.TargetState = STATE_TR5_LARSON_ATTACK;
				else
					item->Animation.TargetState = STATE_TR5_LARSON_STOP;

				break;

			case STATE_TR5_LARSON_IDLE:
				joint0 = AI.angle / 2;
				joint2 = AI.angle / 2;

				if (AI.ahead)
					joint1 = AI.xAngle;

				if (creature->Mood != MoodType::Bored)
					item->Animation.TargetState = STATE_TR5_LARSON_STOP;
				else
				{
					if (Random::TestProbability(1.0f / 340))
					{
						item->Animation.TargetState = STATE_TR5_LARSON_STOP;
						item->Animation.RequiredState = STATE_TR5_LARSON_WALK;
					}
				}

				break;

			case STATE_TR5_LARSON_ATTACK:
				creature->MaxTurn = 0;
				joint0 = AI.angle / 2;
				joint2 = AI.angle / 2;

				if (AI.ahead)
					joint1 = AI.xAngle;

				if (abs(AI.angle) >= ANGLE(2.0f))
				{
					if (AI.angle > 0)
						item->Pose.Orientation.y += ANGLE(2.0f);
					else
						item->Pose.Orientation.y -= ANGLE(2.0f);
				}
				else
				{
					item->Pose.Orientation.y += AI.angle;
				}
				
				if (item->Animation.FrameNumber == GetAnimData(item).frameBase)
				{
					if (item->ObjectNumber == ID_PIERRE)
					{
						ShotLara(item, &AI, PierreGunLeft, joint0, 20);
						ShotLara(item, &AI, PierreGunRight, joint0, 20);

						creature->MuzzleFlash[0].Bite = PierreGunLeft;
						creature->MuzzleFlash[0].Delay = 2;
						creature->MuzzleFlash[1].Bite = PierreGunRight;
						creature->MuzzleFlash[1].Delay = 2;
					}
					else
					{
						ShotLara(item, &AI, LarsonGun, joint0, 20);
						creature->MuzzleFlash[0].Bite = LarsonGun;
						creature->MuzzleFlash[0].Delay = 2;
					}
				}

				if (creature->Mood == MoodType::Escape && Random::TestProbability(0.75f))
					item->Animation.RequiredState = STATE_TR5_LARSON_STOP;

				break;

			default:
				break;
			}
		}
		else if (item->Animation.ActiveState == STATE_TR5_LARSON_DIE)
		{
			// When Larson dies, it activates trigger at start position
			if (item->ObjectNumber == ID_LARSON &&
				item->Animation.FrameNumber == GetAnimData(item).frameEnd)
			{
				short roomNumber = item->ItemFlags[2] & 0xFF;
				short floorHeight = item->ItemFlags[2] & 0xFF00;

				auto* room = &g_Level.Rooms[roomNumber];

				int x = room->Position.x + (creature->Tosspad / 256 & 0xFF) * BLOCK(1) + 512;
				int y = room->BottomHeight + floorHeight;
				int z = room->Position.z + (creature->Tosspad & 0xFF) * BLOCK(1) + 512;

				TestTriggers(x, y, z, roomNumber, true);

				joint0 = 0;
			}
		}
		else
		{
			// Death.
			if (item->ObjectNumber == ID_PIERRE)
				item->Animation.AnimNumber = Objects[ID_PIERRE].animIndex + ANIMATION_TR5_PIERRE_DIE;
			else
				item->Animation.AnimNumber = Objects[ID_LARSON].animIndex + ANIMATION_TR5_LARSON_DIE;

			item->Animation.FrameNumber = GetAnimData(item).frameBase;
			item->Animation.ActiveState = STATE_TR5_LARSON_DIE;
		}

		CreatureTilt(item, tilt);
		CreatureJoint(item, 0, joint0);
		CreatureJoint(item, 1, joint1);
		CreatureJoint(item, 2, joint2);
		CreatureAnimation(itemNumber, angle, 0);

		if (creature->Flags != 0)
		{
			auto start = Camera.pos;
			auto target = GameVector(item->Pose.Position, item->RoomNumber);
			target.y -= BLOCK(1);

			bool shouldReachGoal = creature->AITarget->ObjectNumber == GAME_OBJECT_ID::ID_AI_AMBUSH &&
								   creature->AITarget->TriggerFlags == item->TriggerFlags;

			if (LOS(&start, &target))
				creature->Flags = 1;

			// If Larson/Pierre are in the process of escaping, disable them if they are out of sight
			// for more than 10 frames if no AI_AMBUSH object was set, or when they reach set AI_AMBUSH object.
			if ((shouldReachGoal && creature->ReachedGoal) ||
				(!shouldReachGoal && creature->Flags > TR5_LARSON_DISAPPEAR_FRAME_COUNT))
			{
				item->HitPoints = NOT_TARGETABLE;
				DisableEntityAI(itemNumber);
				KillItem(itemNumber);
			}
		}
	}
}
