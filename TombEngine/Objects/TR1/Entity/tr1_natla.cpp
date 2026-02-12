#include "framework.h"
#include "Objects/TR1/Entity/tr1_natla.h"

#include "Game/control/box.h"
#include "Game/effects/effects.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/misc.h"
#include "Game/missile.h"
#include "Game/people.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Specific/clock.h"
#include "Specific/level.h"
#include "Sound/sound.h"

using namespace TEN::Math;

namespace TEN::Entities::Creatures::TR1
{
	// TODO: Organise.
	constexpr auto NATLA_SHOT_DAMAGE  = 100;
	constexpr auto NATLA_DEATH_TIME	  = 16 * FPS; // 16 seconds.
	constexpr auto NATLA_FLY_MODE	  = 0x8000;
	constexpr auto NATLA_TIMER		  = 0x7FFF;
	constexpr auto NATLA_GUN_VELOCITY = 400;

	constexpr auto NATLA_LAND_CHANCE = 1 / 128.0f;

	constexpr auto NATLA_TURN_NEAR_DEATH_SPEED = ANGLE(6.0f);
	constexpr auto NATLA_TURN_SPEED			   = ANGLE(5.0f);
	constexpr auto NATLA_FLY_ANGLE_SPEED	   = ANGLE(5.0f);
	constexpr auto NATLA_SHOOT_ANGLE		   = ANGLE(30.0f);

	const auto NatlaGunBite = CreatureBiteInfo(Vector3(5, 220, 7), 4);

	enum NatlaState
	{
		// No state 0.
		NATLA_STATE_IDLE = 1,
		NATLA_STATE_FLY,
		NATLA_STATE_RUN,
		NATLA_STATE_AIM,
		NATLA_STATE_SEMI_DEATH,
		NATLA_STATE_SHOOT,
		NATLA_STATE_FALL,
		NATLA_STATE_STAND,
		NATLA_STATE_DEATH,
		NATLA_STATE_SHOOT_2, // After first phase when cannot fly anymore.
	};

	void NatlaControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto* creature = GetCreatureInfo(item);

		short headingAngle = 0;
		short tiltAngle = 0;
		auto extraHeadRot = EulerAngles::Identity;
		auto extraTorsoRot = EulerAngles::Identity;

		AI_INFO ai;
		int timer = creature->Flags & NATLA_TIMER;
		bool shoot = false;

		if (item->HitPoints <= 0 && item->ItemFlags[1] != 0)
		{
			item->Animation.TargetState = NATLA_STATE_DEATH;
		}
		else if (item->HitPoints <= (Objects[item->ObjectNumber].HitPoints / 2))
		{
			creature->LOT.Step = CLICK(1);
			creature->LOT.Drop = -CLICK(1);
			creature->LOT.Fly = NO_FLYING;
			creature->LOT.Zone = ZoneType::Basic;
			CreatureAIInfo(item, &ai);

			// NOTE: Only rotate joints if alive.
			if (ai.ahead && item->ItemFlags[1] == 1)
			{
				extraHeadRot.x = ai.xAngle / 2;
				extraHeadRot.y = ai.angle / 2;
				extraTorsoRot.x = ai.xAngle / 2;
				extraTorsoRot.y = ai.angle / 2;
			}

			GetCreatureMood(item, &ai, true);
			CreatureMood(item, &ai, true);

			headingAngle = CreatureTurn(item, creature->MaxTurn);

			if (item->ItemFlags[0] != 0)
			{
				item->Pose.Orientation.y += item->ItemFlags[0];
				item->ItemFlags[0] = 0;
			}

			switch (item->Animation.ActiveState)
			{
			case NATLA_STATE_FALL:
				creature->MaxTurn = 0;

				if (item->Pose.Position.y < item->Floor)
				{
					item->Animation.IsAirborne = true;
					item->Animation.Velocity.z = 0;
				}
				else if (item->Pose.Position.y >= item->Floor)
				{
					item->Animation.TargetState = NATLA_STATE_SEMI_DEATH;
					item->Animation.IsAirborne = false;
					item->Pose.Position.y = item->Floor;
					timer = 0;
				}

				break;

			case NATLA_STATE_STAND:
				creature->MaxTurn = NATLA_TURN_NEAR_DEATH_SPEED;

				if (Targetable(item, &ai))
				{
					item->Animation.TargetState = NATLA_STATE_SHOOT_2;
				}
				else
				{
					item->Animation.TargetState = NATLA_STATE_RUN;
				}

				break;

			case NATLA_STATE_SHOOT_2:
				creature->MaxTurn = 0;

				if (timer >= 20)
				{
					timer = 0;
					CreatureEffect2(item, NatlaGunBite, NATLA_GUN_VELOCITY, ai.angle, ShardGun);
					SoundEffect(SFX_TR1_ATLANTEAN_NEEDLE, &item->Pose);
				}

				break;

			case NATLA_STATE_RUN:
				creature->MaxTurn = NATLA_TURN_NEAR_DEATH_SPEED;
				tiltAngle = headingAngle;

				if (timer >= 20 && Targetable(item, &ai))
				{
					CreatureEffect2(item, NatlaGunBite, NATLA_GUN_VELOCITY, ai.angle, ShardGun);
					SoundEffect(SFX_TR1_ATLANTEAN_NEEDLE, &item->Pose);
					timer = 0;
				}

				if (Targetable(item, &ai))
					item->Animation.TargetState = NATLA_STATE_STAND;

				break;

			case NATLA_STATE_SEMI_DEATH:
				creature->MaxTurn = 0;

				if (timer == NATLA_DEATH_TIME)
				{
					item->Animation.TargetState = NATLA_STATE_STAND;
					item->HitPoints = Objects[item->ObjectNumber].HitPoints / 2;
					item->ItemFlags[1] = 1;
					creature->Flags = 0;
					timer = 0;
				}
				else
				{
					item->HitPoints = NOT_TARGETABLE;
				}

				break;

			case NATLA_STATE_FLY:
				item->Animation.TargetState = NATLA_STATE_FALL;
				creature->MaxTurn = 0;
				timer = 0;
				break;

			case NATLA_STATE_IDLE:
			case NATLA_STATE_SHOOT:
			case NATLA_STATE_AIM:
				item->Animation.TargetState = NATLA_STATE_SEMI_DEATH;
				item->Flags = 0;
				creature->MaxTurn = 0;
				timer = 0;
				break;
			}
		}
		else
		{
			creature->LOT.Step = CLICK(1);
			creature->LOT.Drop = -CLICK(1);
			creature->LOT.Fly = NO_FLYING;
			creature->LOT.Zone = ZoneType::Basic;
			CreatureAIInfo(item, &ai);

			shoot = (ai.angle > -NATLA_SHOOT_ANGLE && ai.angle < NATLA_SHOOT_ANGLE) && Targetable(item, &ai);

			if (item->Animation.ActiveState == NATLA_STATE_FLY && (creature->Flags & NATLA_FLY_MODE))
			{
				if (shoot && Random::TestProbability(NATLA_LAND_CHANCE))
					creature->Flags &= ~NATLA_FLY_MODE;

				if (!(creature->Flags & NATLA_FLY_MODE))
				{
					GetCreatureMood(item, &ai, true);
					CreatureMood(item, &ai, true);
				}

				creature->LOT.Step = BLOCK(20);
				creature->LOT.Drop = -BLOCK(20);
				creature->LOT.Fly = 16;
				creature->LOT.Zone = ZoneType::Flyer;
				CreatureAIInfo(item, &ai);
			}
			else if (!shoot)
			{
				creature->Flags |= NATLA_FLY_MODE;
			}

			if (ai.ahead)
			{
				extraHeadRot.x = ai.xAngle / 2;
				extraHeadRot.y = ai.angle / 2;
				extraTorsoRot.x = ai.xAngle / 2;
				extraTorsoRot.y = ai.angle / 2;
			}

			if (item->Animation.ActiveState != NATLA_STATE_FLY || (creature->Flags & NATLA_FLY_MODE))
			{
				GetCreatureMood(item, &ai, false);
				CreatureMood(item, &ai, false);
			}

			item->Pose.Orientation.y -= item->ItemFlags[0];
			headingAngle = CreatureTurn(item, creature->MaxTurn);

			if (item->Animation.ActiveState == NATLA_STATE_FLY)
			{
				if (ai.angle > NATLA_FLY_ANGLE_SPEED)
				{
					item->ItemFlags[0] += NATLA_FLY_ANGLE_SPEED;
				}
				else if (ai.angle < -NATLA_FLY_ANGLE_SPEED)
				{
					item->ItemFlags[0] -= NATLA_FLY_ANGLE_SPEED;
				}
				else
				{
					item->ItemFlags[0] += ai.angle;
				}

				item->Pose.Orientation.y += item->ItemFlags[0];
			}
			else
			{
				item->Pose.Orientation.y += item->ItemFlags[0] - headingAngle;
				item->ItemFlags[0] = 0;
			}

			switch (item->Animation.ActiveState)
			{
			case NATLA_STATE_IDLE:
				creature->MaxTurn = NATLA_TURN_SPEED;
				timer = 0;

				if (creature->Flags & NATLA_FLY_MODE)
				{
					item->Animation.TargetState = NATLA_STATE_FLY;
				}
				else
				{
					item->Animation.TargetState = NATLA_STATE_AIM;
				}

				break;

			case NATLA_STATE_FLY:
				creature->MaxTurn = NATLA_FLY_ANGLE_SPEED;

				if (!(creature->Flags & NATLA_FLY_MODE) && item->Pose.Position.y == item->Floor)
					item->Animation.TargetState = NATLA_STATE_IDLE;

				if (timer >= 30 && ai.ahead)
				{
					timer = 0;
					CreatureEffect2(item, NatlaGunBite, NATLA_GUN_VELOCITY, ai.angle, BombGun);
					SoundEffect(SFX_TR1_ATLANTEAN_BALL, &item->Pose);
				}

				break;

			case NATLA_STATE_AIM:
				creature->MaxTurn = NATLA_FLY_ANGLE_SPEED;

				if (item->Animation.RequiredState != NO_VALUE)
				{
					item->Animation.TargetState = item->Animation.RequiredState;
				}
				else if (shoot)
				{
					item->Animation.TargetState = NATLA_STATE_SHOOT;
				}
				else
				{
					item->Animation.TargetState = NATLA_STATE_IDLE;
				}

				break;

			case NATLA_STATE_SHOOT:
				creature->MaxTurn = NATLA_FLY_ANGLE_SPEED;

				if (item->Animation.RequiredState == NO_VALUE && ai.ahead)
				{
					CreatureEffect2(item, NatlaGunBite, NATLA_GUN_VELOCITY, ai.angle, BombGun);
					CreatureEffect2(item, NatlaGunBite, NATLA_GUN_VELOCITY, ai.angle + (GetRandomControl() - ANGLE(45.0f)) / 4, BombGun);
					CreatureEffect2(item, NatlaGunBite, NATLA_GUN_VELOCITY, ai.angle + (GetRandomControl() - ANGLE(45.0f)) / 4, BombGun);
					SoundEffect(SFX_TR1_ATLANTEAN_BALL, &item->Pose);

					item->Animation.RequiredState = NATLA_STATE_IDLE;
				}

				break;
			}
		}

		CreatureTilt(item, tiltAngle);
		CreatureJoint(item, 0, extraTorsoRot.y);
		CreatureJoint(item, 1, extraTorsoRot.x);
		CreatureJoint(item, 2, extraHeadRot.y);
		CreatureJoint(item, 3, extraHeadRot.x);

		timer++;
		creature->Flags &= ~NATLA_TIMER;
		creature->Flags |= timer & NATLA_TIMER;

		if (item->ItemFlags[1] == 0)
			item->Pose.Orientation.y -= item->ItemFlags[0];

		CreatureAnimation(itemNumber, headingAngle, 0);

		if (item->ItemFlags[1] == 0)
			item->Pose.Orientation.y += item->ItemFlags[0];
	}
}
