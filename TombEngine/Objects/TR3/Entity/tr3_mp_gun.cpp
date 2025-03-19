#include "framework.h"
#include "Objects/TR3/Entity/tr3_mp_gun.h"

#include "Game/Animation/Animation.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/Point.h"
#include "Game/control/box.h"
#include "Game/control/lot.h"
#include "Game/effects/effects.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/misc.h"
#include "Game/people.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Collision::Point;
using namespace TEN::Math;

namespace TEN::Entities::Creatures::TR3
{
	const auto MPGunBite = CreatureBiteInfo(Vector3(0, 225, 50), 13);

	enum MPGunState
	{
		// No state 0.
		MPGUN_STATE_WAIT = 1,
		MPGUN_STATE_WALK = 2,
		MPGUN_STATE_RUN = 3,
		MPGUN_STATE_AIM_1 = 4,
		MPGUN_STATE_SHOOT_1 = 5,
		MPGUN_STATE_AIM_2 = 6,
		MPGUN_STATE_SHOOT_2 = 7,
		MPGUN_STATE_SHOOT_3A = 8,
		MPGUN_STATE_SHOOT_3B = 9,
		MPGUN_STATE_SHOOT_4A = 10,
		MPGUN_STATE_AIM_3 = 11,
		MPGUN_STATE_AIM_4 = 12,
		MPGUN_STATE_DEATH = 13,
		MPGUN_STATE_SHOOT_4B = 14,
		MPGUN_STATE_CROUCH = 15,
		MPGUN_STATE_CROUCHED = 16,
		MPGUN_STATE_CROUCH_AIM = 17,
		MPGUN_STATE_CROUCH_SHOT = 18,
		MPGUN_STATE_CROUCH_WALK = 19,
		MPGUN_STATE_STAND = 20
	};

	// TODO
	enum MPGunAnim
	{

	};

	void MPGunControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto* creature = GetCreatureInfo(item);

		short angle = 0;
		short tilt = 0;
		short head = 0;
		auto extraTorsoRot = EulerAngles::Identity;

		if (creature->MuzzleFlash[0].Delay != 0)
			creature->MuzzleFlash[0].Delay--;

		if (item->BoxNumber != NO_VALUE && (g_Level.PathfindingBoxes[item->BoxNumber].flags & BLOCKED))
		{
			DoDamage(item, 20);
			DoLotsOfBlood(item->Pose.Position.x, item->Pose.Position.y - (GetRandomControl() & 255) - 32, item->Pose.Position.z, (GetRandomControl() & 127) + 128, GetRandomControl() * 2, item->RoomNumber, 3);
		}

		AI_INFO AI;
		AI_INFO laraAI;
		if (item->HitPoints <= 0)
		{
			item->HitPoints = 0;
			if (item->Animation.ActiveState != 13)
			{
				item->Animation.AnimNumber = 14;
				item->Animation.FrameNumber = 0;
				item->Animation.ActiveState = 13;
			}
			else if (Random::TestProbability(0.25f) && item->Animation.FrameNumber == 1)
			{
				CreatureAIInfo(item, &AI);

				if (Targetable(item, &AI))
				{
					if (AI.angle > -ANGLE(45.0f) && AI.angle < ANGLE(45.0f))
					{
						head = AI.angle;
						extraTorsoRot.y = AI.angle;

						ShotLara(item, &AI, MPGunBite, extraTorsoRot.y, 32);
						creature->MuzzleFlash[0].Bite = MPGunBite;
						creature->MuzzleFlash[0].Delay = 2;
						SoundEffect(SFX_TR3_OIL_SMG_FIRE, &item->Pose, SoundEnvironment::Land, 1.0f, 0.7f);
					}
				}
			}
		}
		else
		{
			if (item->AIBits)
			{
				GetAITarget(creature);
			}
			else
			{
				creature->Enemy = LaraItem;

				int dx = LaraItem->Pose.Position.x - item->Pose.Position.x;
				int dz = LaraItem->Pose.Position.z - item->Pose.Position.z;

				laraAI.distance = pow(dx, 2) + pow(dx, 2);

				for (auto& currentCreature : ActiveCreatures)
				{
					if (currentCreature->ItemNumber == NO_VALUE || currentCreature->ItemNumber == itemNumber)
						continue;

					auto* target = &g_Level.Items[currentCreature->ItemNumber];
					if (target->ObjectNumber != ID_LARA)
						continue;

					dx = target->Pose.Position.x - item->Pose.Position.x;
					dz = target->Pose.Position.z - item->Pose.Position.z;

					int distance = pow(dx, 2) + pow(dz, 2);
					if (distance < laraAI.distance)
						creature->Enemy = target;
				}
			}

			CreatureAIInfo(item, &AI);

			if (creature->Enemy == LaraItem)
			{
				laraAI.angle = AI.angle;
				laraAI.distance = AI.distance;
			}
			else
			{
				int dx = LaraItem->Pose.Position.x - item->Pose.Position.x;
				int dz = LaraItem->Pose.Position.z - item->Pose.Position.z;
				laraAI.angle = phd_atan(dz, dx) - item->Pose.Orientation.y;
				laraAI.distance = pow(dx, 2) + pow(dz, 2);
			}

			GetCreatureMood(item, &AI, creature->Enemy != LaraItem ? true : false);
			CreatureMood(item, &AI, creature->Enemy != LaraItem ? true : false);

			angle = CreatureTurn(item, creature->MaxTurn);

			int x = item->Pose.Position.x + BLOCK(1) * phd_sin(item->Pose.Orientation.y + laraAI.angle);
			int y = item->Pose.Position.y;
			int z = item->Pose.Position.z + BLOCK(1) * phd_cos(item->Pose.Orientation.y + laraAI.angle);

			int height = GetPointCollision(Vector3i(x, y, z), item->RoomNumber).GetFloorHeight();
			bool cover = (item->Pose.Position.y > (height + CLICK(3)) && item->Pose.Position.y < (height + CLICK(4.5f)) && laraAI.distance > pow(BLOCK(1), 2));

			auto* enemy = creature->Enemy;
			creature->Enemy = LaraItem;

			if (laraAI.distance < pow(BLOCK(1), 2) || item->HitStatus || TargetVisible(item, &laraAI))
			{
				if (!creature->Alerted)
					SoundEffect(SFX_TR3_AMERCAN_HOY, &item->Pose);

				AlertAllGuards(itemNumber);
			}

			creature->Enemy = enemy;

			switch (item->Animation.ActiveState)
			{
			case MPGUN_STATE_WAIT:
				creature->MaxTurn = 0;
				head = laraAI.angle;

				if (item->Animation.AnimNumber == 17 ||
					item->Animation.AnimNumber == 27 ||
					item->Animation.AnimNumber == 28)
				{
					if (abs(AI.angle) < ANGLE(10.0f))
					{
						item->Pose.Orientation.y += AI.angle;
					}
					else if (AI.angle < 0)
					{
						item->Pose.Orientation.y -= ANGLE(10.0f);
					}
					else
					{
						item->Pose.Orientation.y += ANGLE(10.0f);
					}
				}

				if (item->AIBits & GUARD)
				{
					head = AIGuard(creature);
					item->Animation.TargetState = MPGUN_STATE_WAIT;
					break;
				}
				else if (item->AIBits & PATROL1)
				{
					item->Animation.TargetState = MPGUN_STATE_WALK;
					head = 0;
				}
				else if (cover && (Lara.TargetEntity == item || item->HitStatus))
				{
					item->Animation.TargetState = MPGUN_STATE_CROUCH;
				}
				else if (item->Animation.RequiredState == MPGUN_STATE_CROUCH)
				{
					item->Animation.TargetState = MPGUN_STATE_CROUCH;
				}
				else if (creature->Mood == MoodType::Escape)
				{
					item->Animation.TargetState = MPGUN_STATE_RUN;
				}
				else if (Targetable(item, &AI))
				{
					if (Random::TestProbability(0.25f))
					{
						item->Animation.TargetState = MPGUN_STATE_SHOOT_1;
					}
					else if (Random::TestProbability(1 / 2.0f))
					{
						item->Animation.TargetState = MPGUN_STATE_SHOOT_2;
					}
					else
					{
						item->Animation.TargetState = MPGUN_STATE_AIM_3;
					}
				}
				else if (creature->Mood == MoodType::Bored ||
					(item->AIBits & FOLLOW && (creature->ReachedGoal || laraAI.distance > pow(BLOCK(2), 2))))
				{
					if (AI.ahead)
					{
						item->Animation.TargetState = MPGUN_STATE_WAIT;
					}
					else
					{
						item->Animation.TargetState = MPGUN_STATE_WALK;
					}
				}
				else
				{
					item->Animation.TargetState = MPGUN_STATE_RUN;
				}

				break;

			case MPGUN_STATE_WALK:
				creature->MaxTurn = ANGLE(6.0f);
				head = laraAI.angle;

				if (item->AIBits & PATROL1)
				{
					item->Animation.TargetState = MPGUN_STATE_WALK;
					head = 0;
				}
				else if (cover && (Lara.TargetEntity == item || item->HitStatus))
				{
					item->Animation.RequiredState = MPGUN_STATE_CROUCH;
					item->Animation.TargetState = MPGUN_STATE_WAIT;
				}
				else if (creature->Mood == MoodType::Escape)
				{
					item->Animation.TargetState = MPGUN_STATE_RUN;
				}
				else if (Targetable(item, &AI))
				{
					if (AI.distance > pow(CLICK(1.5f), 2) && AI.zoneNumber == AI.enemyZone)
					{
						item->Animation.TargetState = MPGUN_STATE_AIM_4;
					}
					else
					{
						item->Animation.TargetState = MPGUN_STATE_WAIT;
					}
				}
				else if (creature->Mood == MoodType::Bored)
				{
					if (AI.ahead)
					{
						item->Animation.TargetState = MPGUN_STATE_WALK;
					}
					else
					{
						item->Animation.TargetState = MPGUN_STATE_WAIT;
					}
				}
				else
				{
					item->Animation.TargetState = MPGUN_STATE_RUN;
				}

				break;

			case MPGUN_STATE_RUN:
				creature->MaxTurn = ANGLE(10.0f);
				tilt = angle / 2;

				if (AI.ahead)
					head = AI.angle;

				if (item->AIBits & GUARD)
				{
					item->Animation.TargetState = MPGUN_STATE_WAIT;
				}
				else if (cover && (Lara.TargetEntity == item || item->HitStatus))
				{
					item->Animation.RequiredState = MPGUN_STATE_CROUCH;
					item->Animation.TargetState = MPGUN_STATE_WAIT;
				}
				else if (creature->Mood == MoodType::Escape)
				{
					break;
				}
				else if (Targetable(item, &AI) ||
					(item->AIBits & FOLLOW && (creature->ReachedGoal || laraAI.distance > pow(BLOCK(2), 2))))
				{
					item->Animation.TargetState = MPGUN_STATE_WAIT;
				}
				else if (creature->Mood == MoodType::Bored)
				{
					item->Animation.TargetState = MPGUN_STATE_WALK;
				}

				break;

			case MPGUN_STATE_AIM_1:
				if (AI.ahead)
				{
					extraTorsoRot.x = AI.xAngle;
					extraTorsoRot.y = AI.angle;
				}

				if (item->Animation.AnimNumber == 12 ||
					(item->Animation.AnimNumber == 1 &&
						item->Animation.FrameNumber == 10))
				{
					if (!ShotLara(item, &AI, MPGunBite, extraTorsoRot.y, 32))
						item->Animation.RequiredState = MPGUN_STATE_WAIT;
				}
				else if (item->HitStatus && Random::TestProbability(0.25f) && cover)
				{
					item->Animation.RequiredState = MPGUN_STATE_CROUCH;
					item->Animation.TargetState = MPGUN_STATE_WAIT;
				}

				break;

			case MPGUN_STATE_SHOOT_1:
				if (AI.ahead)
				{
					extraTorsoRot.x = AI.xAngle;
					extraTorsoRot.y = AI.angle;
				}

				if (item->Animation.FrameNumber == 0)
				{
					creature->MuzzleFlash[0].Bite = MPGunBite;
					creature->MuzzleFlash[0].Delay = 1;
				}

				if (item->Animation.RequiredState == MPGUN_STATE_WAIT)
					item->Animation.TargetState = MPGUN_STATE_WAIT;

				break;

			case MPGUN_STATE_SHOOT_2:
				if (AI.ahead)
				{
					extraTorsoRot.x = AI.xAngle;
					extraTorsoRot.y = AI.angle;
				}

				if (item->Animation.FrameNumber == 0)
				{
					if (!ShotLara(item, &AI, MPGunBite, extraTorsoRot.y, 32))
						item->Animation.TargetState = MPGUN_STATE_WAIT;

					creature->MuzzleFlash[0].Bite = MPGunBite;
					creature->MuzzleFlash[0].Delay = 2;
				}
				else if (item->HitStatus && Random::TestProbability(0.25f) && cover)
				{
					item->Animation.RequiredState = MPGUN_STATE_CROUCH;
					item->Animation.TargetState = MPGUN_STATE_WAIT;
				}

				break;

			case MPGUN_STATE_SHOOT_3A:
			case MPGUN_STATE_SHOOT_3B:
				if (AI.ahead)
				{
					extraTorsoRot.x = AI.xAngle;
					extraTorsoRot.y = AI.angle;
				}

				if (item->Animation.FrameNumber == 0 ||
					item->Animation.FrameNumber == 11)
				{
					if (!ShotLara(item, &AI, MPGunBite, extraTorsoRot.y, 32))
						item->Animation.TargetState = MPGUN_STATE_WAIT;

					creature->MuzzleFlash[0].Bite = MPGunBite;
					creature->MuzzleFlash[0].Delay = 2;
				}
				else if (item->HitStatus && Random::TestProbability(0.25f) && cover)
				{
					item->Animation.RequiredState = MPGUN_STATE_CROUCH;
					item->Animation.TargetState = MPGUN_STATE_WAIT;
				}

				break;

			case MPGUN_STATE_AIM_4:
				if (AI.ahead)
				{
					extraTorsoRot.x = AI.xAngle;
					extraTorsoRot.y = AI.angle;
				}

				if ((item->Animation.AnimNumber == 18 &&
						item->Animation.FrameNumber == 17) ||
					(item->Animation.AnimNumber == 19 &&
						item->Animation.FrameNumber == 6))
				{
					if (!ShotLara(item, &AI, MPGunBite, extraTorsoRot.y, 32))
						item->Animation.RequiredState = MPGUN_STATE_WALK;

					creature->MuzzleFlash[0].Bite = MPGunBite;
					creature->MuzzleFlash[0].Delay = 2;
				}
				else if (item->HitStatus && Random::TestProbability(0.25f) && cover)
				{
					item->Animation.RequiredState = MPGUN_STATE_CROUCH;
					item->Animation.TargetState = MPGUN_STATE_WAIT;
				}

				if (AI.distance < pow(BLOCK(1.5f), 2))
					item->Animation.RequiredState = MPGUN_STATE_WALK;

				break;

			case MPGUN_STATE_SHOOT_4A:
			case MPGUN_STATE_SHOOT_4B:
				if (AI.ahead)
				{
					extraTorsoRot.x = AI.xAngle;
					extraTorsoRot.y = AI.angle;
				}

				if (item->Animation.RequiredState == MPGUN_STATE_WALK)
					item->Animation.TargetState = MPGUN_STATE_WALK;

				if (item->Animation.FrameNumber == 16)
				{
					if (!ShotLara(item, &AI, MPGunBite, extraTorsoRot.y, 32))
						item->Animation.TargetState = MPGUN_STATE_WALK;

					creature->MuzzleFlash[0].Bite = MPGunBite;
					creature->MuzzleFlash[0].Delay = 2;
				}

				if (AI.distance < pow(BLOCK(1.5f), 2))
					item->Animation.TargetState = MPGUN_STATE_WALK;

				break;

			case MPGUN_STATE_CROUCHED:
				creature->MaxTurn = 0;

				if (AI.ahead)
					head = AI.angle;

				if (Targetable(item, &AI))
				{
					item->Animation.TargetState = MPGUN_STATE_CROUCH_AIM;
				}
				else if (item->HitStatus || !cover || (AI.ahead && Random::TestProbability(1 / 30.0f)))
				{
					item->Animation.TargetState = MPGUN_STATE_STAND;
				}
				else
				{
					item->Animation.TargetState = MPGUN_STATE_CROUCH_WALK;
				}

				break;

			case MPGUN_STATE_CROUCH_AIM:
				creature->MaxTurn = ANGLE(1.0f);

				if (AI.ahead)
					extraTorsoRot.y = AI.angle;

				if (Targetable(item, &AI))
				{
					item->Animation.TargetState = MPGUN_STATE_CROUCH_SHOT;
				}
				else
				{
					item->Animation.TargetState = MPGUN_STATE_CROUCHED;
				}

				break;

			case MPGUN_STATE_CROUCH_SHOT:
				if (AI.ahead)
					extraTorsoRot.y = AI.angle;

				if (item->Animation.FrameNumber == 0)
				{
					if (!ShotLara(item, &AI, MPGunBite, extraTorsoRot.y, 32) || Random::TestProbability(1 / 8.0f))
						item->Animation.TargetState = MPGUN_STATE_CROUCHED;

					creature->MuzzleFlash[0].Bite = MPGunBite;
					creature->MuzzleFlash[0].Delay = 2;
				}

				break;

			case MPGUN_STATE_CROUCH_WALK:
				creature->MaxTurn = ANGLE(6.0f);

				if (AI.ahead)
					head = AI.angle;

				if (Targetable(item, &AI) || item->HitStatus || !cover || (AI.ahead && Random::TestProbability(1 / 30.0f)))
					item->Animation.TargetState = MPGUN_STATE_CROUCHED;

				break;
			}
		}

		CreatureTilt(item, tilt);
		CreatureJoint(item, 0, extraTorsoRot.y);
		CreatureJoint(item, 1, extraTorsoRot.x);
		CreatureJoint(item, 2, head);
		CreatureAnimation(itemNumber, angle, tilt);
	}
}
