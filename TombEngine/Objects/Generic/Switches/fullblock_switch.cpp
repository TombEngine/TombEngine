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

    void SetupFullBlockSwitch()
    {
        auto& sequenceData = Lara.Control.SequenceSwitch;

        sequenceData.CurrentSequence = 0;
        sequenceData.SequenceResults[0][1][2] = 0;
        sequenceData.SequenceResults[0][2][1] = 1;
        sequenceData.SequenceResults[1][0][2] = 2;
        sequenceData.SequenceResults[1][2][0] = 3;
        sequenceData.SequenceResults[2][0][1] = 4;
        sequenceData.SequenceResults[2][1][0] = 5;
        sequenceData.SequenceUsed[0] = 0;
        sequenceData.SequenceUsed[1] = 0;
        sequenceData.SequenceUsed[2] = 0;
        sequenceData.SequenceUsed[3] = 0;
        sequenceData.SequenceUsed[4] = 0;
        sequenceData.SequenceUsed[5] = 0;
    }

    void FullBlockSwitchCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll)
    {
        auto* laraInfo = GetLaraInfo(laraItem);
        auto& sequenceData = laraInfo->Control.SequenceSwitch;
        auto* switchItem = &g_Level.Items[itemNumber];

        if (switchItem->Animation.ActiveState == SWITCH_ON)
            g_Hud.InteractionHighlighter.Test(*laraItem, *switchItem);

        if ((!IsHeld(In::Action) ||
            laraItem->Animation.ActiveState != LS_IDLE ||
            laraItem->Animation.AnimNumber != LA_STAND_IDLE ||
            laraInfo->Control.HandStatus != HandStatus::Free ||
            switchItem->ItemFlags[0] == 1 ||
            sequenceData.CurrentSequence >= 3) &&
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
        auto& sequenceData = Lara.Control.SequenceSwitch;

        if (switchItem->Animation.AnimNumber != 2 ||
            sequenceData.CurrentSequence >= 3 ||
            switchItem->ItemFlags[0])
        {
            if (sequenceData.CurrentSequence >= 4)
            {
                switchItem->ItemFlags[0] = 0;
                switchItem->Animation.TargetState = SWITCH_ON;
                switchItem->Status = ITEM_NOT_ACTIVE;
                sequenceData.CurrentSequence++;

                if (sequenceData.CurrentSequence >= 7)
                    sequenceData.CurrentSequence = 0;
            }
        }
        else
        {
            switchItem->ItemFlags[0] = 1;
            sequenceData.Sequences[sequenceData.CurrentSequence] = switchIndex;
            sequenceData.CurrentSequence++;

            if (sequenceData.CurrentSequence == 3 && sequenceData.SequenceUsed[sequenceData.SequenceResults[sequenceData.Sequences[0]][sequenceData.Sequences[1]][sequenceData.Sequences[2]]])
                sequenceData.CurrentSequence++;
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
