#include "framework.h"
#include "Objects/Generic/Switches/jump_switch.h"

#include "Game/collision/collide_item.h"
#include "Game/control/control.h"
#include "Game/Hud/Hud.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Setup.h"
#include "Objects/Generic/Switches/generic_switch.h"
#include "Specific/Input/Input.h"
#include "Specific/level.h"

using namespace TEN::Hud;
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

	void SetupJumpSwitch(ObjectInfo& object)
	{
		if (object.Animations.size() < 4)
			return;

		auto& idleUpAnim = object.Animations[0];
		auto& pullDownAnim = object.Animations[1];
		auto& idleDownAnim = object.Animations[2];
		auto& resetUpAnim = object.Animations[3];

		idleUpAnim.StateID = SWITCH_ON;
		pullDownAnim.StateID = SWITCH_OFF;
		idleDownAnim.StateID = SWITCH_OFF;
		resetUpAnim.StateID = SWITCH_ON;

		auto remapDispatch = [](auto& anim, int fromState, int toState, int nextAnim)
		{
			for (auto& dispatch : anim.Dispatches)
			{
				if (dispatch.StateID == fromState)
				{
					dispatch.StateID = toState;
					dispatch.NextAnimNumber = nextAnim;
				}
			}
		};

		remapDispatch(idleUpAnim, SWITCH_ON, SWITCH_OFF, 1);
		remapDispatch(idleDownAnim, SWITCH_OFF, SWITCH_ON, 3);
	}

	void JumpSwitchCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll)
	{
		auto* laraInfo = GetLaraInfo(laraItem);
		auto* switchItem = &g_Level.Items[itemNumber];

		bool isSwitchAvailable =
			switchItem->Status == ITEM_NOT_ACTIVE &&
			switchItem->Animation.ActiveState == SWITCH_ON;

		if (isSwitchAvailable)
			g_Hud.InteractionHighlighter.Test(*laraItem, *switchItem, InteractionMode::Activation);

		if (isSwitchAvailable &&
			IsHeld(In::Action) &&
			(laraItem->Animation.ActiveState == LS_REACH || laraItem->Animation.ActiveState == LS_JUMP_UP) &&
			(laraItem->Status || laraItem->Animation.IsAirborne) &&
			laraItem->Animation.Velocity.y > 0 &&
			laraInfo->Control.HandStatus == HandStatus::Free)
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
				switchItem->Animation.TargetState = SWITCH_OFF;
				switchItem->Status = ITEM_ACTIVE;

				AddActiveItem(itemNumber);
			}
		}
	}
}
