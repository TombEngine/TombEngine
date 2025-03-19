#include "framework.h"
#include "Objects/Generic/Switches/AirlockSwitch.h"

#include "Game/Animation/Animation.h"
#include "Game/collision/collide_item.h"
#include "Game/control/control.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Objects/Generic/Switches/generic_switch.h"
#include "Specific/Input/Input.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Input;

namespace TEN::Entities::Switches
{
	ObjectCollisionBounds AirlockSwitchBounds =
	{
		GameBoundingBox::Zero,
		std::pair(
			EulerAngles(ANGLE(-10.0f), ANGLE(-30.0f), ANGLE(-10.0f)),
			EulerAngles(ANGLE(10.0f), ANGLE(30.0f), ANGLE(10.0f)))
	};

	auto AirlockSwitchPos = Vector3i::Zero;

	void AirlockSwitchCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll)
	{
		auto& switchItem = g_Level.Items[itemNumber];
		auto& player = *GetLaraInfo(laraItem);

		if ((IsHeld(In::Action) &&
			switchItem.Animation.ActiveState == 0 &&
			switchItem.Status == ITEM_NOT_ACTIVE &&
			laraItem->Animation.ActiveState == LS_IDLE &&
			laraItem->Animation.AnimNumber == LA_STAND_IDLE &&
			player.Control.HandStatus == HandStatus::Free) ||
			(player.Control.IsMoving && player.Context.InteractedItem == itemNumber))
		{
			auto bounds = GameBoundingBox(&switchItem);

			AirlockSwitchBounds.BoundingBox.X1 = bounds.X1 - BLOCK(0.25f);
			AirlockSwitchBounds.BoundingBox.X2 = bounds.X2 + BLOCK(0.25f);
			AirlockSwitchBounds.BoundingBox.Z1 = bounds.Z1 - BLOCK(0.5f);
			AirlockSwitchBounds.BoundingBox.Z2 = bounds.Z2 + BLOCK(0.5f);
			AirlockSwitchPos.z = bounds.Z1 - 112;

			if (TestLaraPosition(AirlockSwitchBounds, &switchItem, laraItem))
			{
				if (MoveLaraPosition(AirlockSwitchPos, &switchItem, laraItem))
				{
					if (switchItem.Animation.ActiveState == 0)
					{
						SetAnimation(*laraItem, LaraAnim::LA_VALVE_TURN);
						switchItem.Animation.TargetState = 1;
					}

					ResetPlayerFlex(laraItem);
					laraItem->Animation.FrameNumber = 0;
					player.Control.IsMoving = false;
					player.Control.HandStatus = HandStatus::Busy;

					AddActiveItem(itemNumber);
					switchItem.Status = ITEM_ACTIVE;
					AnimateItem(switchItem);
				}
				else
				{
					player.Context.InteractedItem = itemNumber;
				}
			}
			else if (player.Control.IsMoving && player.Context.InteractedItem == itemNumber)
			{
				player.Control.IsMoving = false;
				player.Control.HandStatus = HandStatus::Free;
			}

			return;
		}

		if (laraItem->Animation.ActiveState != LS_SWITCH_DOWN)
			ObjectCollision(itemNumber, laraItem, coll);
	}
}
