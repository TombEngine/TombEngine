#pragma once

struct CollisionInfo;
struct ItemInfo;

namespace TEN::Entities::Traps
{
	void InitializeCircularSaw(short itemNumber);
	void ControlCircularSaw(short itemNumber);
	void CollideCircularSaw(short itemNumber, ItemInfo* playerItem, CollisionInfo* coll);
	void TriggerSawSparkles(ItemInfo* item);
	void TriggerSawSpark(const GameVector& pos, const EulerAngles& angle, int count, const Vector4& colorStart);
}
