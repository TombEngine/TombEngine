#pragma once

struct CollisionInfo;
struct ItemInfo;

namespace TEN::Entities::Traps
{
	void ControlTunnelBorer(short itemNumber);
	void CollideTunnelBorer(short itemNumber, ItemInfo* item, CollisionInfo* coll);
}
