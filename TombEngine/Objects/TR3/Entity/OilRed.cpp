#include "framework.h"
#include "Objects/TR3/Entity/OilRed.h"

#include "Game/Animation/Animation.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/Point.h"
#include "Game/control/box.h"
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

using namespace TEN::Collision::Point;
using namespace TEN::Math;

namespace TEN::Entities::Creatures::TR3
{
	constexpr auto OILRED_SHOT_DAMAGE = 35;

	constexpr auto OILRED_WALK_RANGE				= SQUARE(BLOCK(2));
	constexpr auto OILRED_AWARE_DISTANCE			= SQUARE(BLOCK(1));
	constexpr auto OILRED_FEELER_DISTANCE			= BLOCK(1);

	constexpr auto OILRED_WALK_TURN_RATE_MAX		= ANGLE(6.0f);
	constexpr auto OILRED_RUN_TURN_RATE_MAX			= ANGLE(10.0f);

	constexpr auto OILRED_ONE_HAND_SHOOT_ANIM		= 1;
	constexpr auto OILRED_ONE_HAND_AIM_ANIM			= 12;
	constexpr auto OILRED_DIE_ANIM					= 14;
	constexpr auto OILRED_WALK_TO_STOP_ANIM			= 17;
	constexpr auto OILRED_WALK_SHOOT_RIGHT_ANIM		= 18;
	constexpr auto OILRED_WALK_SHOOT_LEFT_ANIM		= 19;
	constexpr auto OILRED_RUN_STOP_LEFT_ANIM		= 27;
	constexpr auto OILRED_RUN_STOP_RIGHT_ANIM		= 28;
	

	constexpr auto OILRED_DEATH_SHOT_ANGLE = ANGLE(45.0f);

	const auto OilRedGunBite = CreatureBiteInfo(Vector3(0, 160, 40), 13);

	enum OilRedState
	{
		OILRED_STATE_EMPTY = 0,
		OILRED_STATE_WAIT = 1,
		OILRED_STATE_WALK = 2,
		OILRED_STATE_RUN = 3,
		OILRED_STATE_AIM1 = 4,
		OILRED_STATE_SHOOT1 = 5,
		OILRED_STATE_AIM2 = 6,
		OILRED_STATE_SHOOT2 = 7,
		OILRED_STATE_SHOOT3A = 8,
		OILRED_STATE_SHOOT3B = 9,
		OILRED_STATE_SHOOT4A = 10,
		OILRED_STATE_AIM3 = 11,
		OILRED_STATE_AIM4 = 12,
		OILRED_STATE_DEATH = 13,
		OILRED_STATE_SHOOT4B = 14,
		OILRED_STATE_DUCK = 15,
		OILRED_STATE_DUCKED = 16,
		OILRED_STATE_DUCKAIM = 17,
		OILRED_STATE_DUCKSHOT = 18,
		OILRED_STATE_DUCKWALK = 19,
		OILRED_STATE_STAND = 20
	};

	void ControlOilRed(short itemNumber)
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

		if (item->HitPoints <= 0)
		{
			item->HitPoints = 0;
			if (item->Animation.ActiveState != OILRED_STATE_DEATH)
			{
				SetAnimation(*item, OILRED_DIE_ANIM);
			}
			else if (item->Animation.FrameNumber == 47)
			{
				AI_INFO ai;
				CreatureAIInfo(item, &ai);

				if (Targetable(item, &ai))
				{
					if (ai.angle > -OILRED_DEATH_SHOT_ANGLE && ai.angle < OILRED_DEATH_SHOT_ANGLE)
					{
						extraTorsoRot.y = ai.angle;
						head = ai.angle;
						ShotLara(item, &ai, OilRedGunBite, extraTorsoRot.y, OILRED_SHOT_DAMAGE * 3);
						creature->MuzzleFlash[0].Bite = OilRedGunBite;
						creature->MuzzleFlash[0].Delay = 2;
						SoundEffect(SFX_TR3_OIL_SMG_FIRE, &item->Pose, SoundEnvironment::Land, 1.0f, 0.5f);
					}
				}
			}
		}
		else
		{
			if (item->AIBits)
				GetAITarget(creature);
			else
				creature->Enemy = LaraItem;

			AI_INFO ai;
			CreatureAIInfo(item, &ai);

			AI_INFO laraAI;
			if (creature->Enemy == LaraItem)
			{
				laraAI.angle = ai.angle;
				laraAI.distance = ai.distance;
			}
			else
			{
				int dx = LaraItem->Pose.Position.x - item->Pose.Position.x;
				int dz = LaraItem->Pose.Position.z - item->Pose.Position.z;
				laraAI.angle = phd_atan(dz, dx) - item->Pose.Orientation.y;
				laraAI.distance = SQUARE(dx) + SQUARE(dz);
			}

			bool metaMood = (creature->Enemy != LaraItem);
			GetCreatureMood(item, &ai, metaMood);
			CreatureMood(item, &ai, metaMood);

			angle = CreatureTurn(item, creature->MaxTurn);

			// Calculate near cover
			int x = item->Pose.Position.x + OILRED_FEELER_DISTANCE * phd_sin(item->Pose.Orientation.y + laraAI.angle);
			int y = item->Pose.Position.y;
			int z = item->Pose.Position.z + OILRED_FEELER_DISTANCE * phd_cos(item->Pose.Orientation.y + laraAI.angle);
			int height = GetPointCollision(Vector3i(x, y, z), item->RoomNumber).GetFloorHeight();
			bool nearCover = (item->Pose.Position.y > (height + CLICK(1.5f)) &&
							  item->Pose.Position.y < (height + CLICK(4.5f)) &&
							  laraAI.distance > OILRED_AWARE_DISTANCE);

			auto* realEnemy = creature->Enemy;
			creature->Enemy = LaraItem;

			if ((laraAI.distance < OILRED_AWARE_DISTANCE || item->HitStatus || TargetVisible(item, &laraAI)) &&
				!(item->AIBits & FOLLOW))
			{
				if (!creature->Alerted)
					SoundEffect(SFX_TR3_AMERCAN_HOY, &item->Pose);

				AlertAllGuards(itemNumber);
			}

			creature->Enemy = realEnemy;

			switch (item->Animation.ActiveState)
			{
			case OILRED_STATE_WAIT:
				head = laraAI.angle;
				creature->MaxTurn = 0;

				if (item->Animation.AnimNumber == OILRED_WALK_TO_STOP_ANIM ||
					item->Animation.AnimNumber == OILRED_RUN_STOP_LEFT_ANIM ||
					item->Animation.AnimNumber == OILRED_RUN_STOP_RIGHT_ANIM)
				{
					if (abs(ai.angle) < OILRED_RUN_TURN_RATE_MAX)
						item->Pose.Orientation.y += ai.angle;
					else if (ai.angle < 0)
						item->Pose.Orientation.y -= OILRED_RUN_TURN_RATE_MAX;
					else
						item->Pose.Orientation.y += OILRED_RUN_TURN_RATE_MAX;
				}

				if (item->AIBits & GUARD)
				{
					head = AIGuard(creature);
					item->Animation.TargetState = OILRED_STATE_WAIT;
				}
				else if (item->AIBits & PATROL1)
				{
					item->Animation.TargetState = OILRED_STATE_WALK;
				}
				else if (nearCover && (Lara.TargetEntity == item || item->HitStatus))
				{
					item->Animation.TargetState = OILRED_STATE_DUCK;
				}
				else if (item->Animation.RequiredState == OILRED_STATE_DUCK)
				{
					item->Animation.TargetState = OILRED_STATE_DUCK;
				}
				else if (creature->Mood == MoodType::Escape)
				{
					item->Animation.TargetState = OILRED_STATE_RUN;
				}
				else if (Targetable(item, &ai))
				{
					if (ai.distance > OILRED_WALK_RANGE)
					{
						item->Animation.TargetState = OILRED_STATE_WALK;
					}
					else
					{
						int random = GetRandomControl();
						if (random < 0x2000)
							item->Animation.TargetState = OILRED_STATE_SHOOT1;
						else if (random < 0x4000)
							item->Animation.TargetState = OILRED_STATE_SHOOT2;
						else
							item->Animation.TargetState = OILRED_STATE_AIM3;
					}
				}
				else if (creature->Mood == MoodType::Bored ||
					((item->AIBits & FOLLOW) && (creature->ReachedGoal || laraAI.distance > SQUARE(BLOCK(2)))))
				{
					if (ai.ahead)
						item->Animation.TargetState = OILRED_STATE_WAIT;
					else
						item->Animation.TargetState = OILRED_STATE_WALK;
				}
				else
				{
					item->Animation.TargetState = OILRED_STATE_RUN;
				}

				break;

			case OILRED_STATE_WALK:
				head = laraAI.angle;
				creature->MaxTurn = OILRED_WALK_TURN_RATE_MAX;

				if (item->AIBits & PATROL1)
				{
					item->Animation.TargetState = OILRED_STATE_WALK;
					head = 0;
				}
				else if (nearCover && (Lara.TargetEntity == item || item->HitStatus))
				{
					item->Animation.RequiredState = OILRED_STATE_DUCK;
					item->Animation.TargetState = OILRED_STATE_WAIT;
				}
				else if (creature->Mood == MoodType::Escape)
				{
					item->Animation.TargetState = OILRED_STATE_RUN;
				}
				else if (Targetable(item, &ai))
				{
					if (ai.distance > OILRED_WALK_RANGE && ai.zoneNumber == ai.enemyZone)
						item->Animation.TargetState = OILRED_STATE_AIM4;
					else
						item->Animation.TargetState = OILRED_STATE_WAIT;
				}
				else if (creature->Mood == MoodType::Bored)
				{
					if (ai.ahead)
						item->Animation.TargetState = OILRED_STATE_WALK;
					else
						item->Animation.TargetState = OILRED_STATE_WAIT;
				}
				else
				{
					item->Animation.TargetState = OILRED_STATE_RUN;
				}

				break;

			case OILRED_STATE_RUN:
				if (ai.ahead)
					head = ai.angle;

				creature->MaxTurn = OILRED_RUN_TURN_RATE_MAX;
				tilt = angle / 2;

				if (item->AIBits & GUARD)
				{
					item->Animation.TargetState = OILRED_STATE_WAIT;
				}
				else if (nearCover && (Lara.TargetEntity == item || item->HitStatus))
				{
					item->Animation.RequiredState = OILRED_STATE_DUCK;
					item->Animation.TargetState = OILRED_STATE_WAIT;
				}
				else if (creature->Mood == MoodType::Escape)
				{
					break;
				}
				else if (Targetable(item, &ai) ||
					((item->AIBits & FOLLOW) && (creature->ReachedGoal || laraAI.distance > SQUARE(BLOCK(2)))))
				{
					item->Animation.TargetState = OILRED_STATE_WAIT;
				}
				else if (creature->Mood == MoodType::Bored)
				{
					item->Animation.TargetState = OILRED_STATE_WALK;
				}

				break;

			case OILRED_STATE_AIM1:
				if (ai.ahead)
				{
					extraTorsoRot.y = ai.angle;
					extraTorsoRot.x = ai.xAngle;
				}

				if ((item->Animation.AnimNumber == OILRED_ONE_HAND_AIM_ANIM) ||
					(item->Animation.AnimNumber == OILRED_ONE_HAND_SHOOT_ANIM &&
					 item->Animation.FrameNumber == 10))
				{
					if (!ShotLara(item, &ai, OilRedGunBite, extraTorsoRot.y, OILRED_SHOT_DAMAGE))
						item->Animation.RequiredState = OILRED_STATE_WAIT;

					creature->MuzzleFlash[0].Bite = OilRedGunBite;
					creature->MuzzleFlash[0].Delay = 2;
				}
				else if (item->HitStatus && Random::TestProbability(0.25f) && nearCover)
				{
					item->Animation.RequiredState = OILRED_STATE_DUCK;
					item->Animation.TargetState = OILRED_STATE_WAIT;
				}

				break;

			case OILRED_STATE_SHOOT1:
				if (ai.ahead)
				{
					extraTorsoRot.y = ai.angle;
					extraTorsoRot.x = ai.xAngle;
				}

				if (item->Animation.FrameNumber == 0)
				{
					creature->MuzzleFlash[0].Bite = OilRedGunBite;
					creature->MuzzleFlash[0].Delay = 1;
				}

				if (item->Animation.RequiredState == OILRED_STATE_WAIT)
					item->Animation.TargetState = OILRED_STATE_WAIT;

				break;

			case OILRED_STATE_SHOOT2:
				if (ai.ahead)
				{
					extraTorsoRot.y = ai.angle;
					extraTorsoRot.x = ai.xAngle;
				}

				if (item->Animation.FrameNumber == 0)
				{
					if (!ShotLara(item, &ai, OilRedGunBite, extraTorsoRot.y, OILRED_SHOT_DAMAGE))
						item->Animation.TargetState = OILRED_STATE_WAIT;

					creature->MuzzleFlash[0].Bite = OilRedGunBite;
					creature->MuzzleFlash[0].Delay = 2;
				}
				else if (item->HitStatus && Random::TestProbability(0.25f) && nearCover)
				{
					item->Animation.RequiredState = OILRED_STATE_DUCK;
					item->Animation.TargetState = OILRED_STATE_WAIT;
				}

				break;

			case OILRED_STATE_SHOOT3A:
			case OILRED_STATE_SHOOT3B:
				if (ai.ahead)
				{
					extraTorsoRot.y = ai.angle;
					extraTorsoRot.x = ai.xAngle;
				}

				if (item->Animation.FrameNumber == 0 ||
					item->Animation.FrameNumber == 11)
				{
					if (!ShotLara(item, &ai, OilRedGunBite, extraTorsoRot.y, OILRED_SHOT_DAMAGE))
						item->Animation.TargetState = OILRED_STATE_WAIT;

					creature->MuzzleFlash[0].Bite = OilRedGunBite;
					creature->MuzzleFlash[0].Delay = 2;
				}
				else if (item->HitStatus && Random::TestProbability(0.25f) && nearCover)
				{
					item->Animation.RequiredState = OILRED_STATE_DUCK;
					item->Animation.TargetState = OILRED_STATE_WAIT;
				}

				break;

			case OILRED_STATE_AIM4:
				if (ai.ahead)
				{
					extraTorsoRot.y = ai.angle;
					extraTorsoRot.x = ai.xAngle;
				}

				if ((item->Animation.AnimNumber == OILRED_WALK_SHOOT_RIGHT_ANIM &&
					 item->Animation.FrameNumber == 17) ||
					(item->Animation.AnimNumber == OILRED_WALK_SHOOT_LEFT_ANIM &&
					 item->Animation.FrameNumber == 6))
				{
					if (!ShotLara(item, &ai, OilRedGunBite, extraTorsoRot.y, OILRED_SHOT_DAMAGE))
						item->Animation.RequiredState = OILRED_STATE_WALK;

					creature->MuzzleFlash[0].Bite = OilRedGunBite;
					creature->MuzzleFlash[0].Delay = 2;
				}
				else if (item->HitStatus && Random::TestProbability(0.25f) && nearCover)
				{
					item->Animation.RequiredState = OILRED_STATE_DUCK;
					item->Animation.TargetState = OILRED_STATE_WAIT;
				}

				if (ai.distance < OILRED_WALK_RANGE)
					item->Animation.RequiredState = OILRED_STATE_WALK;

				break;

			case OILRED_STATE_SHOOT4A:
			case OILRED_STATE_SHOOT4B:
				if (ai.ahead)
				{
					extraTorsoRot.y = ai.angle;
					extraTorsoRot.x = ai.xAngle;
				}

				if (item->Animation.RequiredState == OILRED_STATE_WALK)
					item->Animation.TargetState = OILRED_STATE_WALK;

				if (item->Animation.FrameNumber == 16)
				{
					if (!ShotLara(item, &ai, OilRedGunBite, extraTorsoRot.y, OILRED_SHOT_DAMAGE))
						item->Animation.TargetState = OILRED_STATE_WALK;

					creature->MuzzleFlash[0].Bite = OilRedGunBite;
					creature->MuzzleFlash[0].Delay = 2;
				}

				if (ai.distance < OILRED_WALK_RANGE)
					item->Animation.TargetState = OILRED_STATE_WALK;

				break;

			case OILRED_STATE_DUCKED:
				if (ai.ahead)
					head = ai.angle;

				creature->MaxTurn = 0;

				if (Targetable(item, &ai))
				{
					item->Animation.TargetState = OILRED_STATE_DUCKAIM;
				}
				else if (item->HitStatus || !nearCover || (ai.ahead && Random::TestProbability(1 / 32.0f)))
				{
					item->Animation.TargetState = OILRED_STATE_STAND;
				}
				else
				{
					item->Animation.TargetState = OILRED_STATE_DUCKWALK;
				}

				break;

			case OILRED_STATE_DUCKAIM:
				creature->MaxTurn = ANGLE(1.0f);

				if (ai.ahead)
					extraTorsoRot.y = ai.angle;

				if (Targetable(item, &ai))
					item->Animation.TargetState = OILRED_STATE_DUCKSHOT;
				else
					item->Animation.TargetState = OILRED_STATE_DUCKED;

				break;

			case OILRED_STATE_DUCKSHOT:
				if (ai.ahead)
					extraTorsoRot.y = ai.angle;

				if (item->Animation.FrameNumber == 0)
				{
					if (!ShotLara(item, &ai, OilRedGunBite, extraTorsoRot.y, OILRED_SHOT_DAMAGE) ||
						Random::TestProbability(1 / 8.0f))
					{
						item->Animation.TargetState = OILRED_STATE_DUCKED;
					}

					creature->MuzzleFlash[0].Bite = OilRedGunBite;
					creature->MuzzleFlash[0].Delay = 2;
				}

				break;

			case OILRED_STATE_DUCKWALK:
				if (ai.ahead)
					head = ai.angle;

				creature->MaxTurn = OILRED_WALK_TURN_RATE_MAX;

				if (Targetable(item, &ai) || item->HitStatus || !nearCover ||
					(ai.ahead && Random::TestProbability(1 / 32.0f)))
				{
					item->Animation.TargetState = OILRED_STATE_DUCKED;
				}

				break;

			case OILRED_STATE_STAND:
				if (abs(ai.angle) < OILRED_WALK_TURN_RATE_MAX)
					item->Pose.Orientation.y += ai.angle;
				else if (ai.angle < 0)
					item->Pose.Orientation.y -= OILRED_WALK_TURN_RATE_MAX;
				else
					item->Pose.Orientation.y += OILRED_WALK_TURN_RATE_MAX;

				break;
			}
		}

		CreatureTilt(item, tilt);
		CreatureJoint(item, 0, extraTorsoRot.y);
		CreatureJoint(item, 1, extraTorsoRot.x);
		CreatureJoint(item, 2, head);
		CreatureAnimation(itemNumber, angle, 0);
	}
}
