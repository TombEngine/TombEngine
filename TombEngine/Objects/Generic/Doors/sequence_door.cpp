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
#include "Objects/Generic/Doors/generic_doors.h"
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
		auto* door = &GetDoorObject(*doorItem);

		if (doorItem->ItemFlags[0])
		{
			if (TriggerActive(doorItem))
			{
				if (!doorItem->Animation.ActiveState)
					doorItem->Animation.TargetState =  1;
				else
				{
					if (!door->opened)
					{
						OpenThatDoor(&door->d1, door);
						OpenThatDoor(&door->d2, door);
						OpenThatDoor(&door->d1flip, door);
						OpenThatDoor(&door->d2flip, door);
						door->opened = 1;
					}

					if (CurrentSequence == 3)
					{
						if (SequenceResults[Sequences[0]][Sequences[1]][Sequences[2]] == doorItem->TriggerFlags &&
							!Sequences[0] && Sequences[1] == 1 && Sequences[2] == 2)
						{
							CurrentSequence = 4;
							SequenceUsed[doorItem->TriggerFlags] = Sequences[1];
						}
					}
					else if ((CurrentSequence == 1 || CurrentSequence == 2) && doorItem->TriggerFlags == 2)
					{
						doorItem->Flags &= ~(IFLAG_INVISIBLE | IFLAG_CLEAR_BODY);
						doorItem->Animation.TargetState = 0;
						doorItem->ItemFlags[0] = 0;
					}
				}
			}
			else
			{
				if (doorItem->Animation.ActiveState == 1)
					doorItem->Animation.TargetState = 0;
				else
				{
					if (CurrentSequence == 3 && SequenceResults[Sequences[0]][Sequences[1]][Sequences[2]] == doorItem->TriggerFlags)
					{
						CurrentSequence = 4;

						if (doorItem->TriggerFlags != 2)
							SequenceUsed[doorItem->TriggerFlags] = 1;
					}

					if (door->opened)
					{
						ShutThatDoor(&door->d1, door);
						ShutThatDoor(&door->d2, door);
						ShutThatDoor(&door->d1flip, door);
						ShutThatDoor(&door->d2flip, door);
						door->opened = 0;
					}
				}
			}
		}
		else if (!doorItem->Animation.ActiveState && CurrentSequence == 3 && SequenceResults[Sequences[0]][Sequences[1]][Sequences[2]] == doorItem->TriggerFlags)
		{
			if (doorItem->TriggerFlags && doorItem->TriggerFlags != 2 && !SequenceUsed[0])
			{
				Sequences[1] = 0;
				Sequences[0] = 1;
				Sequences[2] = 2;
				return;
			}

			doorItem->Animation.TargetState = 1;
			doorItem->ItemFlags[0] = 1;
		}

		AnimateItem(doorItem);
	}
}
