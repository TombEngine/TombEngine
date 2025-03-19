#include "framework.h"
#include "Objects/Generic/Doors/generic_doors.h"
#include "Specific/level.h"
#include "Game/control/control.h"
#include "Game/control/box.h"
#include "Game/items.h"
#include "Game/control/lot.h"
#include "Game/Gui.h"
#include "Specific/Input/Input.h"
#include "Game/pickup/pickup.h"
#include "Sound/sound.h"
#include "Game/Animation/Animation.h"
#include "Game/Lara/lara_struct.h"
#include "Game/Lara/lara.h"
#include "Math/Math.h"
#include "Game/misc.h"
#include "Objects/Generic/Doors/sequence_door.h"
#include "Objects/Generic/Switches/fullblock_switch.h"
#include "Game/itemdata/door_data.h"

using namespace TEN::Animation;
using namespace TEN::Entities::Switches;

namespace TEN::Entities::Doors
{
	void SequenceDoorControl(short itemNumber)
	{
		auto* doorItem = &g_Level.Items[itemNumber];
		auto* doorData = (DOOR_DATA*)doorItem->Data;

		if (CurrentSequence == 3)
		{
			if (SequenceResults[Sequences[0]][Sequences[1]][Sequences[2]] == doorItem->TriggerFlags)
			{
				if (doorItem->Animation.ActiveState == 1)
					doorItem->Animation.TargetState = 1;
				else
					doorItem->Animation.TargetState = 0;

				TestTriggers(doorItem, true);
			}

			CurrentSequence = 4;
		}

		if (doorItem->Animation.ActiveState == doorItem->Animation.TargetState)
		{
			if (doorItem->Animation.ActiveState == 1)
			{
				if (!doorData->opened)
				{
					OpenThatDoor(&doorData->d1, doorData);
					OpenThatDoor(&doorData->d2, doorData);
					OpenThatDoor(&doorData->d1flip, doorData);
					OpenThatDoor(&doorData->d2flip, doorData);
					doorData->opened = true;
					doorItem->Flags |= 0x3E;
				}
			}
			else
			{
				if (doorData->opened)
				{
					ShutThatDoor(&doorData->d1, doorData);
					ShutThatDoor(&doorData->d2, doorData);
					ShutThatDoor(&doorData->d1flip, doorData);
					ShutThatDoor(&doorData->d2flip, doorData);
					doorData->opened = false;
					doorItem->Flags &= 0xC1;
				}
			}
		}

		AnimateItem(*doorItem);
	}
}
