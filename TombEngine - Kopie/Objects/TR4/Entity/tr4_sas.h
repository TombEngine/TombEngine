#pragma once

struct CollisionInfo;
struct ItemInfo;

struct CollisionInfo;
struct ItemInfo;
struct ObjectInfo;

namespace TEN::Entities::TR4
{
	void InitializeSas(short itemNumber);
	void SasControl(short itemNumber);
	void SasFireGrenade(ItemInfo& item, short angle1, short angle2);

	void InitializeInjuredSas(short itemNumber);
	void InjuredSasControl(short itemNumber);
	void SasDragBlokeCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll);
}
