#pragma once

struct CollisionInfo;
struct ItemInfo;

namespace TEN::Entities::Doors
{
	void PushPullKickDoorControl(short itemNumber);
	void PushPullKickDoorCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll);
}
