#pragma once

#include "Game/collision/floordata.h"
#include "Physics/Physics.h"

struct ItemInfo;
namespace TEN::Physics { class CollisionMesh; }

using namespace TEN::Physics;

struct DOORPOS_DATA
{
	FloorInfo* floor;
	FloorInfo data;
	int box;
};

struct DOOR_DATA
{
	DOORPOS_DATA d1;
	DOORPOS_DATA d1flip;
	DOORPOS_DATA d2;
	DOORPOS_DATA d2flip;
	bool opened;
	ItemInfo* item;

	CollisionMesh CollisionMesh	= {};
};
