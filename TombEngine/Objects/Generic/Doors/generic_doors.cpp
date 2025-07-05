#include "framework.h"
#include "Objects/Generic/Doors/generic_doors.h"

#include "Game/animation.h"
#include "Game/collision/collide_item.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/Sphere.h"
#include "Game/control/control.h"
#include "Game/control/box.h"
#include "Game/control/lot.h"
#include "Game/Gui.h"
#include "Game/itemdata/door_data.h"
#include "Game/itemdata/itemdata.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Lara/lara_struct.h"
#include "Game/misc.h"
#include "Game/pickup/pickup.h"
#include "Objects/Generic/Switches/cog_switch.h"
#include "Objects/objectslist.h"
#include "Physics/Physics.h"
#include "Sound/sound.h"
#include "Specific/Input/Input.h"
#include "Specific/level.h"

using namespace TEN::Collision::Room;
using namespace TEN::Collision::Sphere;
using namespace TEN::Gui;
using namespace TEN::Input;
using namespace TEN::Physics;

namespace TEN::Entities::Doors
{
	const auto CrowbarDoorPos = Vector3i(-412, 0, 112);
	const ObjectCollisionBounds CrowbarDoorBounds =
	{
		GameBoundingBox(
			-BLOCK(0.5f), BLOCK(0.5f),
			-BLOCK(1), 0,
			0, BLOCK(0.5f)),
		std::pair(
			EulerAngles(ANGLE(-80.0f), ANGLE(-80.0f), ANGLE(-80.0f)),
			EulerAngles(ANGLE(80.0f), ANGLE(80.0f), ANGLE(80.0f)))
	};

	const DOOR_DATA& GetDoorObject(const ItemInfo& item)  
	{  
	   return (DOOR_DATA&)item.Data;  
	}

	DOOR_DATA& GetDoorObject(ItemInfo& item)
	{
		return (DOOR_DATA&)item.Data;
	}

	void InitializeDoor(short itemNumber)
	{
		auto& doorItem = g_Level.Items[itemNumber];

		if (doorItem.ObjectNumber == ID_SEQUENCE_DOOR1)
			doorItem.Flags &= 0xBFFFu;

		if (doorItem.ObjectNumber == ID_LIFT_DOORS1 || doorItem.ObjectNumber == ID_LIFT_DOORS2)
			doorItem.ItemFlags[0] = 4096;

		doorItem.Data = ItemData(DOOR_DATA());
		auto& door = GetDoorObject(doorItem);

		door.opened = false;
		door.dptr1 = nullptr;
		door.dptr2 = nullptr;
		door.dptr3 = nullptr;
		door.dptr4 = nullptr;

		short boxNumber, twoRoom;

		int xOffset = 0;
		int zOffset = 0;

		if (doorItem.Pose.Orientation.y == 0)
		{
			zOffset = -BLOCK(1);
		}
		else if (doorItem.Pose.Orientation.y == ANGLE(180.0f))
		{
			zOffset = BLOCK(1);
		}
		else if (doorItem.Pose.Orientation.y == ANGLE(90.0f))
		{
			xOffset = -BLOCK(1);
		}
		else
		{
			xOffset = BLOCK(1);
		}

		auto* room = &g_Level.Rooms[doorItem.RoomNumber];
		door.d1.floor = GetSector(room, doorItem.Pose.Position.x - room->Position.x + xOffset, doorItem.Pose.Position.z - room->Position.z + zOffset);

		auto roomNumber = door.d1.floor->SidePortalRoomNumber;
		if (roomNumber == NO_VALUE)
		{
			boxNumber = door.d1.floor->PathfindingBoxID;
		}
		else
		{
			auto* b = &g_Level.Rooms[roomNumber];
			boxNumber = GetSector(b, doorItem.Pose.Position.x - b->Position.x + xOffset, doorItem.Pose.Position.z - b->Position.z + zOffset)->PathfindingBoxID;
		}

		door.d1.block = (boxNumber != NO_VALUE && g_Level.PathfindingBoxes[boxNumber].flags & BLOCKABLE) ? boxNumber : NO_VALUE; 
		door.d1.data = *door.d1.floor;

		if (room->flippedRoom != NO_VALUE)
		{
			room = &g_Level.Rooms[room->flippedRoom];
			door.d1flip.floor = GetSector(room, doorItem.Pose.Position.x - room->Position.x + xOffset, doorItem.Pose.Position.z - room->Position.z + zOffset);
				
			roomNumber = door.d1flip.floor->SidePortalRoomNumber;
			if (roomNumber == NO_VALUE)
			{
				boxNumber = door.d1flip.floor->PathfindingBoxID;
			}
			else
			{
				auto* b = &g_Level.Rooms[roomNumber];
				boxNumber = GetSector(b, doorItem.Pose.Position.x - b->Position.x + xOffset, doorItem.Pose.Position.z - b->Position.z + zOffset)->PathfindingBoxID;
			}

			door.d1flip.block = (boxNumber != NO_VALUE && g_Level.PathfindingBoxes[boxNumber].flags & BLOCKABLE) ? boxNumber : NO_VALUE;
			door.d1flip.data = *door.d1flip.floor;
		}
		else
		{
			door.d1flip.floor = nullptr;
		}

		twoRoom = door.d1.floor->SidePortalRoomNumber;

		ShutThatDoor(&door.d1, &door);
		ShutThatDoor(&door.d1flip, &door);

		if (twoRoom == NO_VALUE)
		{
			door.d2.floor = nullptr;
			door.d2flip.floor = nullptr;
		}
		else
		{
			room = &g_Level.Rooms[twoRoom];
			door.d2.floor = GetSector(room, doorItem.Pose.Position.x - room->Position.x, doorItem.Pose.Position.z - room->Position.z);

			roomNumber = door.d2.floor->SidePortalRoomNumber;
			if (roomNumber == NO_VALUE)
			{
				boxNumber = door.d2.floor->PathfindingBoxID;
			}
			else
			{
				auto* b = &g_Level.Rooms[roomNumber];
				boxNumber = GetSector(b, doorItem.Pose.Position.x - b->Position.x, doorItem.Pose.Position.z - b->Position.z)->PathfindingBoxID;
			}

			door.d2.block = (boxNumber != NO_VALUE && g_Level.PathfindingBoxes[boxNumber].flags & BLOCKABLE) ? boxNumber : NO_VALUE;
			door.d2.data = *door.d2.floor;

			if (room->flippedRoom != -1)
			{
				room = &g_Level.Rooms[room->flippedRoom];
				door.d2flip.floor = GetSector(room, doorItem.Pose.Position.x - room->Position.x, doorItem.Pose.Position.z - room->Position.z);

				roomNumber = door.d2flip.floor->SidePortalRoomNumber;
				if (roomNumber == NO_VALUE)
				{
					boxNumber = door.d2flip.floor->PathfindingBoxID;
				}
				else
				{
					auto* b = &g_Level.Rooms[roomNumber];
					boxNumber = GetSector(b, doorItem.Pose.Position.x - b->Position.x, doorItem.Pose.Position.z - b->Position.z)->PathfindingBoxID;
				}

				door.d2flip.block = (boxNumber != NO_VALUE && g_Level.PathfindingBoxes[boxNumber].flags & BLOCKABLE) ? boxNumber : NO_VALUE; 
				door.d2flip.data = *door.d2flip.floor;
			}
			else
			{
				door.d2flip.floor = nullptr;
			}

			ShutThatDoor(&door.d2, &door);
			ShutThatDoor(&door.d2flip, &door);

			roomNumber = doorItem.RoomNumber;
			ItemNewRoom(itemNumber, twoRoom);
			doorItem.RoomNumber = roomNumber;
			doorItem.InDrawRoom = true;
		}

		// Get sector AABB corners. NOTE: Doors in flip rooms use mesh generated in original room.
		auto corners = std::array<Vector3, BoundingBox::CORNER_COUNT>{};
		if (door.d2.floor != nullptr)
		{
			door.d2.floor->Aabb.GetCorners(corners.data());

			auto quadrant = GetQuadrant(doorItem.Pose.Orientation.y);

			// Set initial bound.
			float bound = 0.0f;
			switch (quadrant)
			{
				case CardinalDirection::NORTH:
				case CardinalDirection::EAST:
					bound = FLT_MAX;
					break;

				case CardinalDirection::SOUTH:
				case CardinalDirection::WEST:
					bound = -FLT_MAX;
					break;
			}

			// Determine bound for flattened AABB.
			for (auto& corner : corners)
			{
				switch (quadrant)
				{
					case CardinalDirection::NORTH:
						bound = std::min(bound, corner.z);
						break;

					case CardinalDirection::SOUTH:
						bound = std::max(bound, corner.z);
						break;

					case CardinalDirection::EAST:
						bound = std::min(bound, corner.x);
						break;

					case CardinalDirection::WEST:
						bound = std::max(bound, corner.x);
						break;
				}
			}

			// Flatten box corners for door at portal.
			for (auto& corner : corners)
			{
				switch (quadrant)
				{
					case CardinalDirection::NORTH:
					case CardinalDirection::SOUTH:
						corner.z = bound;
						break;

					case CardinalDirection::EAST:
					case CardinalDirection::WEST:
						corner.x = bound;
						break;
				}
			}
		}
		else
		{
			door.d1.floor->Aabb.GetCorners(corners.data());
		}

		// Set collision mesh.
		auto desc = CollisionMeshDesc();
		desc.InsertTriangle(corners[4], corners[1], corners[0]);
		desc.InsertTriangle(corners[1], corners[4], corners[5]);
		desc.InsertTriangle(corners[6], corners[3], corners[2]);
		desc.InsertTriangle(corners[3], corners[6], corners[7]);
		desc.InsertTriangle(corners[0], corners[1], corners[2]);
		desc.InsertTriangle(corners[0], corners[2], corners[3]);
		desc.InsertTriangle(corners[6], corners[5], corners[4]);
		desc.InsertTriangle(corners[7], corners[6], corners[4]);
		desc.InsertTriangle(corners[0], corners[3], corners[4]);
		desc.InsertTriangle(corners[7], corners[4], corners[3]);
		desc.InsertTriangle(corners[5], corners[2], corners[1]);
		desc.InsertTriangle(corners[2], corners[5], corners[6]);
		door.CollisionMesh = CollisionMesh(Vector3::Zero, Quaternion::Identity, desc);

		EnableDoorCollisionMesh(doorItem);
	}

	void DoorCollision(short itemNumber, ItemInfo* playerItem, CollisionInfo* coll)
	{
		auto& doorItem = g_Level.Items[itemNumber];
		auto& player = GetLaraInfo(*playerItem);

		if (doorItem.TriggerFlags == 2 &&
			doorItem.Status == ITEM_NOT_ACTIVE && !doorItem.Animation.IsAirborne && // CHECK
			((IsHeld(In::Action) || g_Gui.GetInventoryItemChosen() == ID_CROWBAR_ITEM) &&
				playerItem->Animation.ActiveState == LS_IDLE &&
				playerItem->Animation.AnimNumber == LA_STAND_IDLE &&
				!playerItem->HitStatus &&
				player.Control.HandStatus == HandStatus::Free ||
				player.Control.IsMoving && player.Context.InteractedItem == itemNumber))
		{
			doorItem.Pose.Orientation.y ^= ANGLE(180.0f);
			if (TestLaraPosition(CrowbarDoorBounds, &doorItem, playerItem))
			{
				if (!player.Control.IsMoving)
				{
					if (g_Gui.GetInventoryItemChosen() == NO_VALUE)
					{
						if (g_Gui.IsObjectInInventory(ID_CROWBAR_ITEM))
						{
							g_Gui.SetEnterInventory(ID_CROWBAR_ITEM);
							doorItem.Pose.Orientation.y ^= ANGLE(180.0f);
						}
						else
						{
							if (OldPickupPos.x != playerItem->Pose.Position.x || OldPickupPos.y != playerItem->Pose.Position.y || OldPickupPos.z != playerItem->Pose.Position.z)
							{
								OldPickupPos.x = playerItem->Pose.Position.x;
								OldPickupPos.y = playerItem->Pose.Position.y;
								OldPickupPos.z = playerItem->Pose.Position.z;
								SayNo();
							}

							doorItem.Pose.Orientation.y ^= ANGLE(180.0f);
						}

						return;
					}

					if (g_Gui.GetInventoryItemChosen() != ID_CROWBAR_ITEM)
					{
						doorItem.Pose.Orientation.y ^= ANGLE(180.0f);
						return;
					}
				}

				g_Gui.SetInventoryItemChosen(NO_VALUE);

				if (MoveLaraPosition(CrowbarDoorPos, &doorItem, playerItem))
				{
					SetAnimation(playerItem, LA_DOOR_OPEN_CROWBAR);
					doorItem.Pose.Orientation.y ^= ANGLE(180.0f);

					AddActiveItem(itemNumber);

					player.Control.IsMoving = 0;
					player.Control.HandStatus = HandStatus::Busy;
					doorItem.Flags |= IFLAG_ACTIVATION_MASK;
					doorItem.Status = ITEM_ACTIVE;
					doorItem.Animation.TargetState = LS_RUN_FORWARD;
					return;
				}

				player.Context.InteractedItem = itemNumber;
			}
			else if (player.Control.IsMoving && player.Context.InteractedItem == itemNumber)
			{
				player.Control.IsMoving = 0;
				player.Control.HandStatus = HandStatus::Free;
			}

			doorItem.Pose.Orientation.y ^= ANGLE(180.0f);
		}

		if (TestBoundsCollide(&doorItem, playerItem, coll->Setup.Radius))
		{
			if (HandleItemSphereCollision(doorItem, *playerItem))
			{
				if (coll->Setup.EnableObjectPush)
				{
					if (!(doorItem.ObjectNumber >= ID_LIFT_DOORS1 &&
						doorItem.ObjectNumber <= ID_LIFT_DOORS2) || doorItem.ItemFlags[0])
					{
						ItemPushItem(&doorItem, playerItem, coll, false, 1);
					}
				}
			}
		}
	}

	void DoorControl(short itemNumber)
	{
		auto& doorItem = g_Level.Items[itemNumber];
		auto& door = GetDoorObject(doorItem);

		// Doors with OCB = 1 are raisable with cog switchs
		if (doorItem.TriggerFlags == 1)
		{
			if (doorItem.ItemFlags[0])
			{
				auto bounds = GameBoundingBox(&doorItem);
			
				doorItem.ItemFlags[0]--;
				doorItem.Pose.Position.y -= TEN::Entities::Switches::COG_DOOR_SPEED;
				
				int y = bounds.Y1 + doorItem.ItemFlags[2] - CLICK(1);
				if (doorItem.Pose.Position.y < y)
				{
					doorItem.Pose.Position.y = y;
					doorItem.ItemFlags[0] = 0;
				}
				if (!door.opened)
				{
					OpenThatDoor(&door.d1, &door);
					OpenThatDoor(&door.d2, &door);
					OpenThatDoor(&door.d1flip, &door);
					OpenThatDoor(&door.d2flip, &door);
					DisableDoorCollisionMesh(doorItem);
					door.opened = true;
				}
			}
			else
			{
				if (doorItem.Pose.Position.y < doorItem.StartPose.Position.y)
					doorItem.Pose.Position.y += 4;
				if (doorItem.Pose.Position.y >= doorItem.StartPose.Position.y)
				{
					doorItem.Pose.Position.y = doorItem.StartPose.Position.y;
					if (door.opened)
					{
						ShutThatDoor(&door.d1, &door);
						ShutThatDoor(&door.d2, &door);
						ShutThatDoor(&door.d1flip, &door);
						ShutThatDoor(&door.d2flip, &door);
						EnableDoorCollisionMesh(doorItem);
						door.opened = false;
					}
				}
			}

			return;
		}

		if (doorItem.ObjectNumber < ID_LIFT_DOORS1 || doorItem.ObjectNumber > ID_LIFT_DOORS2)
		{
			if (TriggerActive(&doorItem))
			{
				if (doorItem.Animation.ActiveState == 0)
					doorItem.Animation.TargetState = 1;
				else if (!door.opened)
				{
					OpenThatDoor(&door.d1, &door);
					OpenThatDoor(&door.d2, &door);
					OpenThatDoor(&door.d1flip, &door);
					OpenThatDoor(&door.d2flip, &door);
					DisableDoorCollisionMesh(doorItem);
					door.opened = true;
				}
			}
			else
			{
				doorItem.Status = ITEM_ACTIVE;

				if (doorItem.Animation.ActiveState == 1)
					doorItem.Animation.TargetState = 0;
				else if (door.opened)
				{
					ShutThatDoor(&door.d1, &door);
					ShutThatDoor(&door.d2, &door);
					ShutThatDoor(&door.d1flip, &door);
					ShutThatDoor(&door.d2flip, &door);
					EnableDoorCollisionMesh(doorItem);
					door.opened = false;
				}
			}
		}

		AnimateItem(&doorItem);
	}

	void OpenThatDoor(DOORPOS_DATA* doorPos, DOOR_DATA* dd)
	{
		auto* sector = doorPos->floor;
		if (sector == nullptr)
			return;

		*doorPos->floor = doorPos->data;

		int pathfindingBoxID = doorPos->block;
		if (pathfindingBoxID != NO_VALUE)
		{
			g_Level.PathfindingBoxes[pathfindingBoxID].flags &= ~BLOCKED;
			for (auto& creature : ActiveCreatures)
				creature->LOT.TargetBox = NO_VALUE;
		}
	}

	void ShutThatDoor(DOORPOS_DATA* doorPos, DOOR_DATA* dd)
	{
		static const auto WALL_PLANE = Plane(-Vector3::UnitY, (float)NO_HEIGHT);

		auto* sector = doorPos->floor;
		if (sector == nullptr)
			return;

		sector->PathfindingBoxID = NO_VALUE;
		sector->TriggerIndex = 0;

		// FIXME: HACK!!!!!!!
		// Find a better way to deal with doors.

		sector->SidePortalRoomNumber = NO_VALUE;
		sector->FloorSurface.Triangles[0].PortalRoomNumber =
		sector->FloorSurface.Triangles[1].PortalRoomNumber =
		sector->CeilingSurface.Triangles[0].PortalRoomNumber =
		sector->CeilingSurface.Triangles[1].PortalRoomNumber = NO_VALUE;
		sector->FloorSurface.Triangles[0].Plane =
		sector->FloorSurface.Triangles[1].Plane =
		sector->CeilingSurface.Triangles[0].Plane =
		sector->CeilingSurface.Triangles[1].Plane = WALL_PLANE;

		int pathfindingBoxID = doorPos->block;
		if (pathfindingBoxID != NO_VALUE)
		{
			g_Level.PathfindingBoxes[pathfindingBoxID].flags |= BLOCKED;

			for (auto& creature : ActiveCreatures)
				creature->LOT.TargetBox = NO_VALUE;
		}
	}

	void EnableDoorCollisionMesh(const ItemInfo& item)
	{
		const auto& door = GetDoorObject(item);

		auto& room = g_Level.Rooms[(door.d2.floor != nullptr) ? door.d2.floor->RoomNumber : door.d1.floor->RoomNumber];
		room.Doors.Insert(item.Index, (door.d2.floor != nullptr) ? door.d2.floor->Aabb : door.d1.floor->Aabb);

		if (door.d2flip.floor != nullptr)
		{
			auto& flipRoom = g_Level.Rooms[door.d2flip.floor->RoomNumber];
			flipRoom.Doors.Insert(item.Index, (door.d2.floor != nullptr) ? door.d2.floor->Aabb : door.d1.floor->Aabb);
		}
		else if (door.d1flip.floor != nullptr)
		{
			auto& flipRoom = g_Level.Rooms[door.d1flip.floor->RoomNumber];
			flipRoom.Doors.Insert(item.Index, (door.d2.floor != nullptr) ? door.d2.floor->Aabb : door.d1.floor->Aabb);
		}
	}

	void DisableDoorCollisionMesh(const ItemInfo& item)
	{
		const auto& door = GetDoorObject(item);

		auto& room = g_Level.Rooms[(door.d2.floor != nullptr) ? door.d2.floor->RoomNumber : door.d1.floor->RoomNumber];
		room.Doors.Remove(item.Index);
		
		if (door.d2flip.floor != nullptr)
		{
			auto& flipRoom = g_Level.Rooms[door.d2flip.floor->RoomNumber];
			flipRoom.Doors.Remove(item.Index);
		}
		else if (door.d1flip.floor != nullptr)
		{
			auto& flipRoom = g_Level.Rooms[door.d1flip.floor->RoomNumber];
			flipRoom.Doors.Remove(item.Index);
		}
	}
}
