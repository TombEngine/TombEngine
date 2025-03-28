#pragma once

struct CollisionInfo;
struct DOORPOS_DATA;
struct DOOR_DATA;
struct ItemInfo;
struct RoomData;

namespace TEN::Entities::Doors
{
	void InitializeDoor(short itemNumber);
	void DoorCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll);
	void DoorControl(short itemNumber);

	void OpenThatDoor(DOORPOS_DATA* doorPos, DOOR_DATA* door);
	void ShutThatDoor(DOORPOS_DATA* doorPos, DOOR_DATA* door);
	void UpdateDoorRoomCollisionMeshes(const DOOR_DATA& door);
}
