#include "framework.h"
#include "Objects/TR3/Entity/tr3_scuba_diver.h"

#include "Game/collision/collide_room.h"
#include "Game/collision/Point.h"
#include "Game/control/box.h"
#include "Game/control/control.h"
#include "Game/control/los.h"
#include "Game/effects/effects.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/misc.h"
#include "Game/Setup.h"
#include "Specific/level.h"

using namespace TEN::Collision::Point;

namespace TEN::Entities::Creatures::TR3
{
	constexpr auto SCUBA_DIVER_ATTACK_DAMAGE = 50;
	constexpr auto SCUBA_DIVER_SWIM_TURN_RATE_MAX = ANGLE(3.0f);

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

	static void ShootHarpoon(ItemInfo* item, Vector3i pos, short velocity, short yRot, short roomNumber)
	{
		short harpoonItemNumber = CreateItem();
		if (harpoonItemNumber == NO_VALUE)
			return;

		auto* harpoonItem = &g_Level.Items[harpoonItemNumber];

		harpoonItem->ObjectNumber = ID_SCUBA_HARPOON;
		harpoonItem->RoomNumber = item->RoomNumber;
		harpoonItem->Pose.Position = pos;

		InitializeItem(harpoonItemNumber);

		harpoonItem->Animation.Velocity.z = 150.0f;
		harpoonItem->Pose.Orientation.x = 0;
		harpoonItem->Pose.Orientation.y = yRot;

		AddActiveItem(harpoonItemNumber);
		harpoonItem->Status = ITEM_ACTIVE;
	}

	void ScubaHarpoonControl(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		if (item->TouchBits.TestAny())
		{
			DoDamage(LaraItem, SCUBA_DIVER_ATTACK_DAMAGE);
			DoBloodSplat(item->Pose.Position.x, item->Pose.Position.y, item->Pose.Position.z, (GetRandomControl() & 3) + 4, LaraItem->Pose.Orientation.y, LaraItem->RoomNumber);
			KillItem(itemNumber);
		}
		else
		{
			item->Pose.Translate(item->Pose.Orientation, item->Animation.Velocity.z);

			auto probe = GetPointCollision(*item);

			if (item->RoomNumber != probe.GetRoomNumber())
				ItemNewRoom(itemNumber, probe.GetRoomNumber());

			item->Floor = GetPointCollision(*item).GetFloorHeight();
			if (item->Pose.Position.y >= item->Floor)
				KillItem(itemNumber);
		}
	}

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
				SetAnimation(*item, SDIVER_ANIM_DEATH_START);

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
			if (Lara.Control.WaterStatus == WaterStatus::Dry)
			{
				auto origin = GameVector(
					item->Pose.Position.x,
					item->Pose.Position.y - CLICK(1),
					item->Pose.Position.z,
					item->RoomNumber);
				auto target = GameVector(
					LaraItem->Pose.Position.x,
					LaraItem->Pose.Position.y - (LARA_HEIGHT - 150),
					LaraItem->Pose.Position.z);

				shoot = LOS(&origin, &target);

				if (shoot)
					creature->Target = LaraItem->Pose.Position;

				if (ai.angle < -ANGLE(45.0f) || ai.angle > ANGLE(45.0f))
					shoot = false;
			}
			else if (ai.angle > -ANGLE(45.0f) && ai.angle < ANGLE(45.0f))
			{
				auto origin = GameVector(item->Pose.Position, item->RoomNumber);
				auto target = GameVector(LaraItem->Pose.Position);
				
				shoot = LOS(&origin, &target);
			}

			angle = CreatureTurn(item, creature->MaxTurn);
			waterHeight = GetPointCollision(*item).GetWaterSurfaceHeight() + BLOCK(0.5f);

			switch (item->Animation.ActiveState)
			{
			case SDIVER_STATE_SWIM:
				creature->MaxTurn = SCUBA_DIVER_SWIM_TURN_RATE_MAX;

				if (shoot)
					neck = -ai.angle;

				if (creature->Target.y < waterHeight &&
					item->Pose.Position.y < (waterHeight + creature->LOT.Fly))
				{
					item->Animation.TargetState = SDIVER_STATE_TREAD_WATER_IDLE;
				}
				else if (creature->Mood == MoodType::Escape)
					break;
				else if (shoot)
					item->Animation.TargetState = SDIVER_STATE_SWIM_AIM;

				break;

			case SDIVER_STATE_SWIM_AIM:
				creature->Flags = 0;

				if (shoot)
					neck = -ai.angle;

				if (!shoot || creature->Mood == MoodType::Escape ||
					(creature->Target.y < waterHeight &&
						item->Pose.Position.y < (waterHeight + creature->LOT.Fly)))
				{
					item->Animation.TargetState = SDIVER_STATE_SWIM;
				}
				else
					item->Animation.TargetState = SDIVER_STATE_SWIM_SHOOT;

				break;

			case SDIVER_STATE_SWIM_SHOOT:
				if (shoot)
					neck = -ai.angle;

				if (!creature->Flags)
				{
					ShootHarpoon(item, item->Pose.Position, item->Animation.Velocity.z, item->Pose.Orientation.y, item->RoomNumber);
					creature->Flags = 1;
				}

				break;


			case SDIVER_STATE_TREAD_WATER_IDLE:
				creature->MaxTurn = SCUBA_DIVER_SWIM_TURN_RATE_MAX;

				if (shoot)
					head = ai.angle;

				if (creature->Target.y > waterHeight)
					item->Animation.TargetState = SDIVER_STATE_SWIM;
				else if (creature->Mood == MoodType::Escape)
					break;
				else if (shoot)
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

			case SDIVER_STATE_TREAD_WATER_SHOOT:
				if (shoot)
					head = ai.angle;

				if (!creature->Flags)
				{
					ShootHarpoon(item, item->Pose.Position, item->Animation.Velocity.z, item->Pose.Orientation.y, item->RoomNumber);
					creature->Flags = 1;
				}

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
		}
	}
}
