#pragma once

struct CollisionInfo;
struct ItemInfo;
struct ObjectInfo;

namespace TEN::Entities::Switches
{
	void SetupJumpSwitch(ObjectInfo& object);
	void JumpSwitchCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll);
}
