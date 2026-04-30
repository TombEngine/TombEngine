#include "framework.h"
#include "Objects/Generic/Switches/fullblock_switch.h"

#include "Game/Animation/Animation.h"
#include "Game/collision/collide_item.h"
#include "Game/Hud/Hud.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Setup.h"
#include "Objects/Generic/Switches/generic_switch.h"
#include "Specific/Input/Input.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Hud;
using namespace TEN::Input;

namespace TEN::Entities::Switches
{
	const ObjectCollisionBounds FullBlockSwitchBounds = 
	{
		GameBoundingBox(
			-384, 384,
			0, CLICK(1),
            -CLICK(1), BLOCK(0.5f)
		),
		std::pair(
			EulerAngles(ANGLE(-10.0f), ANGLE(-30.0f), ANGLE(-10.0f)),
			EulerAngles(ANGLE(10.0f), ANGLE(30.0f), ANGLE(10.0f))
		)
	};
	const auto FullBlockSwitchPos = Vector3i(0, CLICK(1), 0);

	byte SequenceUsed[6]; //Stores the current active sequence.
	byte SequenceResults[3][3][3];  //Maps combination to a door ocb
	byte Sequences[3]; //Current Sequence
	byte CurrentSequence; //Count of switches pressed in current sequence

    void SetupFullBlockSwitch()
    {
        CurrentSequence = 0;
        SequenceResults[0][1][2] = 0;
        SequenceResults[0][2][1] = 1;
        SequenceResults[1][0][2] = 2;
        SequenceResults[1][2][0] = 3;
        SequenceResults[2][0][1] = 4;
        SequenceResults[2][1][0] = 5;
        SequenceUsed[0] = 0;
        SequenceUsed[1] = 0;
        SequenceUsed[2] = 0;
        SequenceUsed[3] = 0;
        SequenceUsed[4] = 0;
        SequenceUsed[5] = 0;
    }

    void FullBlockSwitchCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll)
    {
        auto* laraInfo = GetLaraInfo(laraItem);
        auto* switchItem = &g_Level.Items[itemNumber];

        if (switchItem->Animation.ActiveState == SWITCH_ON)
            g_Hud.InteractionHighlighter.Test(*laraItem, *switchItem);

        if ((!IsHeld(In::Action) ||
            laraItem->Animation.ActiveState != LS_IDLE ||
            laraItem->Animation.AnimNumber != LA_STAND_IDLE ||
            laraInfo->Control.HandStatus != HandStatus::Free ||
            switchItem->ItemFlags[0] == 1 ||
            CurrentSequence >= 3) &&
            (!laraInfo->Control.IsMoving || laraInfo->Context.InteractedItem != itemNumber))
        {
            ObjectCollision(itemNumber, laraItem, coll);
            return;
        }

        if (TestLaraPosition(FullBlockSwitchBounds, switchItem, laraItem))
        {
            if (MoveLaraPosition(FullBlockSwitchPos, switchItem, laraItem))
            {
                if (switchItem->Animation.ActiveState == SWITCH_ON)
                {
                    laraItem->Animation.ActiveState = LS_SWITCH_DOWN;
                    laraItem->Animation.AnimNumber = LA_BUTTON_GIANT_PUSH;
                    switchItem->Animation.TargetState = SWITCH_OFF;
                }
                laraItem->Animation.TargetState = LS_IDLE;
                laraItem->Animation.FrameNumber = 0;
                switchItem->Status = ITEM_ACTIVE;
                AddActiveItem(itemNumber);
                AnimateItem(switchItem);
                ResetPlayerFlex(laraItem);
                laraInfo->Control.IsMoving = false;
                laraInfo->Control.HandStatus = HandStatus::Busy;
            }
            else
                laraInfo->Context.InteractedItem = itemNumber;
        }
        else if (laraInfo->Control.IsMoving && laraInfo->Context.InteractedItem == itemNumber)
        {
            laraInfo->Control.IsMoving = false;
            laraInfo->Control.HandStatus = HandStatus::Free;
        }
     }

    // Shared control handler
     void FullBlockSwitchControl(short itemNumber, byte switchIndex)
     {
        auto* switchItem = &g_Level.Items[itemNumber];

        if (switchItem->Animation.AnimNumber != 2 ||
            CurrentSequence >= 3 ||
            switchItem->ItemFlags[0])
        {
            if (CurrentSequence >= 4)
            {
                switchItem->ItemFlags[0] = 0;
                switchItem->Animation.TargetState = SWITCH_ON;
                switchItem->Status = ITEM_NOT_ACTIVE;
                CurrentSequence++;

                if (CurrentSequence >= 7)
                    CurrentSequence = 0;
            }
        }
        else
        {
            switchItem->ItemFlags[0] = 1;
            Sequences[CurrentSequence] = switchIndex;
            CurrentSequence++;

            if (CurrentSequence == 3 && SequenceUsed[SequenceResults[Sequences[0]][Sequences[1]][Sequences[2]]])
                CurrentSequence++;
        }

        AnimateItem(switchItem);
    }

    // Switch 1 - TriggerFlag 0
    void FullBlockSwitch1Control(short itemNumber)
    {
        FullBlockSwitchControl(itemNumber, 0);
    }

    // Switch 2 - TriggerFlag 1
    void FullBlockSwitch2Control(short itemNumber)
    {
        FullBlockSwitchControl(itemNumber, 1);
    }

    // Switch 3 - TriggerFlag 2
    void FullBlockSwitch3Control(short itemNumber)
    {
        FullBlockSwitchControl(itemNumber, 2);
    }
}
