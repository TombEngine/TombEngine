#include "framework.h"
#include "Objects/Generic/Switches/jump_switch.h"
#include "Game/control/control.h"
#include "Specific/Input/Input.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Objects/Generic/Switches/generic_switch.h"
#include "Specific/level.h"
#include "Game/collision/collide_item.h"
#include "Game/items.h"

using namespace TEN::Input;

namespace TEN::Entities::Switches
{
	const ObjectCollisionBounds JumpSwitchBounds =  
	{
		GameBoundingBox(
			-CLICK(0.5f), CLICK(0.5f),
			-CLICK(1), CLICK(1),
			CLICK(1.5f), BLOCK(0.5f)
		),
		std::pair(
			EulerAngles(ANGLE(-10.0f), ANGLE(-30.0f), ANGLE(-10.0f)),
			EulerAngles(ANGLE(10.0f), ANGLE(30.0f), ANGLE(10.0f))
		)
	};
	const auto JumpSwitchPos = Vector3i(0, -208, 256);

	void JumpSwitchCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll)
	{
		auto* laraInfo = GetLaraInfo(laraItem);
		auto* switchItem = &g_Level.Items[itemNumber];

		if (IsHeld(In::Action) &&
			(laraItem->Animation.ActiveState == LS_REACH || laraItem->Animation.ActiveState == LS_JUMP_UP) &&
			(laraItem->Status || laraItem->Animation.IsAirborne) &&
			laraItem->Animation.Velocity.y > 0 &&
			laraInfo->Control.HandStatus == HandStatus::Free &&
			switchItem->Animation.ActiveState == SWITCH_OFF)
		{
			if (TestLaraPosition(JumpSwitchBounds, switchItem, laraItem))
			{
				AlignLaraPosition(JumpSwitchPos, switchItem, laraItem);

				laraItem->Animation.ActiveState = LS_SWITCH_DOWN;
				laraItem->Animation.AnimNumber = LA_JUMPSWITCH_PULL;
				laraItem->Animation.Velocity.y = 0;
				laraItem->Animation.FrameNumber = 0;
				laraItem->Animation.IsAirborne = false;
				laraInfo->Control.HandStatus = HandStatus::Busy;
				switchItem->Animation.TargetState = SWITCH_ON;
				switchItem->Status = ITEM_ACTIVE;

				AddActiveItem(itemNumber);
			}
		}
	}
}
