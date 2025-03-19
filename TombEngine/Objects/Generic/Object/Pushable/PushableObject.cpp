#include "framework.h"
#include "Objects/Generic/Object/Pushable/PushableObject.h"

#include "Game/Animation/Animation.h"
#include "Game/collision/collide_item.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/Point.h"
#include "Game/collision/floordata.h"
#include "Game/control/box.h"
#include "Game/control/flipeffect.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Setup.h"
#include "Objects/Generic/Object/Pushable/PushableBridge.h"
#include "Objects/Generic/Object/Pushable/PushableCollision.h"
#include "Objects/Generic/Object/Pushable/PushableInfo.h"
#include "Objects/Generic/Object/Pushable/PushableSound.h"
#include "Objects/Generic/Object/Pushable/PushableStack.h"
#include "Objects/Generic/Object/Pushable/PushableStates.h"
#include "Sound/sound.h"
#include "Specific/Input/Input.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Collision::Floordata;
using namespace TEN::Collision::Point;
using namespace TEN::Input;

namespace TEN::Entities::Generic
{
	auto PushableBlockPos = Vector3i::Zero;

	auto PushableBlockBounds = ObjectCollisionBounds
	{
		GameBoundingBox(
			0, 0,
			-CLICK(0.25f), 0,
			0, 0),
		std::pair(
			EulerAngles(ANGLE(-10.0f), -LARA_GRAB_THRESHOLD, ANGLE(-10.0f)),
			EulerAngles(ANGLE(10.0f), LARA_GRAB_THRESHOLD, ANGLE(10.0f)))
	};

	std::vector<PushableAnimSet> PushableAnimSets =
	{
		// Pushable object (TR4-5).
		{ LA_PUSHABLE_OBJECT_PULL, LA_PUSHABLE_OBJECT_PUSH, LA_PUSHABLE_OBJECT_PUSH_EDGE_SLIP, true },

		// Pushable block (TR1-3).
		{ LA_PUSHABLE_BLOCK_PULL, LA_PUSHABLE_BLOCK_PUSH, LA_PUSHABLE_BLOCK_PUSH_EDGE_SLIP, false }
	};

	PushableInfo& GetPushableInfo(const ItemInfo& item)
	{
		return (PushableInfo&)item.Data;
	}

	void InitializePushableBlock(int itemNumber)
	{
		auto& pushableItem = g_Level.Items[itemNumber];
		if (pushableItem.Data == NULL) // First pushableItem in initialize.
			InitializePushableStacks();
		
		auto& pushable = GetPushableInfo(pushableItem);

		pushable.StartPos = GameVector(pushableItem.Pose.Position, pushableItem.RoomNumber);
		pushable.Height = GetPushableHeight(pushableItem);

		if (pushableItem.ObjectNumber >= ID_PUSHABLE_OBJECT_CLIMBABLE1 &&
			pushableItem.ObjectNumber <= ID_PUSHABLE_OBJECT_CLIMBABLE10)
		{
			pushable.UseRoomCollision = true;
			AddPushableBridge(pushableItem);
		}
		else
		{
			pushable.UseRoomCollision = false;
		}

		SetPushableStopperFlag(true, pushableItem.Pose.Position, pushableItem.RoomNumber);

		// Read OCB flags.
		int ocb = pushableItem.TriggerFlags;
		pushable.CanFall	   = (ocb & (1 << 0)) != 0;			  // Bit 0.
		pushable.DoCenterAlign = (ocb & (1 << 1)) == 0;			  // Bit 1.
		pushable.IsBuoyant	   = (ocb & (1 << 2)) != 0;			  // Bit 2.
		pushable.AnimSetID	   = ((ocb & (1 << 3)) != 0) ? 1 : 0; // Bit 3.

		pushableItem.Status = ITEM_ACTIVE;
		AddActiveItem(itemNumber);
	}

	void PushableBlockControl(int itemNumber)
	{
		auto& pushableItem = g_Level.Items[itemNumber];
		auto& pushable = GetPushableInfo(pushableItem);

		if (Lara.Context.InteractedItem == itemNumber && Lara.Control.IsMoving)
			return;

		// Handle behaviour and sound state.
		HandlePushableBehaviorState(pushableItem);
		HandlePushableSoundState(pushableItem);

		// Update bridge.
		AddPushableStackBridge(pushableItem, false);
		int probeRoomNumber = GetPointCollision(pushableItem).GetRoomNumber();
		AddPushableStackBridge(pushableItem, true);

		// Update room number.
		if (pushableItem.RoomNumber != probeRoomNumber)
		{
			ItemNewRoom(itemNumber, probeRoomNumber);
			pushable.StartPos.RoomNumber = pushableItem.RoomNumber;
		}
	}

	// If player is holding Action, initiates object interaction.
	// Otherwise, activates normal object collision.
	void PushableBlockCollision(int itemNumber, ItemInfo* playerItem, CollisionInfo* coll)
	{
		auto& pushableItem = g_Level.Items[itemNumber];
		auto& pushable = GetPushableInfo(pushableItem);
		auto& player = *GetLaraInfo(playerItem);

		int quadrant = GetQuadrant(LaraItem->Pose.Orientation.y);
		auto& pushableSidesAttributes = pushable.EdgeAttribs[quadrant]; // NOTE: 0 = north, 1 = east, 2 = south, 3 = west.

		// Align player to pushable.
		if ((IsHeld(In::Action) &&
			!IsHeld(In::Forward) &&
			playerItem->Animation.ActiveState == LS_IDLE &&
			playerItem->Animation.AnimNumber == LA_STAND_IDLE &&
			!playerItem->Animation.IsAirborne &&
			player.Control.HandStatus == HandStatus::Free &&
			IsPushableValid(pushableItem)) &&
			pushable.BehaviorState == PushableBehaviourState::Idle && 
			(pushableSidesAttributes.IsPushable || pushableSidesAttributes.IsPullable) || // Can interact with this side.
			(player.Control.IsMoving && player.Context.InteractedItem == itemNumber))	  // Already interacting.
		{
			// Set pushable bounds.
			auto bounds = GameBoundingBox(&pushableItem);
			PushableBlockBounds.BoundingBox.X1 = (bounds.X1 / 2) - 100;
			PushableBlockBounds.BoundingBox.X2 = (bounds.X2 / 2) + 100;
			PushableBlockBounds.BoundingBox.Z1 = bounds.Z1 - 200;
			PushableBlockBounds.BoundingBox.Z2 = 0;

			short yOrient = pushableItem.Pose.Orientation.y;
			pushableItem.Pose.Orientation.y = GetQuadrant(playerItem->Pose.Orientation.y) * ANGLE(90.0f);

			// Within interaction range, calculate target position for alignment.
			if (TestLaraPosition(PushableBlockBounds, &pushableItem, playerItem))
			{
				int quadrant = GetQuadrant(pushableItem.Pose.Orientation.y);
				switch (quadrant)
				{
				case NORTH:
					PushableBlockPos.z = bounds.Z1 - CLICK(0.4f);
					PushableBlockPos.x = pushable.DoCenterAlign ? 0 : LaraItem->Pose.Position.x - pushableItem.Pose.Position.x;
					break;

				case SOUTH:
					PushableBlockPos.z = (bounds.Z2 + CLICK(0.4f)) * (-1);
					PushableBlockPos.x = pushable.DoCenterAlign ? 0 : pushableItem.Pose.Position.x - LaraItem->Pose.Position.x;
					break;

				case EAST:
					PushableBlockPos.z = bounds.X1 - CLICK(0.4f);
					PushableBlockPos.x = pushable.DoCenterAlign ? 0 : pushableItem.Pose.Position.z - LaraItem->Pose.Position.z;
					break;

				case WEST:
					PushableBlockPos.z = (bounds.X2 + CLICK(0.4f)) * (- 1);
					PushableBlockPos.x = pushable.DoCenterAlign ? 0 : LaraItem->Pose.Position.z - pushableItem.Pose.Position.z;
					break;

				default:
					break;
				}

				// Align player.
				if (MoveLaraPosition(PushableBlockPos, &pushableItem, playerItem))
				{
					SetAnimation(*playerItem, LA_PUSHABLE_GRAB);
					playerItem->Pose.Orientation = pushableItem.Pose.Orientation;
					player.Control.IsMoving = false;
					player.Control.HandStatus = HandStatus::Busy;
					player.Context.NextCornerPos.Position.x = itemNumber; // TODO: Do this differently.
				}

				player.Context.InteractedItem = itemNumber;
			}
			else
			{
				if (player.Control.IsMoving && player.Context.InteractedItem == itemNumber)
				{
					player.Control.IsMoving = false;
					player.Control.HandStatus = HandStatus::Free;
				}
			}

			pushableItem.Pose.Orientation.y = yOrient;
		}
		else
		{
			// Not holding Action; do normal collision routine.
			if (playerItem->Animation.ActiveState != LS_PUSHABLE_GRAB ||
				!TestLastFrame(*playerItem, LA_PUSHABLE_GRAB) ||
				player.Context.NextCornerPos.Position.x != itemNumber)
			{
				// NOTE: If using room collision, use bridge collision.
				if (!pushable.UseRoomCollision)
					ObjectCollision(itemNumber, playerItem, coll);

				return;
			}
		}
	}

	int GetPushableHeight(const ItemInfo& item)
	{
		int boxHeight = -GameBoundingBox(&item).Y1;
		int worldAlignedHeight = (boxHeight / CLICK(0.5)) * CLICK(0.5);
		return worldAlignedHeight;
	}

	void SetPushableStopperFlag(bool isStopper, const Vector3i& pos, int roomNumber)
	{
		auto pointColl = GetPointCollision(pos, roomNumber);
		pointColl.GetSector().Stopper = isStopper;

		// TODO: There is a problem, it also has to set/reset the flag in the flipped room.
		// Because when flipmaps happens, it forgets about the old flag.
	}
}
