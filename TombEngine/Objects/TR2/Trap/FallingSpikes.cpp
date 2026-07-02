#include "framework.h"
#include "Objects/TR2/Trap/FallingSpikes.h"

#include "Game/Animation/Animation.h"
#include "Game/camera.h"
#include "Game/collision/collide_item.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/Point.h"
#include "Game/effects/effects.h"
#include "Game/Lara/lara.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Scripting/Internal/TEN/Properties/PropertyHandler.h"
#include "Specific/level.h"

using namespace TEN::Collision::Point;
using namespace TEN::Math;
using namespace TEN::Scripting::Properties;

namespace TEN::Entities::Traps
{
	// NOTES:
	// ItemFlags[0] = save original orientation.
	// ItemFlags[1] = calculated forward velocity.

	constexpr auto FALLING_SPIKES_DAMAGE = 100;
	constexpr auto FALLING_SPIKES_VELOCITY_MIN = BLOCK(1 / 20.0f);
	constexpr auto FALLING_SPIKES_VELOCITY_MAX = BLOCK(1 / 8.0f);
	constexpr auto FALLING_SPIKES_ACTIVATE_RANGE_2D		  = BLOCK(1.5f);
	constexpr auto FALLING_SPIKES_ACTIVATE_RANGE_VERTICAL = BLOCK(8);

	enum FallingSpikesState
	{
		FALLINGSPIKES_STATE_HANGING = 1,
		FALLINGSPIKES_STATE_FALLING = 2,
		FALLINGSPIKES_STATE_FLOOR = 3
	};

	enum FallingSpikesAnim
	{
		FALLINGSPIKES_ANIM_HANGING = 0,
		FALLINGSPIKES_ANIM_FALLING = 1,
		FALLINGSPIKES_ANIM_FLOOR = 2
	};

	void InitializeFallingSpikes(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];
		item.Animation.Velocity.y = FALLING_SPIKES_VELOCITY_MIN;
		item.ItemFlags[0] = item.Pose.Orientation.y;
	}

	void ControlFallingSpikes(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];
		const auto& laraItem = *LaraItem;

		if (item.Status == ItemStatus::ITEM_NOT_ACTIVE)
			return;

		// Fall toward player.
		if (item.Animation.IsAirborne)
		{
			// Keep original rotation.
			item.Pose.Orientation.y = item.ItemFlags[0];

			// Calculate vertical velocity.
			item.Animation.Velocity.y += (item.Animation.Velocity.y < FALLING_SPIKES_VELOCITY_MAX) ? g_GameFlow->GetSettings()->Physics.Gravity : 1.0f;
			auto orient = Geometry::GetOrientToPoint(item.Pose.Position.ToVector3(), laraItem.Pose.Position.ToVector3());
			short headingAngle = (short)orient.y;

			// If "FallingSpikeForwardVelocity" property is true: falls towards the Player.
			item.ItemFlags[1] = PropertyHandler::Get(item, "FallingSpikeForwardVelocity", false) ? item.ItemFlags[1] : 0;
			item.Pose.Translate(headingAngle, item.ItemFlags[1], item.Animation.Velocity.y);

			int vPos = item.Pose.Position.y;
			auto pointColl = GetPointCollision(item);
			auto floorY = pointColl.GetFloorHeight();

			int probedRoomNumber = GetPointCollision(item).GetRoomNumber();
			if (item.RoomNumber != probedRoomNumber)
				ItemNewRoom(itemNumber, probedRoomNumber);

			// Impale floor.
			if (vPos > floorY && item.Animation.TargetState != FALLINGSPIKES_STATE_FLOOR)
			{
				item.Pose.Position.y = floorY;
				item.Animation.TargetState = FALLINGSPIKES_STATE_FLOOR;
				item.Animation.AnimNumber = FALLINGSPIKES_ANIM_FLOOR;
				float distance = Vector3::Distance(item.Pose.Position.ToVector3(), Camera.pos.ToVector3());
				Camera.bounce = -((BLOCK(7.0f / 2) - distance) * abs(item.Animation.Velocity.y)) / BLOCK(7.0f / 2);
				item.Animation.IsAirborne = false;
			}

			if (item.Animation.AnimNumber == FALLINGSPIKES_ANIM_FLOOR &&
				item.Animation.FrameNumber == GetAnimData(item).EndFrameNumber)
			{
				item.Status = ItemStatus::ITEM_NOT_ACTIVE;
					RemoveActiveItem(itemNumber);
			}
			
			return;
		}
		else if (item.Pose.Position.y < GetPointCollision(item).GetFloorHeight() && item.Animation.TargetState != FALLINGSPIKES_STATE_FLOOR)
		{
			// Scan for Player.
			// Keep original rotation.
			item.Pose.Orientation.y = item.ItemFlags[0];

			// Check vertical position to player.
			if (item.Pose.Position.y >= laraItem.Pose.Position.y)
				return;

			// Check vertical distance.
			float distanceV = laraItem.Pose.Position.y - item.Pose.Position.y;
			if (distanceV > FALLING_SPIKES_ACTIVATE_RANGE_VERTICAL)
				return;

			// Check 2D distance.
			float distance2D = Vector2i::Distance(
				Vector2i(item.Pose.Position.x, item.Pose.Position.z),
				Vector2i(laraItem.Pose.Position.x, laraItem.Pose.Position.z));
			if (distance2D > FALLING_SPIKES_ACTIVATE_RANGE_2D)
				return;

			// Drop spikes.
			if (item.Animation.TargetState != FALLINGSPIKES_STATE_FALLING)
			{
				item.Animation.TargetState = FALLINGSPIKES_STATE_FALLING;
				item.Animation.IsAirborne = true;
				item.ItemFlags[1] = distance2D / 32;
				return;
			}
		}

		AnimateItem(&item);
	}

	void CollideFallingSpikes(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll)
	{
		auto& item = g_Level.Items[itemNumber];

		if (!TestBoundsCollide(&item, laraItem, coll->Setup.Radius))
			return;

		if (coll->Setup.EnableObjectPush)
			ItemPushItem(&item, laraItem, coll, false, 1);

		if (item.Animation.IsAirborne)
		{
			DoDamage(laraItem, FALLING_SPIKES_DAMAGE);

			auto bloodBox = GameBoundingBox(laraItem).ToBoundingOrientedBox(laraItem->Pose);
			auto bloodPos = Vector3i(Random::GeneratePointInBox(bloodBox));

			auto orientToSword = Geometry::GetOrientToPoint(laraItem->Pose.Position.ToVector3(), item.Pose.Position.ToVector3());
			short randAngleOffset = Random::GenerateAngle(ANGLE(-11.25f), ANGLE(11.25f));
			short bloodHeadingAngle = orientToSword.y + randAngleOffset;
			
			DoLotsOfBlood(bloodPos.x, bloodPos.y, bloodPos.z, laraItem->Animation.Velocity.z, bloodHeadingAngle, laraItem->RoomNumber, 20);
		}
	}
}
