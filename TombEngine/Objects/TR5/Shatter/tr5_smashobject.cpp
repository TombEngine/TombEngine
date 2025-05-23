#include "framework.h"
#include "Objects/TR5/Shatter/tr5_smashobject.h"
#include "Specific/level.h"
#include "Game/control/box.h"
#include "Sound/sound.h"
#include "Game/effects/tomb4fx.h"
#include "Game/items.h"

using namespace TEN::Collision::Room;

void InitializeSmashObject(short itemNumber)
{
	auto* item = &g_Level.Items[itemNumber];
	item->Flags = 0;
	item->MeshBits = 1;

	auto& room = g_Level.Rooms[item->RoomNumber];

	// NOTE: Avoids crash when attempting to access Boxes[] array while box is equal to NO_VALUE. -- TokyoSU 2022.12.20
	FloorInfo* floor = GetSector(&room, item->Pose.Position.x - room.Position.x, item->Pose.Position.z - room.Position.z);
	if (floor->PathfindingBoxID == NO_VALUE)
	{
		TENLog("Smash object with ID " + std::to_string(itemNumber) + " may be inside a wall." , LogLevel::Warning);
		return;
	}

	auto* box = &g_Level.PathfindingBoxes[floor->PathfindingBoxID];
	if (box->flags & 0x8000)
		box->flags |= BLOCKED;
}

void SmashObject(short itemNumber)
{
	auto* item = &g_Level.Items[itemNumber];
	auto* room = &g_Level.Rooms[item->RoomNumber];

	int sector = ((item->Pose.Position.z - room->Position.z) / 1024) + room->ZSize * ((item->Pose.Position.x - room->Position.z) / 1024);

	auto* box = &g_Level.PathfindingBoxes[room->Sectors[sector].PathfindingBoxID];
	if (box->flags & 0x8000)
		box->flags &= ~BLOCKED;

	SoundEffect(SFX_TR5_SMASH_GLASS, &item->Pose);

	item->Collidable = false;
	item->MeshBits = 0xFFFE;

	ExplodingDeath(itemNumber, BODY_DO_EXPLOSION | BODY_NO_BOUNCE);

	item->Flags |= IFLAG_INVISIBLE;

	if (item->Status == ITEM_ACTIVE)
		RemoveActiveItem(itemNumber);

	item->Status = ITEM_DEACTIVATED;
}

void SmashObjectControl(short itemNumber)
{
	SmashObject(itemNumber * 65536);
}
