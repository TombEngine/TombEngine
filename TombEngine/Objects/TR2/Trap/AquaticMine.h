#pragma once

class Vector3i;
struct CollisionInfo;
struct ItemInfo;

namespace TEN::Entities::Traps
{
	void InitializeAquaticMine(short itemNumber);
	void ControlAquaticMine(short itemNumber);
	void CollideAquaticMine(short itemNumber, ItemInfo* playerItem, CollisionInfo* coll);
	void TriggerMineExplosion(short itemNumber);
	void TriggerNearbyMines(const Vector3i& position);
}
