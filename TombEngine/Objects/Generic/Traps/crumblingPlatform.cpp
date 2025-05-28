#include "framework.h"
#include "Objects/Generic/Traps/CrumblingPlatform.h"

#include "Game/collision/collide_item.h"
#include "Game/collision/floordata.h"
#include "Game/collision/Point.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/setup.h"
#include "Objects/Generic/Object/BridgeObject.h"
#include "Specific/clock.h"
#include "Specific/level.h"

using namespace TEN::Collision::Floordata;
using namespace TEN::Collision::Point;
using namespace TEN::Entities::Generic;

// NOTES:
// ItemFlags[0]: Delay in frame time.
// ItemFlags[1]: Fall velocity.

namespace TEN::Entities::Traps
{
	constexpr auto CRUMBLING_PLATFORM_VELOCITY_MAX	 = 100.0f;
	constexpr auto CRUMBLING_PLATFORM_VELOCITY_MIN	 = 10.0f;
	constexpr auto CRUMBLING_PLATFORM_VELOCITY_ACCEL = 4.0f;

	constexpr auto CRUMBLING_PLATFORM_DELAY = 1.2f;

	enum CrumblingPlatformState
	{
		CRUMBLING_PLATFORM_STATE_IDLE = 0,
		CRUMBLING_PLATFORM_STATE_SHAKE = 1,
		CRUMBLING_PLATFORM_STATE_FALL = 2,
		CRUMBLING_PLATFORM_STATE_LAND = 3
	};

	enum CrumblingPlatformAnim
	{
		CRUMBLING_PLATFORM_ANIM_IDLE = 0,
		CRUMBLING_PLATFORM_ANIM_SHAKE = 1,
		CRUMBLING_PLATFORM_ANIM_FALL = 2,
		CRUMBLING_PLATFORM_ANIM_LAND = 3
	};

	static std::optional<int> GetCrumblingPlatformFloorHeight(const ItemInfo& item, const Vector3i& pos)
	{
		if (item.Animation.ActiveState == CRUMBLING_PLATFORM_STATE_IDLE ||
			item.Animation.ActiveState == CRUMBLING_PLATFORM_STATE_SHAKE)
		{
			auto boxHeight = GetBridgeItemIntersect(item, pos, false);
			if (boxHeight.has_value())
				return *boxHeight;
		}

		return std::nullopt;
	}

	static std::optional<int> GetCrumblingPlatformCeilingHeight(const ItemInfo& item, const Vector3i& pos)
	{
		if (item.Animation.ActiveState == CRUMBLING_PLATFORM_STATE_IDLE ||
			item.Animation.ActiveState == CRUMBLING_PLATFORM_STATE_SHAKE)
		{
			auto boxHeight = GetBridgeItemIntersect(item, pos, true);
			if (boxHeight.has_value())
				return *boxHeight;
		}

		return std::nullopt;
	}

	static int GetCrumblingPlatformFloorBorder(const ItemInfo& item)
	{
		auto bounds = GameBoundingBox(&item);
		return bounds.Y1;
	}

	static int GetCrumblingPlatformCeilingBorder(const ItemInfo& item)
	{
		auto bounds = GameBoundingBox(&item);
		return bounds.Y2;
	}

	void InitializeCrumblingPlatform(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];
		item.Data = BridgeObject();
		auto& bridge = GetBridgeObject(item);

		int delayInFrameTime = (item.TriggerFlags != 0) ? std::abs(item.TriggerFlags) : (int)round(CRUMBLING_PLATFORM_DELAY * FPS);
		item.ItemFlags[0] = delayInFrameTime;

		// Initialize routines.
		bridge.GetFloorHeight = GetCrumblingPlatformFloorHeight;
		bridge.GetCeilingHeight = GetCrumblingPlatformCeilingHeight;
		bridge.GetFloorBorder = GetCrumblingPlatformFloorBorder;
		bridge.GetCeilingBorder = GetCrumblingPlatformCeilingBorder;
		bridge.Initialize(item);
	}

	static void ActivateCrumblingPlatform(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		item.Status = ITEM_ACTIVE;
		AddActiveItem(itemNumber);

		item.Animation.TargetState = CRUMBLING_PLATFORM_STATE_SHAKE;
		item.Flags |= CODE_BITS;
	}

	void ControlCrumblingPlatform(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];
		auto& bridge = GetBridgeObject(item);

		bridge.Update(item);

		// OCB < 0; must be activated by trigger.
		if (item.TriggerFlags < 0)
		{
			if (TriggerActive(&item))
			{
				ActivateCrumblingPlatform(itemNumber);
				item.TriggerFlags = -item.TriggerFlags;
			}

			return;
		}

		switch (item.Animation.ActiveState)
		{
		case CRUMBLING_PLATFORM_STATE_IDLE:
			break;

		case CRUMBLING_PLATFORM_STATE_SHAKE:
		{
			if (item.ItemFlags[0] > 0)
			{
				item.ItemFlags[0]--;
			}
			else
			{
				item.Animation.TargetState = CRUMBLING_PLATFORM_STATE_FALL;
				item.ItemFlags[1] = CRUMBLING_PLATFORM_VELOCITY_MIN;

				auto& room = g_Level.Rooms[item.RoomNumber];
				bridge.Disable(item);
			}
		}

		break;

		case CRUMBLING_PLATFORM_STATE_FALL:
		{
			short& fallVel = item.ItemFlags[1];

			// Get point collision.
			auto box = GameBoundingBox(&item);
			auto pointColl = GetPointCollision(item);
			int relFloorHeight = (item.Pose.Position.y - pointColl.GetFloorHeight()) - box.Y1 ;

			// Airborne.
			if (relFloorHeight <= fallVel)
			{
				fallVel += CRUMBLING_PLATFORM_VELOCITY_ACCEL;
				if (fallVel > CRUMBLING_PLATFORM_VELOCITY_MAX)
					fallVel = CRUMBLING_PLATFORM_VELOCITY_MAX;

				item.Pose.Position.y += fallVel;
			}
			// Grounded.
			else
			{
				item.Animation.TargetState = CRUMBLING_PLATFORM_STATE_LAND;
				item.Pose.Position.y = pointColl.GetFloorHeight();
			}

			// Update room number.
			int probedRoomNumber = pointColl.GetRoomNumber();
			if (item.RoomNumber != probedRoomNumber)
				ItemNewRoom(itemNumber, probedRoomNumber);
		}

		break;

		case CRUMBLING_PLATFORM_STATE_LAND:
		{
			// Align to surface.
			auto radius = Vector2(Objects[item.ObjectNumber].radius);
			AlignEntityToSurface(&item, radius);

			// Deactivate.
			if (TestLastFrame(&item))
			{
				RemoveActiveItem(itemNumber);
				item.Status = ITEM_NOT_ACTIVE;
			}
		}

		break;

		default:
			TENLog(
				"Error with crumbling platform item " + std::to_string(itemNumber) +
				": attempted to handle missing state " + std::to_string(item.Animation.ActiveState),
				LogLevel::Error, LogConfig::All);
			break;
		}

		AnimateItem(&item);
	}

	void CollideCrumblingPlatform(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll)
	{
		auto& item = g_Level.Items[itemNumber];
		const auto& player = GetLaraInfo(*laraItem);

		// OCB >= 0; activate via player collision. OCB < 0 activates via trigger.
		if (item.TriggerFlags >= 0 && item.Animation.ActiveState == CRUMBLING_PLATFORM_STATE_IDLE)
		{
			// Crumble if player is on platform.
			if (!laraItem->Animation.IsAirborne &&
				player.Control.WaterStatus != WaterStatus::TreadWater &&
				player.Control.WaterStatus != WaterStatus::Underwater &&
				player.Control.WaterStatus != WaterStatus::FlyCheat &&
				coll->LastBridgeItemNumber == item.Index)
			{
				ActivateCrumblingPlatform(itemNumber);
			}
		}
	}
}
