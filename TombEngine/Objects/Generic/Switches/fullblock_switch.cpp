#include "framework.h"
#include "Objects/Generic/Switches/fullblock_switch.h"

#include "Game/Animation/Animation.h"
#include "Game/collision/collide_item.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Setup.h"
#include "Objects/Generic/Switches/generic_switch.h"
#include "Specific/Input/Input.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Input;

namespace TEN::Entities::Switches
{
	const ObjectCollisionBounds FullBlockSwitchBounds = 
	{
		GameBoundingBox(
			-384, 384,
			0, CLICK(1),
			0, BLOCK(0.5f)
		),
		std::pair(
			EulerAngles(ANGLE(-10.0f), ANGLE(-30.0f), ANGLE(-10.0f)),
			EulerAngles(ANGLE(10.0f), ANGLE(30.0f), ANGLE(10.0f))
		)
	};
	const auto FullBlockSwitchPos = Vector3i::Zero;

	byte SequenceUsed[6];
	byte SequenceResults[3][3][3];
	byte Sequences[3];
	byte CurrentSequence;

	void FullBlockSwitchCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll)
	{
		auto* laraInfo = GetLaraInfo(laraItem);
		auto* switchItem = &g_Level.Items[itemNumber];

		if ((!IsHeld(In::Action) ||
			laraItem->Animation.ActiveState != LS_IDLE ||
			laraItem->Animation.AnimNumber != LA_STAND_IDLE ||
			laraInfo->Control.HandStatus != HandStatus::Free ||
			switchItem->Status ||
			switchItem->Flags & ONESHOT ||
			CurrentSequence >= 3) &&
			(!laraInfo->Control.IsMoving || laraInfo->Context.InteractedItem !=itemNumber))
		{
			ObjectCollision(itemNumber, laraItem, coll);
			return;
		}

		if (TestLaraPosition(FullBlockSwitchBounds, switchItem, laraItem))
		{
			if (MoveLaraPosition(FullBlockSwitchPos, switchItem, laraItem))
			{
				if (switchItem->Animation.ActiveState == 1)
				{
					laraItem->Animation.ActiveState = LS_SWITCH_DOWN;
					laraItem->Animation.AnimNumber = LA_BUTTON_GIANT_PUSH;
					switchItem->Animation.TargetState = 0;
				}

				laraItem->Animation.TargetState = LS_IDLE;
				laraItem->Animation.FrameNumber = 0;
				switchItem->Status = ITEM_ACTIVE;

				AddActiveItem(itemNumber);
				AnimateItem(*switchItem);

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

	void FullBlockSwitchControl(short itemNumber)
	{
		ItemInfo* switchItem = &g_Level.Items[itemNumber];

		if (switchItem->Animation.AnimNumber != 2 ||
			CurrentSequence >= 3 ||
			switchItem->ItemFlags[0])
		{
			if (CurrentSequence >= 4)
			{
				switchItem->ItemFlags[0] = 0;
				switchItem->Animation.TargetState = SWITCH_ON;
				switchItem->Status = ITEM_NOT_ACTIVE;

				if (++CurrentSequence >= 7)
					CurrentSequence = 0;
			}
		}
		else
		{
			switchItem->ItemFlags[0] = 1;
			Sequences[CurrentSequence++] = switchItem->TriggerFlags;
		}

		AnimateItem(*switchItem);
	}
}
