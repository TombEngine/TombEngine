#include "framework.h"
#include "Objects/TR4/Trap/tr4_mine.h"

#include "Game/collision/collide_item.h"
#include "Game/collision/Sphere.h"
#include "Game/effects/debris.h"
#include "Game/effects/effects.h"
#include "Game/effects/tomb4fx.h"
#include "Game/effects/weather.h"
#include "Game/items.h"
#include "Objects/objectslist.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Collision::Sphere;
using namespace TEN::Effects::Environment;

namespace TEN::Entities::Traps
{
	void InitializeMine(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		if (item.TriggerFlags)
			item.MeshBits = 0;
	}

	void ControlMine(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		auto spheres = item.GetSpheres();
		if (item.ItemFlags[0] >= 150)
		{
			SoundEffect(SFX_TR4_EXPLOSION1, &item.Pose);
			SoundEffect(SFX_TR4_EXPLOSION2, &item.Pose);
			SoundEffect(SFX_TR4_EXPLOSION1, &item.Pose, SoundEnvironment::Land, 0.7f, 0.5f);

			if (!spheres.empty())
			{
				for (int i = 0; i < spheres.size(); i++)
				{
					// TODO: Hardcoding.
					if (i >= 7 && i != 9)
					{
						const auto& sphere = spheres[i];

						TriggerExplosionSparks(sphere.Center.x, sphere.Center.y, sphere.Center.z, 3, -2, 0, -item.RoomNumber);
						TriggerExplosionSparks(sphere.Center.x, sphere.Center.y, sphere.Center.z, 3, -1, 0, -item.RoomNumber);
						TriggerShockwave(&Pose(Vector3i(sphere.Center)), 48, 304, (GetRandomControl() & 0x1F) + 112, 0, 96, 128, 32, EulerAngles(2048, 0.0f, 0.0f), 0, true, false, false, (int)ShockwaveStyle::Normal);
					}
				}

				for (int i = 0; i < spheres.size(); i++)
					ExplodeItemNode(&item, i, 0, -128);
			}

			Weather.Flash(255, 192, 64, 0.03f);

			int currentItemNumber = g_Level.Rooms[item.RoomNumber].itemNumber;

			// Make sentry gun explode?
			while (currentItemNumber != NO_VALUE)
			{
				auto* currentItem = &g_Level.Items[currentItemNumber];

				if (currentItem->ObjectNumber == ID_SENTRY_GUN)
					currentItem->MeshBits &= ~0x40;

				currentItemNumber = currentItem->NextItem;
			}

			KillItem(itemNumber);
		}
		else
		{
			item.ItemFlags[0]++;

			int fade = 4 * item.ItemFlags[0];
			if (fade > 255)
				fade = 0;

			for (int i = 0; i < spheres.size(); i++)
			{
				if (i == 0 || i > 5)
				{
					const auto& sphere = spheres[i];
					AddFire(sphere.Center.x, sphere.Center.y, sphere.Center.z, item.RoomNumber, 0.25f, fade);
				}
			}

			SoundEffect(SFX_TR4_LOOP_FOR_SMALL_FIRES, &item.Pose);
		}
	}

	void CollideMine(short itemNumber, ItemInfo* playerItem, CollisionInfo* coll)
	{
		auto& mineItem = g_Level.Items[itemNumber];

		if (!mineItem.TriggerFlags || mineItem.ItemFlags[3])
			return;

		if (playerItem->Animation.AnimNumber != LA_DETONATOR_USE ||
			playerItem->Animation.FrameNumber < 57)
		{
			if (TestBoundsCollide(&mineItem, playerItem, BLOCK(0.5f)))
			{
				TriggerExplosionSparks(mineItem.Pose.Position.x, mineItem.Pose.Position.y, mineItem.Pose.Position.z, 3, -2, 0, mineItem.RoomNumber);
				for (int i = 0; i < 2; i++)
					TriggerExplosionSparks(mineItem.Pose.Position.x, mineItem.Pose.Position.y, mineItem.Pose.Position.z, 3, -1, 0, mineItem.RoomNumber);

				mineItem.MeshBits = 1;

				ExplodeItemNode(&mineItem, 0, 0, 128);
				KillItem(itemNumber);

				playerItem->Animation.AnimNumber = LA_MINE_DEATH;
				playerItem->Animation.FrameNumber = 0;
				playerItem->Animation.ActiveState = LS_DEATH;
				playerItem->Animation.Velocity.z = 0;

				SoundEffect(SFX_TR4_MINE_EXPLOSION_OVERLAY, &mineItem.Pose);
			}
		}
		else
		{
			for (int i = 0; i < g_Level.NumItems; i++)
			{
				auto* currentItem = &g_Level.Items[i];

				// Explode other mines.
				if (currentItem->ObjectNumber == ID_MINE &&
					currentItem->Status != ITEM_INVISIBLE &&
					currentItem->TriggerFlags == 0)
				{
					TriggerExplosionSparks(
						currentItem->Pose.Position.x,
						currentItem->Pose.Position.y,
						currentItem->Pose.Position.z,
						3,
						-2,
						0,
						currentItem->RoomNumber);

					for (int j = 0; j < 2; j++)
						TriggerExplosionSparks(
							currentItem->Pose.Position.x,
							currentItem->Pose.Position.y,
							currentItem->Pose.Position.z,
							3,
							-1,
							0,
							currentItem->RoomNumber);

					currentItem->MeshBits = 1;

					ExplodeItemNode(currentItem, 0, 0, -32);
					KillItem(i);

					if (!(GetRandomControl() & 3))
						SoundEffect(SFX_TR4_MINE_EXPLOSION_OVERLAY, &currentItem->Pose);

					currentItem->Status = ITEM_INVISIBLE;
				}
			}
		}
	}
}
