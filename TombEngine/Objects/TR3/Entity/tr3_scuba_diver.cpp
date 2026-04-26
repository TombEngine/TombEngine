#include "framework.h"
#include "Objects/TR3/Entity/tr3_scuba_diver.h"

#include "Game/collision/collide_room.h"
#include "Game/collision/Point.h"
#include "Game/control/box.h"
#include "Game/control/control.h"
#include "Game/control/los.h"
#include "Game/effects/effects.h"
#include "Game/effects/Ripple.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/misc.h"
#include "Game/missile.h"
#include "Game/Setup.h"
#include "Specific/level.h"

using namespace TEN::Collision::Point;
using namespace TEN::Effects::Ripple;

namespace TEN::Entities::Creatures::TR3
{
	constexpr auto SCUBA_DIVER_ATTACK_DAMAGE = 50;
	constexpr auto SCUBA_DIVER_SWIM_TURN_RATE_MAX = ANGLE(3.0f);
	constexpr auto SCUBA_DIVER_HARPOON_VELOCITY = 150.0f;
	constexpr auto SCUBA_DIVER_VISIBILITY_DISTANCE = BLOCK(5);

	const auto ScubaGunBite = CreatureBiteInfo(Vector3(17, 164, 44), 18);

	enum ScubaDiverState
	{
		// No state 0.
		SDIVER_STATE_SWIM = 1,
		SDIVER_STATE_TREAD_WATER_IDLE = 2,
		SDIVER_STATE_SWIM_SHOOT = 3,
		SDIVER_STATE_SWIM_AIM = 4,
		// No state 5.
		SDIVER_STATE_TREAD_WATER_AIM = 6,
		SDIVER_STATE_TREAD_WATER_SHOOT = 7,
		// No state 8.
		SDIVER_STATE_DEATH = 9
	};

	enum ScubaDiverAnim
	{
		SDIVER_ANIM_SWIM = 0,
		SDIVER_ANIM_TREAD_WATER_IDLE = 1,
		SDIVER_ANIM_RESURFACE = 2,
		SDIVER_ANIM_SWIM_AIM_CONTINUE = 3,
		SDIVER_ANIM_SWIM_AIM_END = 4,
		SDIVER_ANIM_SWIM_SHOOT_LEFT = 5,
		SDIVER_ANIM_SWIM_SHOOT_RIGHT = 6,
		SDIVER_ANIM_SWIM_AIM_START_LEFT = 7,
		SDIVER_ANIM_SWIM_AIM_START_RIGHT = 8,
		SDIVER_ANIM_TREAD_WATER_AIM_CONTINUE = 9,
		SDIVER_ANIM_TREAD_WATER_AIM_END = 10,
		SDIVER_ANIM_TREAD_WATER_SHOOT_LEFT = 11,
		SDIVER_ANIM_TREAD_WATER_SHOOT_RIGHT = 12,
		SDIVER_STATE_TREAD_WATER_AIM_START_LEFT = 13,
		SDIVER_STATE_TREAD_WATER_AIM_START_RIGHT = 14,
		SDIVER_STATE_DIVE = 15,
		SDIVER_ANIM_DEATH_START = 16,
		SDIVER_ANIM_DEATH_END = 17
	};

	void ScubaControl(short itemNumber)
	{
		if (!CreatureActive(itemNumber))
			return;

		auto* item = &g_Level.Items[itemNumber];
		auto* creature = GetCreatureInfo(item);

		short angle = 0;
		short head = 0;
		short neck = 0;

		int waterHeight = 0;

		if (item->HitPoints <= 0)
		{
			if (item->Animation.ActiveState != SDIVER_STATE_DEATH)
				SetAnimation(item, SDIVER_ANIM_DEATH_START);

			CreatureFloat(itemNumber);
			return;
		}
		else
		{
			AI_INFO ai;
			CreatureAIInfo(item, &ai);

			GetCreatureMood(item, &ai, false);
			CreatureMood(item, &ai, false);

			bool shoot = false;
			bool outOfReach = false;

			if (Lara.Control.WaterStatus == WaterStatus::Dry)
			{
				// Lara is on dry land; diver may surface to target her.
				auto origin = GameVector(
					item->Pose.Position.x,
					item->Pose.Position.y - CLICK(1),
					item->Pose.Position.z,
					item->RoomNumber);
				auto target = GameVector(
					LaraItem->Pose.Position.x,
					LaraItem->Pose.Position.y - (LARA_HEIGHT - LARA_HEADROOM),
					LaraItem->Pose.Position.z);

				outOfReach = Vector3i::Distance(origin.ToVector3i(), target.ToVector3i()) >= SCUBA_DIVER_VISIBILITY_DISTANCE;
				shoot = LOS(&origin, &target) && !outOfReach;

				// Cancel shoot if not facing towards Lara.
				if (ai.angle < -ANGLE(45.0f) || ai.angle > ANGLE(45.0f))
					shoot = false;
			}
			else if (ai.angle > -ANGLE(45.0f) && ai.angle < ANGLE(45.0f))
			{
				// Lara is in water; normal targeting.
				auto origin = GameVector(item->Pose.Position, item->RoomNumber);
				auto target = GameVector(LaraItem->Pose.Position);

				outOfReach = Vector3i::Distance(origin.ToVector3i(), target.ToVector3i()) >= SCUBA_DIVER_VISIBILITY_DISTANCE;
				shoot = LOS(&origin, &target) && !outOfReach;
			}

			// Only set target to Lara if diver can see her.
			if (shoot)
				creature->Target = PredictTargetPosition(*item, *creature->Enemy);

			angle = CreatureTurn(item, creature->MaxTurn);
			waterHeight = GetPointCollision(*item).GetWaterSurfaceHeight() + BLOCK(0.5f);

			switch (item->Animation.ActiveState)
			{
			case SDIVER_STATE_SWIM:
				creature->MaxTurn = SCUBA_DIVER_SWIM_TURN_RATE_MAX;

				if (shoot)
					neck = -ai.angle;

				if (creature->Target.y < waterHeight && item->Pose.Position.y < (waterHeight + creature->LOT.Fly) && !outOfReach)
				{
					item->Animation.TargetState = SDIVER_STATE_TREAD_WATER_IDLE;
				}
				else if (creature->Mood != MoodType::Escape && shoot)
				{
					item->Animation.TargetState = SDIVER_STATE_SWIM_AIM;
				}

				break;

			case SDIVER_STATE_SWIM_AIM:
				creature->Flags = 0;

				if (shoot)
					neck = -ai.angle;

				if (!shoot || creature->Mood == MoodType::Escape ||
					(creature->Target.y < waterHeight && item->Pose.Position.y < (waterHeight + creature->LOT.Fly)))
				{
					item->Animation.TargetState = SDIVER_STATE_SWIM;
				}
				else
					item->Animation.TargetState = SDIVER_STATE_SWIM_SHOOT;

				break;

			case SDIVER_STATE_SWIM_SHOOT:
			case SDIVER_STATE_TREAD_WATER_SHOOT:
				if (shoot)
				{
					if (item->Animation.ActiveState == SDIVER_STATE_SWIM_SHOOT)
						neck = -ai.angle;
					else
						head = ai.angle;
				}

				if (!creature->Flags)
				{
					ShootAtEnemy(creature->Target, creature->Enemy, CreatureEffect2(item, ScubaGunBite, SCUBA_DIVER_HARPOON_VELOCITY, head, HarpoonGun));
					creature->Flags = 1;
				}

				break;

			case SDIVER_STATE_TREAD_WATER_IDLE:
				creature->MaxTurn = SCUBA_DIVER_SWIM_TURN_RATE_MAX;

				if (shoot)
					head = ai.angle;

				if (creature->Target.y > waterHeight || outOfReach)
					item->Animation.TargetState = SDIVER_STATE_SWIM;
				else if (creature->Mood != MoodType::Escape && shoot)
					item->Animation.TargetState = SDIVER_STATE_TREAD_WATER_AIM;

				break;

			case SDIVER_STATE_TREAD_WATER_AIM:
				creature->Flags = 0;

				if (shoot)
					head = ai.angle;

				if (!shoot || creature->Mood == MoodType::Escape || creature->Target.y > waterHeight)
					item->Animation.TargetState = SDIVER_STATE_TREAD_WATER_IDLE;
				else
					item->Animation.TargetState = SDIVER_STATE_TREAD_WATER_SHOOT;

				break;
			}
		}

		CreatureJoint(item, 0, head);
		CreatureJoint(item, 1, neck);
		CreatureAnimation(itemNumber, angle, 0);

		switch (item->Animation.ActiveState)
		{
		case SDIVER_STATE_SWIM:
		case SDIVER_STATE_SWIM_AIM:
		case SDIVER_STATE_SWIM_SHOOT:
			CreatureUnderwater(item, CLICK(2));
			break;

		default:
			item->Pose.Position.y = waterHeight - CLICK(2);
			if (!(Wibble & 0xF))
				SpawnRipple(item->Pose.Position.ToVector3(), item->RoomNumber, Random::GenerateFloat(192.0f, 256.0f), (int)RippleFlags::SlowFade | (int)RippleFlags::LowOpacity);
			break;

		}
	}
}
