#pragma once

struct CollisionInfo;
struct ItemInfo;

namespace TEN::Entities::Generic
{
	void InitializeTrapDoor(short itemNumber);
	void TrapDoorCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll);
	void CeilingTrapDoorCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll);
	void FloorTrapDoorCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll);
	void TrapDoorControl(short itemNumber);
	void CloseTrapDoor(short itemNumber);
	void OpenTrapDoor(short itemNumber);
}
