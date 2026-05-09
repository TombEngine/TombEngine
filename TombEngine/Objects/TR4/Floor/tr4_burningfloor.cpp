#include "framework.h"
#include "Objects/TR4/Floor/tr4_burningfloor.h"

#include "Game/effects/debris.h"
#include "Game/effects/item_fx.h"
#include "Game/effects/tomb4fx.h"
#include "Game/effects/weather.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/room.h"
#include "Math/Math.h"
#include "Objects/objectslist.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Effects::Environment;
using namespace TEN::Effects::Items;

// 15 of the 16 fire spawn locations on the burning floor, relative to the item's origin.
// Format: { xOffset, zOffset, sizeIndex }
// sizeIndex: 0 = small (ItemFlags[0]), 1 = medium (ItemFlags[1]), 2 = large (ItemFlags[2])
// NOTE: The original TR4 data contained 16 entries but only iterated 15
static constexpr int FLOOR_FIRES[16][3] =
{
	{  -96,  1216, 2 },
	{  560,   736, 2 },
	{ -432,  -976, 2 },
	{  -64,  -128, 2 },
	{  824,    64, 2 },
	{  456,  -352, 1 },
	{  392,   352, 1 },
	{ 1096,   608, 1 },
	{ -424,  -416, 1 },
	{  520,  1152, 1 },
	{ -248,   516, 1 },
	{ -808,    80, 1 },
	{-1192,  -384, 0 },
	{ -904,  -864, 0 },
	{ -136,  -912, 0 },
	{  184,   608, 0 }  // Entry 15: present in data but not iterated in the original.
};

// 4 world-space positions around the burning floor that will burn Lara on explosion.
static constexpr int DEADLY_FLOOR_FIRES[4][2] =
{
	{ -512, -512 },
	{    0,    0 },
	{  512,  512 },
	{    0,  768 }
};

// Maps sizeIndex (0/1/2) to AddFire float size and Y height offset.
static constexpr float FLOOR_FIRE_SIZES[3]  = { 0.5f, 1.0f, 2.0f };
static constexpr int   FLOOR_FIRE_Y_OFFS[3] = { 64, 128, 192 };

void InitializeBurningFloor(short itemNumber)
{
	g_Level.Items[itemNumber].Animation.RequiredState = 127;
}

void BurningFloorControl(short itemNumber)
{
	auto* item = &g_Level.Items[itemNumber];

	// Phase 0: Wait for a stationary lit torch to ignite the floor
	if (!item->ItemFlags[3])
	{
		auto spheres = item->GetSpheres();

		for (short torchNum = g_Level.Rooms[item->RoomNumber].itemNumber; torchNum != NO_VALUE;
			torchNum = g_Level.Items[torchNum].NextItem)
		{
			auto* torch = &g_Level.Items[torchNum];

			// Only consider a lit, fully-settled burning torch.
			if (torch->ObjectNumber != ID_BURNING_TORCH_ITEM ||
				torch->Animation.Velocity.z != 0.0f ||
				torch->Animation.Velocity.y != 0.0f ||
				!torch->ItemFlags[3])
			{
				continue;
			}

			// If the torch lies outside any of the floor object's collision spheres, ignite.
			for (const auto& sphere : spheres)
			{
				float dx   = sphere.Center.x - torch->Pose.Position.x;
				float dy   = sphere.Center.y - torch->Pose.Position.y;
				float dz   = sphere.Center.z - torch->Pose.Position.z;
				float rSum = sphere.Radius + 32.0f;

				if ((dx * dx + dy * dy + dz * dz) <= (rSum * rSum))
				{
					item->ItemFlags[3] = 1;
					KillItem(torchNum);
					return;
				}
			}
		}

		return;
	}

	// Precompute Y-rotation basis for offsetting fire positions
	short deltaY = item->Pose.Orientation.y + ANGLE(90.0f);
	float sinY = phd_sin(deltaY);
	float cosY = phd_cos(deltaY);

	// Spawn floor fires at rotated offsets ---
	for (int i = 0; i < 15; i++) // NOTE: Original iterates only 15 of 16 entries.
	{
		int   xoff     = FLOOR_FIRES[i][0];
		int   zoff     = FLOOR_FIRES[i][1];
		int   sizeIdx  = FLOOR_FIRES[i][2];
		short intensity = item->ItemFlags[sizeIdx];

		if (!intensity)
			continue;

		// Convert intensity (1-255, higher = brighter) to fade (1-255, higher = more faded).
		short fade = UCHAR_MAX + 1 - intensity;

		// Rotate offset by the object's current Y orientation.
		int rotX = (int)(xoff * cosY - zoff * sinY);
		int rotZ = (int)(xoff * sinY + zoff * cosY);

		AddFire(
			item->Pose.Position.x + rotX,
			item->Pose.Position.y - FLOOR_FIRE_Y_OFFS[sizeIdx],
			item->Pose.Position.z + rotZ,
			item->RoomNumber,
			FLOOR_FIRE_SIZES[sizeIdx],
			fade);
	}

	// Burn Lara if she steps into a deadly fire position
	if (LaraItem->Effect.Type != EffectType::Fire)
	{
		for (int i = 0; i < 4; i++)
		{
			int xoff = DEADLY_FLOOR_FIRES[i][0];
			int zoff = DEADLY_FLOOR_FIRES[i][1];

			int rotX = (int)(xoff * cosY - zoff * sinY);
			int rotZ = (int)(xoff * sinY + zoff * cosY);

			int dx = abs(item->Pose.Position.x + rotX - LaraItem->Pose.Position.x);
			int dy = abs(item->Pose.Position.y       - LaraItem->Pose.Position.y);
			int dz = abs(item->Pose.Position.z + rotZ - LaraItem->Pose.Position.z);

			if (dx < 200 && dy < 200 && dz < 200)
			{
				ItemBurn(LaraItem);
				LaraItem->HitPoints = 100;

				// Fast-forward to the wind-down phase.
				item->ItemFlags[3] = 450;
				item->ItemFlags[0] = 2;
				item->ItemFlags[1] = 2;
				item->ItemFlags[2] = 2;

				Weather.Flash(255, 64, 0, 0.03f);
				break;
			}
		}
	}


	if (!item->ItemFlags[4])
		SoundEffect(SFX_TR4_LOOP_FOR_SMALL_FIRES, &item->Pose);

	// Ramp-up phase: gradually increase fire intensities
	if (item->ItemFlags[3] < 450)
	{
		item->ItemFlags[0] += 4;

		if (item->ItemFlags[3] > 30)
			item->ItemFlags[1] += 4;

		if (item->ItemFlags[3] > 60)
			item->ItemFlags[2] += 8;

		if (item->ItemFlags[0] > 255) item->ItemFlags[0] = 255;
		if (item->ItemFlags[1] > 255) item->ItemFlags[1] = 255;
		if (item->ItemFlags[2] > 255) item->ItemFlags[2] = 255;

		item->ItemFlags[3]++;
		item->Animation.RequiredState = 127 - item->ItemFlags[3] / 6;
	}
	else // Wind-down phase: fade out fires and trigger the flipmap
	{
		item->ItemFlags[0] -= 4;
		item->ItemFlags[1] -= 3;
		item->ItemFlags[2] -= 2;

		if (item->ItemFlags[0] < 2) item->ItemFlags[0] = 2;
		if (item->ItemFlags[1] < 2) item->ItemFlags[1] = 2;
		if (item->ItemFlags[2] < 2) item->ItemFlags[2] = 2;

		if (item->ItemFlags[0] == 2 && item->ItemFlags[1] == 2 && item->ItemFlags[2] == 2)
		{
			// TriggerFlags specifies which flipmap group to trigger (0 = original group 0).
			item->ItemFlags[4] = 1;
			DoFlipMap(item->TriggerFlags);
			SoundEffect(SFX_TR4_SMASH_ROCK, &item->Pose);
			ExplodeItemNode(item, 0, 1, -24);
			ExplodeItemNode(item, 1, 1, -24);
			ExplodeItemNode(item, 2, 1, -24);
			ExplodeItemNode(item, 3, 1, -24);
			ExplodeItemNode(item, 4, 1, -32);
			KillItem(itemNumber);
		}
	}
}
