#include "framework.h"
#include "Objects/TR1/Trap/ThorHammer.h"

#include "Game/camera.h"
#include "Game/collision/collide_item.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/Point.h"
#include "Game/collision/Sphere.h"
#include "Game/effects/effects.h"
#include "Game/Lara/lara.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Specific/level.h"

using namespace TEN::Collision::Point;
using namespace TEN::Collision::Sphere;
using namespace TEN::Math;

// NOTES:
// item.TriggerFlags = 0; Default TR1 behavior.
// item.TriggerFlags = 1; Retract after striking once.
// item.TriggerFlags = 2; Strike continuously.

namespace TEN::Entities::Traps
{
	constexpr auto HAMMER_HIT_FRAME = 30;

	enum HammerState
	{
		HAMMER_STATE_IDLE = 0,
		HAMMER_STATE_UNSTABLE = 1,
		HAMMER_STATE_FALL_START = 2,
		HAMMER_STATE_FALL_END = 3,
		HAMMER_STATE_RETRACT = 4
	};

	enum HammerAnim
	{
		HAMMER_ANIM_IDLE = 0,
		HAMMER_ANIM_UNSTABLE = 1,
		HAMMER_ANIM_FALL_START = 2,
		HAMMER_ANIM_FALL_END = 3,
		HAMMER_ANIM_RETRACT = 4
	};

	void InitializeThorHammer(short itemNumber)
	{
		auto& headItem = g_Level.Items[itemNumber];

		int handleItemNumber = SpawnItem(headItem, ID_THOR_HAMMER_HEAD);
		if (handleItemNumber == NO_VALUE)
		{
			TENLog("Failed to create hammer handle moveable.", LogLevel::Warning);
			return;
		}

		auto& handleItem = g_Level.Items[handleItemNumber];

		// Store hammer handle item number.
		headItem.ItemFlags[0] = handleItemNumber;
		handleItem.ItemFlags[0] = NO_VALUE;
	}

	static void SyncThorHammerHandle(ItemInfo& headItem)
	{
		int handleItemNumber = headItem.ItemFlags[0];
		auto& handleItem = g_Level.Items[handleItemNumber];

		// Sync item status.
		handleItem.Status = headItem.Status;

		// Sync TriggerFlags.
		handleItem.TriggerFlags = headItem.TriggerFlags;

		// Sync animation.
		SetAnimation(handleItem, headItem.Animation.AnimNumber, headItem.Animation.FrameNumber);

		// Sync position.
		handleItem.Pose = headItem.Pose;
		if (handleItem.RoomNumber != headItem.RoomNumber)
			ItemNewRoom(handleItem.Index, headItem.RoomNumber);
	}

	void ControlThorHammer(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];
		const auto& playerItem = *LaraItem;
		
		switch (item.Animation.ActiveState)
		{
		case HAMMER_STATE_IDLE:
			if (TriggerActive(&item))
			{
				if (std::abs(item.TriggerFlags) == 1 && item.ItemFlags[1] == 1)
				{
					item.Status = ITEM_NOT_ACTIVE;
					break;

				}

				if (std::abs(item.TriggerFlags) == 2)
				{
					item.Animation.TargetState = HAMMER_STATE_FALL_START;
					break;
					
				}

				item.Animation.TargetState = HAMMER_STATE_UNSTABLE;
			}
			else
			{
				RemoveActiveItem(itemNumber);
				item.Status = ITEM_NOT_ACTIVE;
			}

			break;

		case HAMMER_STATE_UNSTABLE:
			if (TriggerActive(&item))
			{
				item.Animation.TargetState = HAMMER_STATE_FALL_START;
			}
			else
			{
				item.Animation.TargetState = HAMMER_STATE_IDLE;
			}

			break;

		case HAMMER_STATE_FALL_START:
			break;

		case HAMMER_STATE_FALL_END:
			if (abs(item.TriggerFlags) > 0)
			{
				item.Animation.TargetState = HAMMER_STATE_RETRACT;

				if (abs(item.TriggerFlags) == 1)
					item.ItemFlags[1] = 1;
			}
			else
			{
				item.Status = ITEM_NOT_ACTIVE;
				RemoveActiveItem(itemNumber);
			}

			break;
		}

		AnimateItem(item);
		SyncThorHammerHandle(item);
	}
	
	void CollideThorHammer(short itemNumber, ItemInfo* playerItem, CollisionInfo* coll)
	{
		auto& item = g_Level.Items[itemNumber];

		if (!TestBoundsCollide(&item, playerItem, coll->Setup.Radius))
			return;

		if (!HandleItemSphereCollision(item, *playerItem))
			return;

		if (item.Animation.ActiveState == HAMMER_STATE_FALL_START && item.Animation.FrameNumber <= HAMMER_HIT_FRAME)
		{
			auto pointColl = GetPointCollision(*playerItem);

			playerItem->Pose.Position.y = pointColl.GetFloorHeight();
			playerItem->Animation.Velocity = Vector3::Zero;
			playerItem->Animation.IsAirborne = false;

			if (item.TriggerFlags < 0)
			playerItem->Pose.Scale = Vector3(1.0f, 0.1f, 1.0f);

			DoDamage(playerItem, INT_MAX);
			SetAnimation(*playerItem, LA_BOULDER_DEATH);
		}
		else if (playerItem->HitPoints > 0)
		{
			ItemPushItem(&item, playerItem, coll, false, 1);
		}
	}

	void CollideThorHammerHandle(short itemNumber, ItemInfo* playerItem, CollisionInfo* coll)
	{
		auto& item = g_Level.Items[itemNumber];

		if (!TestBoundsCollide(&item, playerItem, coll->Setup.Radius))
			return;

		if (coll->Setup.EnableObjectPush)
			ItemPushItem(&item, playerItem, coll, false, 1);
	}
}
