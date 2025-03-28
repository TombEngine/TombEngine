#include "framework.h"
#include "Objects/Generic/Object/Pushable/PushableCollision.h"

#include "Game/collision/collide_item.h"
#include "Game/collision/floordata.h"
#include "Game/collision/Point.h"
#include "Game/Lara/lara.h"
#include "Game/Setup.h"
#include "Objects/Generic/Object/BridgeObject.h"
#include "Objects/Generic/Object/Pushable/PushableBridge.h"
#include "Objects/Generic/Object/Pushable/PushableObject.h"
#include "Objects/Generic/Object/Pushable/PushableStack.h"
#include "Specific/Input/Input.h"
#include "Specific/level.h"

using namespace TEN::Collision::Floordata;
using namespace TEN::Collision::Point;
using namespace TEN::Entities::Generic;
using namespace TEN::Input;

namespace TEN::Entities::Generic
{
	constexpr auto PUSHABLE_FLOOR_HEIGHT_TOLERANCE = 32;

	bool IsPushableValid(ItemInfo& pushableItem)
	{
		auto& pushable = GetPushableInfo(pushableItem);

		// NOTE: Requires positive OCB value for interaction.
		if (pushableItem.Status == ITEM_INVISIBLE || pushableItem.TriggerFlags < 0)
			return false;

		auto pointColl = GetPointCollision(pushableItem);
		if (pushable.UseRoomCollision)
		{
			// HACK: Track if bridge was disabled by behaviour state.
			bool isEnabled = false;
			if (pushable.Bridge.has_value())
				isEnabled = pushable.Bridge->IsEnabled();

			// HACK: Temporarily disable bridge before probing.
			if (isEnabled && pushable.Bridge.has_value())
				pushable.Bridge->Disable(pushableItem);

			pointColl = GetPointCollision(pushableItem);
			pointColl.GetFloorHeight();

			// HACK: Reenable bridge after probing.
			if (isEnabled && pushable.Bridge.has_value())
				pushable.Bridge->Enable(pushableItem);
		}

		// 1) Check for wall.
		if (pointColl.GetSector().IsWall(pushableItem.Pose.Position.x, pushableItem.Pose.Position.z))
			return false;

		// 2) Test height from floor.
		int heightFromFloor = abs(pointColl.GetFloorHeight() - pushableItem.Pose.Position.y);
		if (heightFromFloor >= PUSHABLE_FLOOR_HEIGHT_TOLERANCE)
			return false;

		// 3) Test height from player. Prevents player from grabbing pushable at wrong elevation in stack.
		int heightFromPlayer = abs(LaraItem->Pose.Position.y - pushableItem.Pose.Position.y);
		if (heightFromPlayer >= PUSHABLE_FLOOR_HEIGHT_TOLERANCE)
			return false;

		return true;
	}

	bool IsPushableObjectMoveAllowed(ItemInfo& pushableItem, const Vector3i& targetPos, int targetRoomNumber)
	{
		auto& pushable = GetPushableInfo(pushableItem);

		pushable.IsOnEdge = false;

		auto pointColl = GetPointCollision(targetPos, targetRoomNumber);

		// 1) Check for wall.
		if (pointColl.GetSector().IsWall(targetPos.x, targetPos.z))
			return false;
		
		// 2) Check for gaps or steps.
		int heightFromFloor = abs(pointColl.GetFloorHeight() - pushableItem.Pose.Position.y);
		if (heightFromFloor >= PUSHABLE_FLOOR_HEIGHT_TOLERANCE)
		{
			// Step.
			if (pointColl.GetFloorHeight() < pushableItem.Pose.Position.y)
			{
				return false;
			}
			// Gap.
			else
			{
				pushable.IsOnEdge = true;
				if (!pushable.CanFall || pushable.Stack.ItemNumberAbove != NO_VALUE)
					return false;
			}
		}

		// 3) Check for slippery floor slope.
		if (pointColl.IsSteepFloor())
			return false;

		// 4) Check for diagonal floor step.
		if (pointColl.IsDiagonalFloorStep())
			return false;

		// 5) Test floor slope. TODO: Check slope angles of normals directly.
		if ((pointColl.GetSector().GetSurfaceNormal(0, true) != -Vector3::UnitY) || (pointColl.GetSector().GetSurfaceNormal(1, true) != -Vector3::UnitY))
			return false;

		// Check for stopper flag.
		/*if (collisionResult.Block->Stopper)
		{
			if (collisionResult.GetFloorHeight() <= pushableItem.Pose.Position.y)
				return false;
		}*/

		// Is ceiling (square or diagonal) high enough?
		int distFromCeil = abs(pointColl.GetCeilingHeight() - pointColl.GetFloorHeight());
		int blockHeight = GetStackHeight(pushableItem.Index) - PUSHABLE_FLOOR_HEIGHT_TOLERANCE;
		if (distFromCeil < blockHeight)
			return false;

		// TODO: Don't modify item.
		// Test object collision.
		auto prevPos = pushableItem.Pose.Position;
		pushableItem.Pose.Position = targetPos;
		auto collObjects = GetCollidedObjects(pushableItem, true, true);
		pushableItem.Pose.Position = prevPos;

		if (!collObjects.Statics.empty())
			return false;

		for (const auto* itemPtr : collObjects.Items)
		{
			const auto& object = Objects[itemPtr->ObjectNumber];

			if (object.isPickup)
				continue;

			if (!itemPtr->IsBridge())
				return false;

			const auto& bridge = GetBridgeObject(*itemPtr);
			if (!bridge.GetFloorHeight(*itemPtr, itemPtr->Pose.Position).has_value())
				return false;
		}

		return true;
	}

	bool IsValidForPlayer(const ItemInfo& pushableItem)
	{
		auto playerOffset = Vector3i::Zero;
				
		int quadrant = GetQuadrant(LaraItem->Pose.Orientation.y);
		switch (quadrant)
		{
		case NORTH:
			playerOffset.z = GetBestFrame(*LaraItem).Offset.z - BLOCK(1);
			break;

		case EAST:
			playerOffset.x = GetBestFrame(*LaraItem).Offset.z - BLOCK(1);
			break;

		case SOUTH:
			playerOffset.z = -GetBestFrame(*LaraItem).Offset.z + BLOCK(1);
			break;

		case WEST:
			playerOffset.x = -GetBestFrame(*LaraItem).Offset.z + BLOCK(1);
			break;

		default:
			break;
		}

		// This collisionResult is the point where Lara would be at the end of the pushable pull.
		auto pointColl = GetPointCollision(LaraItem->Pose.Position + playerOffset, LaraItem->RoomNumber);

		// Test for wall.
		if (pointColl.GetSector().IsWall(pointColl.GetPosition().x, pointColl.GetPosition().z))
			return false;

		// Test for flat floor.
		int floorHeightDelta = abs(pointColl.GetFloorHeight() - LaraItem->Pose.Position.y);
		if (floorHeightDelta >= PUSHABLE_FLOOR_HEIGHT_TOLERANCE)
			return false;

		// Test floor-to-ceiling height.
		int floorToCeilHeight = abs(pointColl.GetCeilingHeight() - pointColl.GetFloorHeight());
		if (floorToCeilHeight < LARA_HEIGHT)
			return false;

		// Collide with objects.
		auto prevPos = LaraItem->Pose.Position;
		LaraItem->Pose.Position = pointColl.GetPosition();
		auto collObjects = GetCollidedObjects(*LaraItem, true, true);
		LaraItem->Pose.Position = prevPos;

		if (!collObjects.Statics.empty())
			return false;

		for (const auto* itemPtr : collObjects.Items)
		{
			const auto& object = Objects[itemPtr->ObjectNumber];

			if (itemPtr->Index == pushableItem.Index)
				continue;

			if (object.isPickup)
				continue;

			if (!itemPtr->IsBridge())
			{
				return false;
			}
			else
			{
				const auto& bridge = GetBridgeObject(*itemPtr);
				if (!bridge.GetFloorHeight(*itemPtr, itemPtr->Pose.Position).has_value())
					return false;
			}
		}

		return true;
	}

	bool PushableIdleConditions(ItemInfo& pushableItem)
	{
		auto& pushable = GetPushableInfo(pushableItem);

		// If player is grabbing, check push and pull actions.
		if (LaraItem->Animation.ActiveState != LS_PUSHABLE_GRAB || 
			!TestLastFrame(LaraItem, LA_PUSHABLE_GRAB))
		{
			return false;
		}

		bool hasPushAction = IsHeld(In::Forward);
		bool hasPullAction = IsHeld(In::Back);

		// Check for input actions.
		if (!hasPushAction && !hasPullAction)
			return false;

		int quadrant = GetQuadrant(LaraItem->Pose.Orientation.y);
		const auto& edgeAttribs = pushable.EdgeAttribs[quadrant];

		// Test edge attributes for quadrant.
		if ((hasPushAction && !edgeAttribs.IsPushable) ||
			(hasPullAction && !edgeAttribs.IsPullable))
		{
			return false;
		}

		// Test stack limit.
		if (!IsWithinStackLimit(pushableItem.Index))
			return false;

		// Test movement conditions.
		if (!TestPushableMovementConditions(pushableItem, hasPushAction, hasPullAction))
			return false;

		return true;
	}

	bool TestPushableMovementConditions(ItemInfo& pushableItem, bool hasPushAction, bool hasPullAction)
	{
		const auto& pushable = GetPushableInfo(pushableItem);

		int quadrant = GetQuadrant(LaraItem->Pose.Orientation.y);
		auto targetPos = pushableItem.Pose.Position;

		int dir = (hasPushAction) ? 1 : -1;

		switch (quadrant)
		{
		case NORTH:
			targetPos.z = targetPos.z + dir * BLOCK(1);
			break;

		case EAST:
			targetPos.x = targetPos.x + dir * BLOCK(1);
			break;

		case SOUTH:
			targetPos.z = targetPos.z - dir * BLOCK(1);
			break;

		case WEST:
			targetPos.x = targetPos.x - dir * BLOCK(1);
			break;
		}

		int targetRoomNumber = pushableItem.RoomNumber;

		if (!IsPushableObjectMoveAllowed(pushableItem, targetPos, targetRoomNumber))
			return false;

		if (hasPullAction && !IsValidForPlayer(pushableItem))
			return false;

		SetPushableStopperFlag(true, targetPos, targetRoomNumber);
		return true;
	}

	PushableCollisionData GetPushableCollision(ItemInfo& item)
	{
		auto& pushable = GetPushableInfo(item);

		auto pointColl = GetPointCollision(item);
		int waterHeight = NO_HEIGHT;

		auto pushableColl = PushableCollisionData{};

		// TODO: If bridges system changes, this routine may be similar to object pushables ones, consider for review.
		if (pushable.UseBridgeCollision)
		{
			// HACK: Track if bridge was disabled by behaviour state.
			bool isEnabled = false;
			if (pushable.Bridge.has_value())
				isEnabled = pushable.Bridge->IsEnabled();

			// HACK: Temporarily disable bridge before probing.
			if (isEnabled && pushable.Bridge.has_value())
				pushable.Bridge->Disable(item);

			pointColl = GetPointCollision(item);

			waterHeight = pointColl.GetWaterSurfaceHeight();
			if (waterHeight == NO_HEIGHT && TestEnvironment(ENV_FLAG_SWAMP, item.RoomNumber))
				waterHeight = g_Level.Rooms[item.RoomNumber].TopHeight;

			pushableColl.FloorHeight = pointColl.GetFloorHeight();
			pushableColl.CeilingHeight = pointColl.GetCeilingHeight();

			// HACK: Reenable bridge after probing.
			if (isEnabled && pushable.Bridge.has_value())
				pushable.Bridge->Enable(item);
		}
		else
		{
			waterHeight = pointColl.GetWaterSurfaceHeight();
			if (waterHeight == NO_HEIGHT && TestEnvironment(ENV_FLAG_SWAMP, item.RoomNumber))
				waterHeight = g_Level.Rooms[item.RoomNumber].TopHeight;

			pushableColl.FloorHeight = pointColl.GetFloorHeight();
			pushableColl.CeilingHeight = pointColl.GetCeilingHeight();
		}

		// Above water.
		if (TestEnvironment(ENV_FLAG_WATER, item.RoomNumber) ||
			TestEnvironment(ENV_FLAG_SWAMP, item.RoomNumber))
		{
			pushable.WaterSurfaceHeight = waterHeight;

			if (pushableColl.FloorHeight > (item.Pose.Position.y + item.Animation.Velocity.y) &&
				abs(item.Pose.Position.y - pushableColl.FloorHeight) >= PUSHABLE_FLOOR_HEIGHT_TOLERANCE)
			{
				pushableColl.EnvType = PushableEnvironmentType::Water;
			}
			else
			{
				pushableColl.EnvType = PushableEnvironmentType::WaterFloor;
			}
		}
		// Above dry ground.
		else
		{
			pushable.WaterSurfaceHeight = NO_HEIGHT;

			// Airborne.
			if (pushableColl.FloorHeight > (item.Pose.Position.y + item.Animation.Velocity.y) &&
				abs(item.Pose.Position.y - pushableColl.FloorHeight) >= PUSHABLE_FLOOR_HEIGHT_TOLERANCE)
			{
				pushableColl.EnvType = PushableEnvironmentType::Air;
			}
			// Grounded.
			else
			{
				auto floorSlopeAngle = Geometry::GetSurfaceSlopeAngle(pointColl.GetFloorNormal());

				if (floorSlopeAngle == 0)
				{
					pushableColl.EnvType = PushableEnvironmentType::FlatFloor;
				}
				else
				{
					pushableColl.EnvType = PushableEnvironmentType::SlopedFloor;
				}
			}
		}

		return pushableColl;
	}
}
