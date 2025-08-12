#pragma once

struct CollisionInfo;
struct ItemInfo;

namespace TEN::Entities::Generic
{
	void InitializeSwingingSandbag(short itemNumber);
	void InitializeSwingingBox(short itemNumber);
	void InitializeOverheadPulleyHook(short itemNumber);
	void ControlPendulum(short itemNumber);
	void CollidePendulum(short itemNumber, ItemInfo* playerItem, CollisionInfo* coll);
}

