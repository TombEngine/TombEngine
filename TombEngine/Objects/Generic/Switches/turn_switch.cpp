#include "framework.h"
#include "Objects/Generic/Switches/turn_switch.h"

#include "Game/Animation/Animation.h"
#include "Game/camera.h"
#include "Game/collision/collide_item.h"
#include "Game/collision/collide_room.h"
#include "Game/control/control.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Setup.h"
#include "Sound/sound.h"
#include "Specific/Input/Input.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Input;

// TODO: Must now also check anim numbers.

namespace TEN::Entities::Switches
{
	enum TurnSwitchDirection
	{
		TURN_SWITCH_STOP,
		TURN_SWITCH_ANTICLOCKWISE,
		TURN_SWITCH_CLOCKWISE
	};

	const auto TurnSwitchPosA = Vector3i(650, 0, -138);
	const auto TurnSwitchPos = Vector3i(650, 0, 138);
	const ObjectCollisionBounds TurnSwitchBoundsA = 
	{
		GameBoundingBox(
			BLOCK(0.5f), CLICK(3.5f),
			0, 0,
			-BLOCK(0.5f), 0
		),
		std::pair(
			EulerAngles(ANGLE(-10.0f), ANGLE(-30.0f), ANGLE(-10.0f)),
			EulerAngles(ANGLE(10.0f), ANGLE(30.0f), ANGLE(10.0f))
		)
	};
	const ObjectCollisionBounds TurnSwitchBoundsC =
	{
		GameBoundingBox(
			BLOCK(0.5f), CLICK(3.5f),
			0, 0,
			0, BLOCK(0.5f)
		),
		std::pair(
			EulerAngles(ANGLE(-10.0f), ANGLE(-30.0f), ANGLE(-10.0f)),
			EulerAngles(ANGLE(10.0f), ANGLE(30.0f), ANGLE(10.0f))
		)
	};

	void TurnSwitchCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll)
	{
		auto* laraInfo = GetLaraInfo(laraItem);
		auto* switchItem = &g_Level.Items[itemNumber];

		int doSwitch = 0;

		if (IsHeld(In::Action) &&
			laraItem->Animation.ActiveState == LS_IDLE &&
			laraItem->Animation.AnimNumber == LA_STAND_IDLE &&
			laraItem->Animation.IsAirborne == false &&
			laraInfo->Control.HandStatus == HandStatus::Free &&
			switchItem->Animation.ActiveState == TURN_SWITCH_STOP ||
			laraInfo->Control.IsMoving && laraInfo->Context.InteractedItem == itemNumber)
		{
			if (TestLaraPosition(TurnSwitchBoundsA, switchItem, laraItem))
			{
				if (MoveLaraPosition(TurnSwitchPosA, switchItem, laraItem))
				{
					SetAnimation(*laraItem, LA_TURNSWITCH_GRAB_COUNTER_CLOCKWISE);
					SetAnimation(*switchItem, 4);
					switchItem->ItemFlags[0] = TURN_SWITCH_ANTICLOCKWISE;

					ForcedFixedCamera.x = switchItem->Pose.Position.x - BLOCK(1) * phd_sin(switchItem->Pose.Orientation.y);
					ForcedFixedCamera.z = switchItem->Pose.Position.z - BLOCK(1) * phd_cos(switchItem->Pose.Orientation.y);

					doSwitch = -1;
				}
				else
				{
					laraInfo->Context.InteractedItem = itemNumber;
				}
			}
			else
			{
				laraItem->Pose.Orientation.y ^= (short)ANGLE(180.0f);
				if (TestLaraPosition(TurnSwitchBoundsC, switchItem, laraItem))
				{
					if (MoveLaraPosition(TurnSwitchPos, switchItem, laraItem))
					{
						SetAnimation(*laraItem, LA_TURNSWITCH_GRAB_CLOCKWISE);

						switchItem->ItemFlags[0] = TURN_SWITCH_CLOCKWISE;

						ForcedFixedCamera.x = switchItem->Pose.Position.x + 1024 * phd_sin(switchItem->Pose.Orientation.y);
						ForcedFixedCamera.z = switchItem->Pose.Position.z + 1024 * phd_cos(switchItem->Pose.Orientation.y);

						doSwitch = 1;
					}
					else
					{
						laraInfo->Context.InteractedItem = itemNumber;
					}
				}
				else if (laraInfo->Control.IsMoving && laraInfo->Context.InteractedItem == itemNumber)
				{
					laraInfo->Control.IsMoving = false;
					laraInfo->Control.HandStatus = HandStatus::Free;
				}

				laraItem->Pose.Orientation.y ^= (short)ANGLE(180.0f);
			}
		}

		if (doSwitch)
		{
			short ItemNos[8];

			laraInfo->Control.IsMoving = false;
			ResetPlayerFlex(laraItem);
			laraInfo->Control.HandStatus = HandStatus::Busy;
			laraItem->Animation.ActiveState = LA_REACH;

			UseForcedFixedCamera = true;
			ForcedFixedCamera.y = switchItem->Pose.Position.y - 2048;
			ForcedFixedCamera.RoomNumber = switchItem->RoomNumber;
			Camera.DisableInterpolation = true;

			AddActiveItem(itemNumber);

			switchItem->Status = ITEM_ACTIVE;
			switchItem->ItemFlags[1] = 0;

			if (GetSwitchTrigger(switchItem, ItemNos, 0))
			{
				if (!TriggerActive(&g_Level.Items[ItemNos[0]]))
				{
					g_Level.Items[ItemNos[0]].Animation.AnimNumber = 0;
					g_Level.Items[ItemNos[0]].Animation.FrameNumber = 0;
				}
			}
		}
		else
		{
			if (coll->Setup.EnableObjectPush && TestBoundsCollide(switchItem, laraItem, coll->Setup.Radius))
			{
				GlobalCollisionBounds.X1 = -512;
				GlobalCollisionBounds.X2 = 512;
				GlobalCollisionBounds.Y1 = -512;
				GlobalCollisionBounds.Y2 = 0;
				GlobalCollisionBounds.Z1 = -512;
				GlobalCollisionBounds.Z2 = 512;

				ItemPushItem(switchItem, laraItem, coll, 0, 2);

				GlobalCollisionBounds.X1 = 256;
				GlobalCollisionBounds.X2 = 1024;
				GlobalCollisionBounds.Z1 = -128;
				GlobalCollisionBounds.Z2 = 128;

				ItemPushItem(switchItem, laraItem, coll, 0, 2);
			}
		}
	}

	void TurnSwitchControl(short itemNumber)
	{
		auto* laraItem = LaraItem;
		auto* switchItem = &g_Level.Items[itemNumber];

		if (g_Level.Items[itemNumber].ItemFlags[0] == TURN_SWITCH_CLOCKWISE)
		{
			if (switchItem->Animation.AnimNumber == 2)
			{
				switchItem->Pose.Orientation.y += ANGLE(90.0f);
				switchItem->DisableInterpolation = true;

				if (IsHeld(In::Action))
				{
					laraItem->Animation.AnimNumber = LA_TURNSWITCH_PUSH_CLOCKWISE_START;
					laraItem->Animation.FrameNumber = 0;

					switchItem->Animation.AnimNumber = 1;
					switchItem->Animation.FrameNumber = 0;
				}
			}

			if (laraItem->Animation.AnimNumber == LA_TURNSWITCH_PUSH_CLOCKWISE_END &&
				TestLastFrame(*laraItem) && 
				!switchItem->ItemFlags[1])
			{
				switchItem->ItemFlags[1] = 1;
				switchItem->DisableInterpolation = true;
			}

			if ((laraItem->Animation.FrameNumber >= 0 &&
				laraItem->Animation.FrameNumber <= 43) ||
				(laraItem->Animation.FrameNumber >= 58 &&
				laraItem->Animation.FrameNumber <= 115))
			{
				SoundEffect(SFX_TR4_PUSHABLE_SOUND, &switchItem->Pose, SoundEnvironment::Always);
			}
		}
		else
		{
			if (switchItem->Animation.AnimNumber == 6)
			{
				switchItem->Pose.Orientation.y -= ANGLE(90.0f);
				switchItem->DisableInterpolation = true;

				if (IsHeld(In::Action))
				{
					SetAnimation(*laraItem, LA_TURNSWITCH_PUSH_COUNTER_CLOCKWISE_START);
					SetAnimation(*switchItem, 5);
				}
			}

			if (laraItem->Animation.AnimNumber == LA_TURNSWITCH_PUSH_COUNTER_CLOCKWISE_END &&
				TestLastFrame(*laraItem) &&
				!switchItem->ItemFlags[1])
			{
				switchItem->ItemFlags[1] = 1;
				switchItem->DisableInterpolation = true;
			}

			if ((laraItem->Animation.FrameNumber >= 0 &&
				laraItem->Animation.FrameNumber <= 43) ||
				(laraItem->Animation.FrameNumber >= 58 &&
				laraItem->Animation.FrameNumber <= 115))
			{
				SoundEffect(SFX_TR4_PUSHABLE_SOUND, &switchItem->Pose, SoundEnvironment::Always);
			}
		}

		AnimateItem(*switchItem);

		if (switchItem->ItemFlags[1] == 1)
		{
			laraItem->Animation.AnimNumber = LA_STAND_IDLE;
			laraItem->Animation.FrameNumber = 0;
			laraItem->Animation.ActiveState = LS_IDLE;
			switchItem->Animation.AnimNumber = 0;
			switchItem->Animation.FrameNumber = 0;
			switchItem->Status = ITEM_NOT_ACTIVE;

			RemoveActiveItem(itemNumber);

			Lara.Control.HandStatus = HandStatus::Free;
			UseForcedFixedCamera = 0;
			Camera.DisableInterpolation = true;
			switchItem->ItemFlags[1] = 2;
		}
	}
}
