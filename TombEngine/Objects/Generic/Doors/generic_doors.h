#pragma once

#include "Objects/game_object_ids.h"

struct CollisionInfo;
struct DOORPOS_DATA;
struct DOOR_DATA;
struct ItemInfo;
struct RoomData;

namespace TEN::Entities::Doors
{
	const DOOR_DATA& GetDoorObject(const ItemInfo& item);
	DOOR_DATA&		 GetDoorObject(ItemInfo& item);

	bool IsDoorObject(GAME_OBJECT_ID objectID);

	void InitializeDoor(short itemNumber);
	void DoorCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll);
	void DoorControl(short itemNumber);

	void OpenThatDoor(DOORPOS_DATA* doorPos, DOOR_DATA* door);
	void ShutThatDoor(DOORPOS_DATA* doorPos, DOOR_DATA* door);

	void EnableDoorCollisionMesh(const ItemInfo& item);
	void DisableDoorCollisionMesh(const ItemInfo& item);
}
