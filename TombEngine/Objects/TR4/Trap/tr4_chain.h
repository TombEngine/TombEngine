#pragma once

struct CollisionInfo;
struct ItemInfo;

namespace TEN::Entities::Traps
{
	void ControlChain(short itemNumber);
	void CollideChain(short itemNumber, ItemInfo* playerItem, CollisionInfo* coll);
	void TriggerPendulumFlame(int itemNumber, Vector3i pos, Color color);
	void TriggerPendulumSpark(const GameVector& pos, const EulerAngles& angle, float length, int count, Color color);
}
