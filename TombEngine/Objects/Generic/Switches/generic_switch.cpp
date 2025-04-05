#include "framework.h"
#include "Game/animation.h"
#include "Game/collision/collide_item.h"
#include "Game/control/control.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Objects/Generic/Switches/generic_switch.h"
#include "Specific/Input/Input.h"
#include "Specific/level.h"

using namespace TEN::Input;

namespace TEN::Entities::Switches
{
	ObjectCollisionBounds SwitchBounds =
	{
		GameBoundingBox::Zero,
		std::pair(
			EulerAngles(ANGLE(-10.0f), ANGLE(-30.0f), ANGLE(-10.0f)),
			EulerAngles(ANGLE(10.0f), ANGLE(30.0f), ANGLE(10.0f))
		)
	};
	auto SwitchPos = Vector3i::Zero;

	auto UnderwaterSwitchPos = Vector3i(0, -512, 0);
	const ObjectCollisionBounds UnderwaterSwitchBounds =
	{
		GameBoundingBox(
			-BLOCK(0.5f), BLOCK(0.5f),
			-BLOCK(1), 0,
			-BLOCK(0.5f), BLOCK(0.25f)
		),
		std::pair(
			EulerAngles(ANGLE(-80.0f), ANGLE(-80.0f), ANGLE(-80.0f)),
			EulerAngles(ANGLE(80.0f), ANGLE(80.0f), ANGLE(80.0f))
		)
	};

	enum SwitchOCBs
	{
		SWT_BIG_LEVER = 0,
		SWT_SMALL_LEVER = 1,
		SWT_SMALL_BUTTON = 2,
		SWT_BIG_BUTTON = 3,
		SWT_GIANT_BUTTON = 4,
		SWT_VALVE = 5,
		SWT_WALL_HOLE = 6,
		SWT_CUSTOM = 7
	};

	void SwitchControl(short itemNumber)
	{
		auto* switchItem = &g_Level.Items[itemNumber];

		switchItem->Flags |= CODE_BITS;

		if (!TriggerActive(switchItem) && !(switchItem->Flags & IFLAG_INVISIBLE))
		{
			if (switchItem->ObjectNumber == ID_JUMP_SWITCH)
			{
				switchItem->Animation.TargetState = SWITCH_OFF;
				switchItem->Timer = 0;
				AnimateItem(switchItem);
			}
			else
			{
				switchItem->Animation.TargetState = SWITCH_ON;
				switchItem->Timer = 0;
			}
		}

		AnimateItem(switchItem);
	}

	void SwitchCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll)
	{
		auto* laraInfo = GetLaraInfo(laraItem);
		auto* switchItem = &g_Level.Items[itemNumber];

		bool isUnderwater = (laraInfo->Control.WaterStatus == WaterStatus::Underwater);

		bool isActionActive = laraInfo->Control.IsMoving && laraInfo->Context.InteractedItem == itemNumber;
		bool isActionReady = IsHeld(In::Action);
		bool isPlayerAvailable = laraInfo->Control.HandStatus == HandStatus::Free && switchItem->Status == ITEM_NOT_ACTIVE && !(switchItem->Flags & ONESHOT) &&	switchItem->TriggerFlags >= 0;

		bool isPlayerIdle = (!isUnderwater && laraItem->Animation.ActiveState == LS_IDLE && laraItem->Animation.AnimNumber == LA_STAND_IDLE) ||
			(isUnderwater && laraItem->Animation.ActiveState == LS_UNDERWATER_IDLE && laraItem->Animation.AnimNumber == LA_UNDERWATER_IDLE);
		
		if (isActionActive || (isActionReady && isPlayerAvailable && isPlayerIdle))
		{
			auto bounds = GameBoundingBox(switchItem);

			//Temporary code to avoid pulling any other switch in the water. This can be removed once all switches are enabled underwater.
			if (switchItem->TriggerFlags != 3 && isUnderwater)
				return;

			if ((switchItem->TriggerFlags == 3 || switchItem->TriggerFlags == 4) && switchItem->Animation.ActiveState == SWITCH_OFF)
				return;

			SwitchBounds.BoundingBox.X1 = bounds.X1 - BLOCK(0.25f);
			SwitchBounds.BoundingBox.X2 = bounds.X2 + BLOCK(0.25f);

			switch (switchItem->TriggerFlags)
			{
				default:
					SwitchPos.z = bounds.Z1 - 128;
					SwitchBounds.BoundingBox.Z1 = bounds.Z1 - BLOCK(0.2f);
					SwitchBounds.BoundingBox.Z2 = bounds.Z2;
					break;

				case SWT_BIG_LEVER:
					SwitchPos.z = bounds.Z1 - 64;
					SwitchBounds.BoundingBox.Z1 = bounds.Z1 - BLOCK(0.2f);
					SwitchBounds.BoundingBox.Z2 = bounds.Z2;
					break;

				case SWT_SMALL_LEVER:
					SwitchPos.z = bounds.Z1 - 112;
					SwitchBounds.BoundingBox.Z1 = bounds.Z1 - BLOCK(0.25f);
					SwitchBounds.BoundingBox.Z2 = bounds.Z2;
					break;

				case SWT_SMALL_BUTTON:
					SwitchPos.z = bounds.Z1 - 156;
					SwitchBounds.BoundingBox.Z1 = bounds.Z1 - BLOCK(0.2f);
					SwitchBounds.BoundingBox.Z2 = bounds.Z2;
					break;

				case SWT_BIG_BUTTON:
					SwitchPos.z = bounds.Z1 - 256;
					SwitchBounds.BoundingBox.Z1 = bounds.Z1 - BLOCK(0.5f);
					SwitchBounds.BoundingBox.Z2 = bounds.Z2;
					break;

				case SWT_GIANT_BUTTON:
					SwitchPos.z = bounds.Z1 - 384;
					SwitchBounds.BoundingBox.Z1 = bounds.Z1 - BLOCK(0.5f);
					SwitchBounds.BoundingBox.Z2 = bounds.Z2;
					break;

				case SWT_VALVE:
					SwitchPos.z = bounds.Z1 - 112;
					SwitchBounds.BoundingBox.Z1 = bounds.Z1 - BLOCK(0.25f);
					SwitchBounds.BoundingBox.Z2 = bounds.Z2;
					break;

				case SWT_WALL_HOLE:
					SwitchPos.z = bounds.Z1 - 196;
					SwitchBounds.BoundingBox.Z1 = bounds.Z1 - BLOCK(0.2f);
					SwitchBounds.BoundingBox.Z2 = bounds.Z2;
					break;

				case SWT_CUSTOM:
					SwitchPos.z = bounds.Z1 - switchItem->ItemFlags[6];
					SwitchBounds.BoundingBox.Z1 = bounds.Z1 - BLOCK(0.5f);
					SwitchBounds.BoundingBox.Z2 = bounds.Z2;
					break;
			}

			const auto& boundSelection = isUnderwater ? UnderwaterSwitchBounds : SwitchBounds;
			const auto& position = isUnderwater ? UnderwaterSwitchPos : SwitchPos;

			if (TestLaraPosition(boundSelection, switchItem, laraItem))
			{
				if (MoveLaraPosition(position, switchItem, laraItem))
				{
					int onAnim = LaraAnim::LA_WALLSWITCH_DOWN;
					int offAnim = LaraAnim::LA_WALLSWITCH_UP;

					switch (switchItem->TriggerFlags)
					{
						case SWT_BIG_LEVER:
							onAnim = LaraAnim::LA_WALLSWITCH_DOWN;
							offAnim = LaraAnim::LA_WALLSWITCH_UP;
							break;

						case SWT_SMALL_LEVER:
							onAnim = LaraAnim::LA_SWITCH_SMALL_DOWN;
							offAnim = LaraAnim::LA_SWITCH_SMALL_UP;
							break;

						case SWT_SMALL_BUTTON:
							onAnim = LaraAnim::LA_BUTTON_SMALL_PUSH;
							offAnim = LaraAnim::LA_BUTTON_SMALL_PUSH;
							break;

						case SWT_BIG_BUTTON:
							onAnim = isUnderwater ? LaraAnim::LA_UNDERWATER_WALL_KICK : LaraAnim::LA_BUTTON_LARGE_PUSH;
							offAnim = LaraAnim::LA_BUTTON_LARGE_PUSH;
							break;

						case SWT_GIANT_BUTTON:
							onAnim = LaraAnim::LA_BUTTON_GIANT_PUSH;
							offAnim = LaraAnim::LA_BUTTON_GIANT_PUSH;
							break;

						case SWT_VALVE:
							onAnim = LaraAnim::LA_VALVE_TURN;
							offAnim = LaraAnim::LA_VALVE_TURN;
							break;

						case SWT_WALL_HOLE:
							onAnim = LaraAnim::LA_HOLESWITCH_ACTIVATE;
							offAnim = LaraAnim::LA_HOLESWITCH_ACTIVATE;
							break;

						case SWT_CUSTOM:
							onAnim = abs(switchItem->ItemFlags[4]);
							offAnim = abs(switchItem->ItemFlags[5]);								
							break;

						default:
							onAnim = (LaraAnim)(switchItem->TriggerFlags);
							offAnim = (LaraAnim)(switchItem->TriggerFlags + 1);
							break;
					}

					if (switchItem->Animation.ActiveState == SWITCH_OFF)
					{
						SetAnimation(laraItem, offAnim);
						switchItem->Animation.TargetState = SWITCH_ON;
					}
					else
					{
						SetAnimation(laraItem, onAnim);
						switchItem->Animation.TargetState = SWITCH_OFF;
					}

					ResetPlayerFlex(laraItem);
					laraItem->Animation.FrameNumber = GetAnimData(laraItem).frameBase;
					laraInfo->Control.IsMoving = false;
					laraInfo->Control.HandStatus = HandStatus::Busy;

					AddActiveItem(itemNumber);
					switchItem->Status = ITEM_ACTIVE;
					AnimateItem(switchItem);
				}
				else
					laraInfo->Context.InteractedItem = itemNumber;
			}
			else if (laraInfo->Control.IsMoving && laraInfo->Context.InteractedItem == itemNumber)
			{
				laraInfo->Control.IsMoving = false;
				laraInfo->Control.HandStatus = HandStatus::Free;
			}

			return;
		}

		if (laraItem->Animation.ActiveState != LS_SWITCH_DOWN && laraItem->Animation.ActiveState != LS_SWITCH_UP)
			ObjectCollision(itemNumber, laraItem, coll);
	}
}
