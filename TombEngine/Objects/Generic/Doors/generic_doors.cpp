#include "framework.h"
#include "Objects/Generic/Doors/generic_doors.h"
#include "Specific/level.h"
#include "Game/control/control.h"
#include "Game/control/box.h"
#include "Game/items.h"
#include "Game/control/lot.h"
#include "Game/Gui.h"
#include "Specific/Input/Input.h"
#include "Game/pickup/pickup.h"
#include "Sound/sound.h"
#include "Game/animation.h"
#include "Game/collision/Sphere.h"
#include "Objects/Generic/Switches/cog_switch.h"
#include "Objects/objectslist.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Lara/lara_struct.h"
#include "Math/Math.h"
#include "Game/misc.h"
#include "Game/itemdata/door_data.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/collide_item.h"
#include "Game/itemdata/itemdata.h"

using namespace TEN::Collision::Room;
using namespace TEN::Collision::Sphere;
using namespace TEN::Gui;
using namespace TEN::Input;

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

	void InitializeDoor(short itemNumber)
	{
		auto* doorItem = &g_Level.Items[itemNumber];

		if (doorItem->ObjectNumber == ID_SEQUENCE_DOOR1)
			doorItem->Flags &= 0xBFFFu;

		if (doorItem->ObjectNumber == ID_LIFT_DOORS1 || doorItem->ObjectNumber == ID_LIFT_DOORS2)
			doorItem->ItemFlags[0] = 4096;

		doorItem->Data = ItemData(DOOR_DATA());
		auto* doorData = (DOOR_DATA*)doorItem->Data;

		doorData->opened = false;
		doorData->dptr1 = nullptr;
		doorData->dptr2 = nullptr;
		doorData->dptr3 = nullptr;
		doorData->dptr4 = nullptr;

		short boxNumber, twoRoom;

		int xOffset = 0;
		int zOffset = 0;

		if (doorItem->Pose.Orientation.y == 0)
			zOffset = -BLOCK(1);
		else if (doorItem->Pose.Orientation.y == ANGLE(180.0f))
			zOffset = BLOCK(1);
		else if (doorItem->Pose.Orientation.y == ANGLE(90.0f))
			xOffset = -BLOCK(1);
		else
			xOffset = BLOCK(1);

		auto* r = &g_Level.Rooms[doorItem->RoomNumber];
		doorData->d1.floor = GetSector(r, doorItem->Pose.Position.x - r->Position.x + xOffset, doorItem->Pose.Position.z - r->Position.z + zOffset);

		auto roomNumber = doorData->d1.floor->SidePortalRoomNumber;
		if (roomNumber == NO_VALUE)
			boxNumber = doorData->d1.floor->PathfindingBoxID;
		else
		{
			auto* b = &g_Level.Rooms[roomNumber];
			boxNumber = GetSector(b, doorItem->Pose.Position.x - b->Position.x + xOffset, doorItem->Pose.Position.z - b->Position.z + zOffset)->PathfindingBoxID;
		}

		doorData->d1.block = (boxNumber != NO_VALUE && g_Level.PathfindingBoxes[boxNumber].flags & BLOCKABLE) ? boxNumber : NO_VALUE; 
		doorData->d1.data = *doorData->d1.floor;

		if (r->flippedRoom != NO_VALUE)
		{
			r = &g_Level.Rooms[r->flippedRoom];
			doorData->d1flip.floor = GetSector(r, doorItem->Pose.Position.x - r->Position.x + xOffset, doorItem->Pose.Position.z - r->Position.z + zOffset);
				
			roomNumber = doorData->d1flip.floor->SidePortalRoomNumber;
			if (roomNumber == NO_VALUE)
				boxNumber = doorData->d1flip.floor->PathfindingBoxID;
			else
			{
				auto* b = &g_Level.Rooms[roomNumber];
				boxNumber = GetSector(b, doorItem->Pose.Position.x - b->Position.x + xOffset, doorItem->Pose.Position.z - b->Position.z + zOffset)->PathfindingBoxID;
			}

			doorData->d1flip.block = (boxNumber != NO_VALUE && g_Level.PathfindingBoxes[boxNumber].flags & BLOCKABLE) ? boxNumber : NO_VALUE;
			doorData->d1flip.data = *doorData->d1flip.floor;
		}
		else
			doorData->d1flip.floor = NULL;

		twoRoom = doorData->d1.floor->SidePortalRoomNumber;

		ShutThatDoor(&doorData->d1, doorData);
		ShutThatDoor(&doorData->d1flip, doorData);

		if (twoRoom == NO_VALUE)
		{
			doorData->d2.floor = NULL;
			doorData->d2flip.floor = NULL;
		}
		else
		{
			r = &g_Level.Rooms[twoRoom];
			doorData->d2.floor = GetSector(r, doorItem->Pose.Position.x - r->Position.x, doorItem->Pose.Position.z - r->Position.z);

			roomNumber = doorData->d2.floor->SidePortalRoomNumber;
			if (roomNumber == NO_VALUE)
				boxNumber = doorData->d2.floor->PathfindingBoxID;
			else
			{
				auto* b = &g_Level.Rooms[roomNumber];
				boxNumber = GetSector(b, doorItem->Pose.Position.x - b->Position.x, doorItem->Pose.Position.z - b->Position.z)->PathfindingBoxID;
			}

			doorData->d2.block = (boxNumber != NO_VALUE && g_Level.PathfindingBoxes[boxNumber].flags & BLOCKABLE) ? boxNumber : NO_VALUE;
			doorData->d2.data = *doorData->d2.floor;

			if (r->flippedRoom != -1)
			{
				r = &g_Level.Rooms[r->flippedRoom];
				doorData->d2flip.floor = GetSector(r, doorItem->Pose.Position.x - r->Position.x, doorItem->Pose.Position.z - r->Position.z);

				roomNumber = doorData->d2flip.floor->SidePortalRoomNumber;
				if (roomNumber == NO_VALUE)
					boxNumber = doorData->d2flip.floor->PathfindingBoxID;
				else
				{
					auto* b = &g_Level.Rooms[roomNumber];
					boxNumber = GetSector(b, doorItem->Pose.Position.x - b->Position.x, doorItem->Pose.Position.z - b->Position.z)->PathfindingBoxID;
				}

				doorData->d2flip.block = (boxNumber != NO_VALUE && g_Level.PathfindingBoxes[boxNumber].flags & BLOCKABLE) ? boxNumber : NO_VALUE; 
				doorData->d2flip.data = *doorData->d2flip.floor;
			}
			else
				doorData->d2flip.floor = NULL;

			ShutThatDoor(&doorData->d2, doorData);
			ShutThatDoor(&doorData->d2flip, doorData);

			roomNumber = doorItem->RoomNumber;
			ItemNewRoom(itemNumber, twoRoom);
			doorItem->RoomNumber = roomNumber;
			doorItem->InDrawRoom = true;
		}

		UpdateDoorRoomCollisionMeshes(*doorData);
	}

	void DoorCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll)
	{
		auto* laraInfo = GetLaraInfo(laraItem);
		auto* doorItem = &g_Level.Items[itemNumber];

		if (doorItem->TriggerFlags == 2 &&
			doorItem->Status == ITEM_NOT_ACTIVE && !doorItem->Animation.IsAirborne && // CHECK
			((IsHeld(In::Action) || g_Gui.GetInventoryItemChosen() == ID_CROWBAR_ITEM) &&
				laraItem->Animation.ActiveState == LS_IDLE &&
				laraItem->Animation.AnimNumber == LA_STAND_IDLE &&
				!laraItem->HitStatus &&
				laraInfo->Control.HandStatus == HandStatus::Free ||
				laraInfo->Control.IsMoving && laraInfo->Context.InteractedItem == itemNumber))
		{
			doorItem->Pose.Orientation.y ^= ANGLE(180.0f);
			if (TestLaraPosition(CrowbarDoorBounds, doorItem, laraItem))
			{
				if (!laraInfo->Control.IsMoving)
				{
					if (g_Gui.GetInventoryItemChosen() == NO_VALUE)
					{
						if (g_Gui.IsObjectInInventory(ID_CROWBAR_ITEM))
						{
							g_Gui.SetEnterInventory(ID_CROWBAR_ITEM);
							doorItem->Pose.Orientation.y ^= ANGLE(180.0f);
						}
						else
						{
							if (OldPickupPos.x != laraItem->Pose.Position.x || OldPickupPos.y != laraItem->Pose.Position.y || OldPickupPos.z != laraItem->Pose.Position.z)
							{
								OldPickupPos.x = laraItem->Pose.Position.x;
								OldPickupPos.y = laraItem->Pose.Position.y;
								OldPickupPos.z = laraItem->Pose.Position.z;
								SayNo();
							}

							doorItem->Pose.Orientation.y ^= ANGLE(180.0f);
						}

						return;
					}

					if (g_Gui.GetInventoryItemChosen() != ID_CROWBAR_ITEM)
					{
						doorItem->Pose.Orientation.y ^= ANGLE(180.0f);
						return;
					}
				}

				g_Gui.SetInventoryItemChosen(NO_VALUE);

				if (MoveLaraPosition(CrowbarDoorPos, doorItem, laraItem))
				{
					SetAnimation(laraItem, LA_DOOR_OPEN_CROWBAR);
					doorItem->Pose.Orientation.y ^= ANGLE(180.0f);

					AddActiveItem(itemNumber);

					laraInfo->Control.IsMoving = 0;
					laraInfo->Control.HandStatus = HandStatus::Busy;
					doorItem->Flags |= IFLAG_ACTIVATION_MASK;
					doorItem->Status = ITEM_ACTIVE;
					doorItem->Animation.TargetState = LS_RUN_FORWARD;
					return;
				}

				laraInfo->Context.InteractedItem = itemNumber;
			}
			else if (laraInfo->Control.IsMoving && laraInfo->Context.InteractedItem == itemNumber)
			{
				laraInfo->Control.IsMoving = 0;
				laraInfo->Control.HandStatus = HandStatus::Free;
			}

			doorItem->Pose.Orientation.y ^= ANGLE(180.0f);
		}

		if (TestBoundsCollide(doorItem, laraItem, coll->Setup.Radius))
		{
			if (HandleItemSphereCollision(*doorItem, *laraItem))
			{
				if (coll->Setup.EnableObjectPush)
				{
					if (!(doorItem->ObjectNumber >= ID_LIFT_DOORS1 &&
						doorItem->ObjectNumber <= ID_LIFT_DOORS2) || doorItem->ItemFlags[0])
					{
						ItemPushItem(doorItem, laraItem, coll, false, 1);
					}
				}
			}
		}
	}

	void DoorControl(short itemNumber)
	{
		auto* doorItem = &g_Level.Items[itemNumber];
		auto* doorData = (DOOR_DATA*)doorItem->Data;

		// Doors with OCB = 1 are raisable with cog switchs
		if (doorItem->TriggerFlags == 1)
		{
			if (doorItem->ItemFlags[0])
			{
				auto bounds = GameBoundingBox(doorItem);
			
				doorItem->ItemFlags[0]--;
				doorItem->Pose.Position.y -= TEN::Entities::Switches::COG_DOOR_SPEED;
				
				int y = bounds.Y1 + doorItem->ItemFlags[2] - CLICK(1);
				if (doorItem->Pose.Position.y < y)
				{
					doorItem->Pose.Position.y = y;
					doorItem->ItemFlags[0] = 0;
				}
				if (!doorData->opened)
				{
					OpenThatDoor(&doorData->d1, doorData);
					OpenThatDoor(&doorData->d2, doorData);
					OpenThatDoor(&doorData->d1flip, doorData);
					OpenThatDoor(&doorData->d2flip, doorData);
					doorData->opened = true;

					UpdateDoorRoomCollisionMeshes(*doorData);
				}
			}
			else
			{
				if (doorItem->Pose.Position.y < doorItem->StartPose.Position.y)
					doorItem->Pose.Position.y += 4;
				if (doorItem->Pose.Position.y >= doorItem->StartPose.Position.y)
				{
					doorItem->Pose.Position.y = doorItem->StartPose.Position.y;
					if (doorData->opened)
					{
						ShutThatDoor(&doorData->d1, doorData);
						ShutThatDoor(&doorData->d2, doorData);
						ShutThatDoor(&doorData->d1flip, doorData);
						ShutThatDoor(&doorData->d2flip, doorData);
						doorData->opened = false;

						UpdateDoorRoomCollisionMeshes(*doorData);
					}
				}
			}

			return;
		}

		if (doorItem->ObjectNumber < ID_LIFT_DOORS1 || doorItem->ObjectNumber > ID_LIFT_DOORS2)
		{
			if (TriggerActive(doorItem))
			{
				if (doorItem->Animation.ActiveState == 0)
					doorItem->Animation.TargetState = 1;
				else if (!doorData->opened)
				{
					OpenThatDoor(&doorData->d1, doorData);
					OpenThatDoor(&doorData->d2, doorData);
					OpenThatDoor(&doorData->d1flip, doorData);
					OpenThatDoor(&doorData->d2flip, doorData);
					doorData->opened = true;

					UpdateDoorRoomCollisionMeshes(*doorData);
				}
			}
			else
			{
				doorItem->Status = ITEM_ACTIVE;

				if (doorItem->Animation.ActiveState == 1)
					doorItem->Animation.TargetState = 0;
				else if (doorData->opened)
				{
					ShutThatDoor(&doorData->d1, doorData);
					ShutThatDoor(&doorData->d2, doorData);
					ShutThatDoor(&doorData->d1flip, doorData);
					ShutThatDoor(&doorData->d2flip, doorData);
					doorData->opened = false;

					UpdateDoorRoomCollisionMeshes(*doorData);
				}
			}
		}
		else
		{
			// TR5 lift doors
			/*if (!TriggerActive(item))
			{
				if (item->itemFlags[0] >= BLOCK(4))
				{
					if (door->opened)
					{
						ShutThatDoor(&door->d1, door);
						ShutThatDoor(&door->d2, door);
						ShutThatDoor(&door->d1flip, door);
						ShutThatDoor(&door->d2flip, door);
						door->opened = false;

						UpdateDoorRoomCollisionMeshes(*door);
					}
				}
				else
				{
					if (!item->itemFlags[0])
						SoundEffect(SFX_TR5_LIFT_DOORS, &item->pos);
					item->itemFlags[0] += CLICK(1);
				}
			}
			else
			{
				if (item->itemFlags[0] > 0)
				{
					if (item->itemFlags[0] == BLOCK(4))
						SoundEffect(SFX_TR5_LIFT_DOORS, &item->pos);
					item->itemFlags[0] -= CLICK(1);
				}
				if (!door->opened)
				{
					DontUnlockBox = true;
					OpenThatDoor(&door->d1, door);
					OpenThatDoor(&door->d2, door);
					OpenThatDoor(&door->d1flip, door);
					OpenThatDoor(&door->d2flip, door);
					DontUnlockBox = false;
					door->opened = true;

					UpdateDoorRoomCollisionMeshes(*door);
				}
			}*/
		}

		AnimateItem(doorItem);
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

	// HACK: Regenerate room collision meshes.
	void UpdateDoorRoomCollisionMeshes(const DOOR_DATA& door)
	{
		// Generate current room collision mesh.
		if (door.d1.floor != nullptr)
		{
			auto& room = g_Level.Rooms[door.d1.floor->RoomNumber];
			room.GenerateCollisionMesh();
		}

		// Generate neighbor room collision mesh.
		if (door.d2.floor != nullptr)
		{
			auto& room = g_Level.Rooms[door.d2.floor->RoomNumber];
			room.GenerateCollisionMesh();
		}
	}
}
