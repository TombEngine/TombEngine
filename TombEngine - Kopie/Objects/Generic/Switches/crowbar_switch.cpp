#include "framework.h"

#include "Game/animation.h"
#include "Game/collision/collide_item.h"
#include "Game/Gui.h"
#include "Game/Hud/Hud.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/pickup/pickup.h"
#include "Objects/Generic/Switches/crowbar_switch.h"
#include "Objects/Generic/Switches/generic_switch.h"
#include "Sound/sound.h"
#include "Specific/Input/Input.h"
#include "Specific/level.h"

using namespace TEN::Gui;
using namespace TEN::Hud;
using namespace TEN::Input;

namespace TEN::Entities::Switches
{
	const auto CrowbarPos = Vector3i(-89, 0, -328);
	const ObjectCollisionBounds CrowbarBounds = 
	{
		GameBoundingBox(
			-CLICK(1), CLICK(1),
			0, 0,
			-BLOCK(0.5f), -CLICK(1)
		),
		std::pair(
			EulerAngles(ANGLE(-10.0f), ANGLE(-30.0f), ANGLE(-10.0f)),
			EulerAngles(ANGLE(10.0f), ANGLE(30.0f), ANGLE(10.0f))
		)
	};

	const auto CrowbarPos2 = Vector3i(89, 0, 328);
	const ObjectCollisionBounds CrowbarBounds2 =
	{
		GameBoundingBox(
			-CLICK(1), CLICK(1),
			0, 0,
			CLICK(1), BLOCK(0.5f)
		),
		std::pair(
			EulerAngles(ANGLE(-10.0f), ANGLE(-30.0f), ANGLE(-10.0f)),
			EulerAngles(ANGLE(10.0f), ANGLE(30.0f), ANGLE(10.0f))
		)
	};

	void CrowbarSwitchCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll)
	{
		auto* laraInfo = GetLaraInfo(laraItem);
		ItemInfo* switchItem = &g_Level.Items[itemNumber];

		g_Hud.InteractionHighlighter.Test(*laraItem, *switchItem);

		int doSwitch = 0;

		if (((IsHeld(In::Action) || g_Gui.GetInventoryItemChosen() == ID_CROWBAR_ITEM) &&
			laraItem->Animation.ActiveState == LS_IDLE &&
			laraItem->Animation.AnimNumber == LA_STAND_IDLE &&
			laraInfo->Control.HandStatus == HandStatus::Free &&
			switchItem->ItemFlags[0] == 0) ||
			(laraInfo->Control.IsMoving && laraInfo->Context.InteractedItem == itemNumber))
		{
			if (switchItem->Animation.ActiveState == SWITCH_ON)
			{
				laraItem->Pose.Orientation.y ^= (short)ANGLE(180.0f);

				if (TestLaraPosition(CrowbarBounds2, switchItem, laraItem))
				{
					if (laraInfo->Control.IsMoving || g_Gui.GetInventoryItemChosen() == ID_CROWBAR_ITEM)
					{
						if (MoveLaraPosition(CrowbarPos2, switchItem, laraItem))
						{
							doSwitch = 1;
							laraItem->Animation.AnimNumber = LA_CROWBAR_USE_ON_FLOOR;
							laraItem->Animation.FrameNumber =  GetAnimData(laraItem).frameBase;
							switchItem->Animation.TargetState = SWITCH_OFF;
						}
						else
							laraInfo->Context.InteractedItem = itemNumber;

						g_Gui.SetInventoryItemChosen(NO_VALUE);
					}
					else
						doSwitch = -1;
				}
				else if (laraInfo->Control.IsMoving && laraInfo->Context.InteractedItem == itemNumber)
				{
					laraInfo->Control.IsMoving = false;
					laraInfo->Control.HandStatus = HandStatus::Free;
				}

				laraItem->Pose.Orientation.y ^= (short)ANGLE(180.0f);
			}
			else
			{
				if (TestLaraPosition(CrowbarBounds, switchItem, laraItem))
				{
					if (laraInfo->Control.IsMoving || g_Gui.GetInventoryItemChosen() == ID_CROWBAR_ITEM)
					{
						if (MoveLaraPosition(CrowbarPos, switchItem, laraItem))
						{
							doSwitch = 1;
							laraItem->Animation.AnimNumber = LA_CROWBAR_USE_ON_FLOOR;
							laraItem->Animation.FrameNumber =  GetAnimData(laraItem).frameBase;
							switchItem->Animation.TargetState = SWITCH_ON;
						}
						else
							laraInfo->Context.InteractedItem = itemNumber;

						g_Gui.SetInventoryItemChosen(NO_VALUE);
					}
					else
						doSwitch = -1;
				}
				else if (laraInfo->Control.IsMoving && laraInfo->Context.InteractedItem == itemNumber)
				{
					laraInfo->Control.IsMoving = false;
					laraInfo->Control.HandStatus = HandStatus::Free;
				}
			}
		}

		if (doSwitch)
		{
			if (doSwitch == -1)
			{
				if (laraInfo->Inventory.HasCrowbar)
					g_Gui.SetEnterInventory(ID_CROWBAR_ITEM);
				else
					SayNo(laraItem->Pose.Position);
			}
			else
			{
				ResetPlayerFlex(laraItem);
				laraItem->Animation.TargetState = LS_SWITCH_DOWN;
				laraItem->Animation.ActiveState = LS_SWITCH_DOWN;
				laraInfo->Control.IsMoving = false;
				laraInfo->Control.HandStatus = HandStatus::Busy;
				switchItem->Status = ITEM_ACTIVE;

				AddActiveItem(itemNumber);
				AnimateItem(switchItem);
			}
		}
		else
			ObjectCollision(itemNumber, laraItem, coll);
	}
}
