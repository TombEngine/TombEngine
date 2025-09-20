#pragma once

struct ItemInfo;
struct CollisionInfo;
struct ObjectCollisionBounds;

namespace TEN::Entities::Effects
{
	extern ObjectCollisionBounds FireBounds;

	void InitializeFlameEmitter(short itemNumber);
	void InitializeFlameEmitter2(short itemNumber);
	void InitializeFlameEmitter3(short itemNumber);
	void FlameEmitterControl(short itemNumber);
	void FlameEmitter2Control(short itemNumber);
	void FlameEmitter3Control(short itemNumber);
	void FlameEmitterCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll);
}
