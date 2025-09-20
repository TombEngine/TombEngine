#include "framework.h"
#include "tr5_highobject.h"
#include "Game/items.h"
#include "Specific/level.h"
#include "Game/room.h"
#include "Sound/sound.h"
#include "Objects/objectslist.h"

void InitializeHighObject1(short itemNumber)
{
	int x = 0;
	int y = 0;
	int z = 0;

	auto* item = &g_Level.Items[itemNumber];

	for (int i = 0; i < g_Level.NumItems; i++)
	{
		auto* currentItem = &g_Level.Items[i];

		if (currentItem->ObjectNumber != ID_TRIGGER_TRIGGERER)
		{
			if (currentItem->ObjectNumber == ID_PUZZLE_ITEM4_COMBO2)
			{
				item->ItemFlags[3] |= (i * 256);
				currentItem->Pose.Position.y = item->Pose.Position.y - 512;
				continue;
			}
		}

		if (currentItem->TriggerFlags == 111)
		{
			item->ItemFlags[3] |= i;
			continue;
		}

		if (currentItem->TriggerFlags != 112)
		{
			if (currentItem->ObjectNumber == ID_PUZZLE_ITEM4_COMBO2)
			{
				item->ItemFlags[3] |= (i * 256);
				currentItem->Pose.Position.y = item->Pose.Position.y - 512;
				continue;
			}
		}
		else
		{
			x = currentItem->Pose.Position.x;
			y = currentItem->Pose.Position.y;
			z = currentItem->Pose.Position.z;
		}
	}

	for (int i = 0; i < g_Level.NumItems; i++)
	{
		auto* currentItem = &g_Level.Items[i];

		if (currentItem->ObjectNumber == ID_PULLEY &&
			currentItem->Pose.Position.x == x &&
			currentItem->Pose.Position.y == y &&
			currentItem->Pose.Position.z == z)
		{
			item->ItemFlags[2] |= i;
			break;
		}
	}
}

void ControlHighObject1(short itemNumber)
{
	auto* item = &g_Level.Items[itemNumber];

	if (!TriggerActive(item))
	{
		if (item->ItemFlags[0] == 4)
		{
			item->ItemFlags[1]--;

			if (!item->ItemFlags[1])
			{
				auto* targetItem = &g_Level.Items[item->ItemFlags[3] & 0xFF];
				targetItem->Flags = (item->Flags & 0xC1FF) | 0x20;

				item->ItemFlags[0] = 6;
				item->ItemFlags[1] = 768;
				TestTriggers(item, true);
			}

			return;
		}

		if (item->ItemFlags[0] == 6)
		{
			item->ItemFlags[1] -= 8;

			if (item->ItemFlags[1] >= 0)
			{
				int flags = 0;

				if (item->ItemFlags[1] >= 256)
				{
					if (item->ItemFlags[1] <= 512)
						flags = 31;
					else
						flags = (768 - item->ItemFlags[1]) / 8;
				}
				else
					flags = item->ItemFlags[1] / 8;

				SoundEffect(SFX_TR4_RAISING_BLOCK, &item->Pose, SoundEnvironment::Land, 1.0f, (flags * 256) / 32767.0f); // TODO: check actual sound!

				item->Pose.Position.y += 8;

				auto* targetItem = &g_Level.Items[(item->ItemFlags[3] / 256) & 0xFF];
				targetItem->Flags |= TRIGGERED;
				targetItem->Pose.Position.y = item->Pose.Position.y - 560;
			}

			if (item->ItemFlags[1] < -60)
			{
				auto* targetItem = &g_Level.Items[item->ItemFlags[2] & 0xFF];
				targetItem->ItemFlags[1] = 0;
				targetItem->Flags |= TRIGGERED;

				item->ItemFlags[0] = 0;
				item->ItemFlags[1] = 0;

				RemoveActiveItem(itemNumber);

				item->Flags &= 0xC1FF;
				item->Status = ITEM_NOT_ACTIVE;
				return;
			}
		}
	}
	else if (item->ItemFlags[0] >= 3)
	{
		if (item->ItemFlags[0] == 4)
		{
			item->ItemFlags[0] = 5;
			item->ItemFlags[1] = 0;
		}
		else if (item->ItemFlags[0] == 5 && !item->ItemFlags[1] &&
			g_Level.Items[(item->ItemFlags[3] / 256) & 0xFF].Flags < 0)
		{
			DoFlipMap(3);
			FlipMap[3] ^= CODE_BITS;
			item->ItemFlags[1] = 1;
		}
	}
	else
	{
		if (item->ItemFlags[1] >= 256)
		{
			item->ItemFlags[1] = 0;
			item->ItemFlags[0]++;

			if (item->ItemFlags[0] == 3)
			{
				item->ItemFlags[1] = 30 * item->TriggerFlags;
				item->ItemFlags[0] = 4;

				short targetItemNumber = item->ItemFlags[3] & 0xFF;
				auto* targetItem = &g_Level.Items[targetItemNumber];

				AddActiveItem(targetItemNumber);

				targetItem->Flags |= 0x3E20u;
				targetItem->Status = ITEM_ACTIVE;

				targetItemNumber = item->ItemFlags[2] & 0xFF;
				targetItem = &g_Level.Items[targetItemNumber];

				targetItem->ItemFlags[1] = 1;
				targetItem->Flags |= TRIGGERED;
				targetItem->Flags &= 0xC1FF;
				return;
			}

			RemoveActiveItem(itemNumber);

			item->Flags &= 0xC1FF;
			item->Status = ITEM_NOT_ACTIVE;
			return;
		}

		int flags = 0;

		if (item->ItemFlags[1] >= 31)
		{
			if (item->ItemFlags[1] <= 224)
				flags = 31;
			else
				flags = 255 - item->ItemFlags[1];
		}
		else
			flags = item->ItemFlags[1];

		SoundEffect(SFX_TR4_RAISING_BLOCK, &item->Pose, SoundEnvironment::Land, 1.0f, (flags * 256) / 32767.0f); // TODO: check actual sound!

		item->ItemFlags[1] += 16;
		item->Pose.Position.y -= 16;

		short targetItemNumber = (item->ItemFlags[3] / 256) & 0xFF;
		auto* targetItem = &g_Level.Items[targetItemNumber];
		targetItem->Flags |= TRIGGERED;
		targetItem->Pose.Position.y = item->Pose.Position.y - 560;
	}
}
