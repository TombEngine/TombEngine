#include "framework.h"
#include "Objects/TR5/Entity/tr5_larson.h"

#include "Game/Animation/Animation.h"
#include "Game/control/box.h"
#include "Game/control/control.h"
#include "Game/effects/effects.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/misc.h"
#include "Game/people.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Specific/level.h"

using namespace TEN::Animation;
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

	const auto LarsonGun	  = CreatureBiteInfo(Vector3(-55, 200, 5), 14);
	const auto PierreGunLeft  = CreatureBiteInfo(Vector3(45, 200, 0), 11);
	const auto PierreGunRight = CreatureBiteInfo(Vector3(-40, 200, 0), 14);

	void InitializeLarson(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		InitializeCreature(itemNumber);
		SetAnimation(*item, 0);

		if (!item->TriggerFlags)
			return;

		item->ItemFlags[3] = item->TriggerFlags;
		short rotY = item->Pose.Orientation.y;

		if (rotY > ANGLE(22.5f) && rotY < ANGLE(157.5f))
			item->Pose.Position.x += STEPUP_HEIGHT;
		else if (rotY < ANGLE(-22.5f) && rotY > ANGLE(-157.5f))
			item->Pose.Position.x -= STEPUP_HEIGHT;
		else if (rotY < ANGLE(-112.5f) || rotY > ANGLE(112.5f))
			item->Pose.Position.z -= STEPUP_HEIGHT;
		else if (rotY > ANGLE(-45.0f) || rotY < ANGLE(45.0f))
			item->Pose.Position.z += STEPUP_HEIGHT;
	}

	// TODO: Make larson 1:1 from TOMB5 code. TokyoSU: 10/27/2024
	// This code is a mess...
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

		// TODO: When Larson's HP is below 40, he runs away in Streets of Rome. Keeping block commented for reference.
		if (item->HitPoints <= TR5_LARSON_MIN_HP && !(item->Flags & IFLAG_INVISIBLE))
		{
			item->HitPoints = TR5_LARSON_MIN_HP;
			creature->Flags++;
		}

		if (creature->MuzzleFlash[0].Delay != 0)
			creature->MuzzleFlash[0].Delay--;
		if (creature->MuzzleFlash[1].Delay != 0)
			creature->MuzzleFlash[1].Delay--;

		if (item->TriggerFlags)
		{
			if (CurrentLevel == 2)
			{
				item->AIBits = AMBUSH;
				item->ItemFlags[3] = 1;
			}
			else
			{
				item->AIBits = GUARD;
			}
			item->TriggerFlags = 0;
		}

		if (item->HitPoints > 0)
		{
			if (item->AIBits)
				GetAITarget(creature);
			else
				creature->Enemy = LaraItem;

			AI_INFO AI;
			CreatureAIInfo(item, &AI);

			if (AI.ahead)
				joint2 = AI.angle;

			// FIXME: This should make Larson run away, but it doesn't work.
			// FIXME: 10/27/2024 - TokyoSU: Implemented TOMB5 way, should work now but need test.
			if (creature->Flags)
			{
				item->HitPoints = 60;
				item->AIBits = AMBUSH;
				creature->Flags = 0;
			}

			GetCreatureMood(item, &AI, true);
			CreatureMood(item, &AI, true);

			if (AI.distance < LARSON_ALERT_RANGE &&
				LaraItem->Animation.Velocity.z > 20.0f ||
				item->HitStatus ||
				TargetVisible(item, &AI) != 0)
			{
				item->AIBits &= ~GUARD;
				creature->Alerted = true;
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
					if (item->AIBits & GUARD || CurrentLevel == 2 || item->ItemFlags[3])
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
				
				if (item->Animation.FrameNumber == 0)
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
				TestLastFrame(*item))
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
				item->Animation.AnimNumber = ANIMATION_TR5_PIERRE_DIE;
			else
				item->Animation.AnimNumber = ANIMATION_TR5_LARSON_DIE;

			item->Animation.FrameNumber = 0;
			item->Animation.ActiveState = STATE_TR5_LARSON_DIE;
		}

		CreatureTilt(item, tilt);
		CreatureJoint(item, 0, joint0);
		CreatureJoint(item, 1, joint1);
		CreatureJoint(item, 2, joint2);
		CreatureAnimation(itemNumber, angle, 0);

		/*if (creature->reachedGoal)
		{
			if (CurrentLevel == 2)
			{
				item->TargetState = STATE_TR5_LARSON_STOP;
				item->RequiredState = STATE_TR5_LARSON_STOP;
				creature->ReachedGoal = false;
				item->IsAirborne = false;
				item->HitStatus = false;
				item->Collidable = false;
				item->Status = ITEM_NOT_ACTIVE;
				item->TriggerFlags = 0;
			}
			else
			{
				item->HitPoints = NOT_TARGETABLE;
				DisableEntityAI(itemNumber);
				KillItem(itemNumber);
			}
		}*/
	}
}
