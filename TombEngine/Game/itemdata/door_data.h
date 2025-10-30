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
	short block;
};

struct DOOR_DATA
{
	DOORPOS_DATA d1;
	DOORPOS_DATA d1flip;
	DOORPOS_DATA d2;
	DOORPOS_DATA d2flip;
	short opened;
	short* dptr1;
	short* dptr2;
	short* dptr3;
	short* dptr4;
	unsigned char dn1;
	unsigned char dn2;
	unsigned char dn3;
	unsigned char dn4;
	ItemInfo* item;

	CollisionMesh CollisionMesh	= {};
};
