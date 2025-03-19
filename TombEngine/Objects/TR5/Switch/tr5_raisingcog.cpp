#include "framework.h"
#include "tr5_raisingcog.h"

#include "Specific/level.h"
#include "Game/control/control.h"
#include "Game/items.h"
#include "Sound/sound.h"
#include "Game/spotcam.h"
#include "Objects/objectslist.h"
#include "Objects/Generic/Switches/generic_switch.h"
#include "Game/Animation/Animation.h"

using namespace TEN::Animation;
using namespace TEN::Entities::Switches;

void InitializeRaisingCog(short itemNumber)
{
	auto* item = &g_Level.Items[itemNumber];

	short itemNos[32];
	int numSwitchItems = GetSwitchTrigger(item, itemNos, 1);

	if (numSwitchItems > 0)
	{
		for (int i = 0; i < numSwitchItems; i++)
		{
			auto* currentItem = &g_Level.Items[itemNos[i]];

			if (currentItem->ObjectNumber == ID_TRIGGER_TRIGGERER)
				item->ItemFlags[1] = currentItem->RoomNumber;

			if (currentItem->ObjectNumber == ID_PULLEY || currentItem->ObjectNumber == ID_TRIGGER_TRIGGERER)
			{
				currentItem->ItemFlags[1] = 1;
				// FIXME: no more hardcoding!
				//PulleyItemNumber = itemNos[i];
			}
		}
	}
}

void RaisingCogControl(short itemNumber)
{
	auto* item = &g_Level.Items[itemNumber];

	if (TriggerActive(item))
	{
		if (item->ItemFlags[0] >= 3)
			AnimateItem(*item);
		else
		{
			if (item->ItemFlags[2] >= 256)
			{
				item->ItemFlags[2] = 0;
				item->ItemFlags[0]++;

				if (item->ItemFlags[0] == 3)
				{
					short itemNos[32];
					short numItems = GetSwitchTrigger(item, itemNos, 1);

					if (numItems > 0)
					{
						for (int i = 0; i < numItems; i++)
						{
							auto* currentItem = &g_Level.Items[itemNos[i]];

							if (item->ObjectNumber == ID_PULLEY)
							{
								if (currentItem->RoomNumber == item->ItemFlags[1])
								{
									currentItem->ItemFlags[1] = 0;
									currentItem->Collidable = true;
								}
								else
									currentItem->ItemFlags[1] = 1;
							}
							else if (item->ObjectNumber == ID_TRIGGER_TRIGGERER)
							{
								AddActiveItem(itemNos[i]);
								currentItem->Status = ITEM_ACTIVE;
								currentItem->AIBits = (GUARD | MODIFY | AMBUSH | PATROL1 | FOLLOW);
							}
						}
					}
				}

				RemoveActiveItem(itemNumber);
				item->Status = ITEM_NOT_ACTIVE;
				item->AIBits = 0;
			}
			else
			{
				if (!item->ItemFlags[2])
				{
					InitializeSpotCam(item->ItemFlags[2]);
					UseSpotCam = true;
				}

				int flags = 0;

				if (item->ItemFlags[2] >= 31)
				{
					if (item->ItemFlags[2] <= 224)
						flags = 31;
					else
						flags = 255 - item->ItemFlags[2];
				}
				else
					flags = item->ItemFlags[2];

				SoundEffect(SFX_TR4_RAISING_BLOCK, &item->Pose, SoundEnvironment::Land, 1.0f, (flags * 256) / 32767.0f); 

				item->ItemFlags[2] += 2;
				item->Pose.Position.y -= 2;
			}
		}
	}
}
