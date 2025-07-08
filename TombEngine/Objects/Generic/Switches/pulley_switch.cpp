#include "framework.h"
#include "Objects/Generic/Switches/pulley_switch.h"

#include "Game/collision/collide_item.h"
#include "Game/control/control.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/pickup/pickup.h"
#include "Objects/Generic/Switches/generic_switch.h"
#include "Sound/sound.h"
#include "Specific/Input/Input.h"
#include "Specific/level.h"

using namespace TEN::Input;

enum PulleyFlags
{
	OneShot,
	NotHidden,
	PullCountReset,
	PullCount,
	State,
	Status
};

enum PulleyStatus
{
	PULLEY_OFF,
	PULLEY_ON,
	PULLEY_WAIT,
	PULLEY_ANIMATE,
	PULLEY_ANIMATE_UNDERWATER
};

namespace TEN::Entities::Switches
{
	const ObjectCollisionBounds PulleyBounds =
	{
		GameBoundingBox(
			-CLICK(1), CLICK(1),
			0, 0,
			-BLOCK(0.5f), BLOCK(0.5f)
		),
		std::pair(
			EulerAngles(ANGLE(-10.0f), ANGLE(-30.0f), ANGLE(-10.0f)),
			EulerAngles(ANGLE(10.0f), ANGLE(30.0f), ANGLE(10.0f))
		)
	};
	const auto PulleyPos = Vector3i(0, 0, -148);

	const ObjectCollisionBounds UnderwaterPulleyBounds =
	{
		GameBoundingBox(
			-BLOCK(0.5f), BLOCK(0.5f),
			-BLOCK(2.0f), 0,
			-BLOCK(0.5f), BLOCK(0.5f)
		),
		std::pair(
			EulerAngles(ANGLE(-80.0f), ANGLE(-80.0f), ANGLE(-80.0f)),
			EulerAngles(ANGLE(80.0f), ANGLE(80.0f), ANGLE(80.0f))
		)
	};

	//-306.05f is the hardcoded distance in the animation.
	const auto UnderwaterPulleyPos = Vector3i(0, -BLOCK(1.0f), -BLOCK(1.0f / 4));

	void InitializePulleySwitch(short itemNumber)
	{
		auto* switchItem = &g_Level.Items[itemNumber];

		//Add OCB 1 in case its not set. This helps avoid builders having to add a manual OCB every single time they have to use the object.
		if (switchItem->TriggerFlags == 0)
			switchItem->TriggerFlags = 1;

		switchItem->ItemFlags[PulleyFlags::PullCount] = switchItem->TriggerFlags;
		switchItem->TriggerFlags = abs(switchItem->TriggerFlags);

		if (switchItem->Status == ITEM_INVISIBLE)
		{
			switchItem->ItemFlags[PulleyFlags::NotHidden] = 1;
			switchItem->Status = ITEM_NOT_ACTIVE;
		}
	}

	void CollisionPulleySwitch(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll)
	{
		auto* laraInfo = GetLaraInfo(laraItem);
		auto* switchItem = &g_Level.Items[itemNumber];

		bool isUnderwater = (laraInfo->Control.WaterStatus == WaterStatus::Underwater);

		const auto& bounds = isUnderwater ? UnderwaterPulleyBounds : PulleyBounds;
		const auto& position = isUnderwater ? UnderwaterPulleyPos : PulleyPos;

		bool isActionActive = laraInfo->Control.IsMoving && laraInfo->Context.InteractedItem == itemNumber;
		bool isActionReady = IsHeld(In::Action);
		bool isPlayerAvailable = laraInfo->Control.HandStatus == HandStatus::Free;

		//polish animations and locations
		bool isPlayerIdle = (!isUnderwater && laraItem->Animation.ActiveState == LS_IDLE && laraItem->Animation.AnimNumber == LA_STAND_IDLE && laraItem->Animation.IsAirborne == false) ||
			(isUnderwater && laraItem->Animation.ActiveState == LS_UNDERWATER_IDLE && laraItem->Animation.AnimNumber == LA_UNDERWATER_IDLE);
		if (isActionActive || (isActionReady && isPlayerAvailable && isPlayerIdle))
			{
				short oldYrot = switchItem->Pose.Orientation.y;

				if (!isUnderwater)
					switchItem->Pose.Orientation.y = laraItem->Pose.Orientation.y;

				if (TestLaraPosition(bounds, switchItem, laraItem))
				{
					if (switchItem->ItemFlags[PulleyFlags::NotHidden])
					{
						if (OldPickupPos.x != laraItem->Pose.Position.x || OldPickupPos.y != laraItem->Pose.Position.y || OldPickupPos.z != laraItem->Pose.Position.z)
						{
							OldPickupPos.x = laraItem->Pose.Position.x;
							OldPickupPos.y = laraItem->Pose.Position.y;
							OldPickupPos.z = laraItem->Pose.Position.z;
							SayNo();
						}
					}
					else if (MoveLaraPosition(position, switchItem, laraItem))
					{
						ResetPlayerFlex(laraItem);
						laraItem->Animation.AnimNumber = isUnderwater ? LA_UNDERWATER_PULLEY_GRAB : LA_PULLEY_GRAB;
						laraItem->Animation.ActiveState = LS_PULLEY;
						laraItem->Animation.TargetState = LS_PULLEY;
						laraItem->Animation.Velocity = Vector3::Zero;
						laraItem->Animation.FrameNumber = GetAnimData(laraItem).frameBase;
						laraInfo->Control.IsMoving = false;
						laraInfo->Control.HandStatus = HandStatus::Busy;
						laraInfo->Context.InteractedItem = itemNumber;

						AddActiveItem(itemNumber);
						switchItem->Status = ITEM_ACTIVE;
						switchItem->Animation.TargetState = isUnderwater ? PULLEY_ANIMATE_UNDERWATER : PULLEY_ANIMATE;

						if (!isUnderwater)
							switchItem->Pose.Orientation.y = oldYrot;
					}
					else
						laraInfo->Context.InteractedItem = itemNumber;

					if (!isUnderwater)
						switchItem->Pose.Orientation.y = oldYrot;
				}
				else
				{
					if (laraInfo->Control.IsMoving && laraInfo->Context.InteractedItem == itemNumber)
					{
						laraInfo->Control.IsMoving = false;
						laraInfo->Control.HandStatus = HandStatus::Free;
					}

					if (!isUnderwater)
						switchItem->Pose.Orientation.y = oldYrot;
				}
		}
		else if (laraItem->Animation.ActiveState != LS_PULLEY)
			ObjectCollision(itemNumber, laraItem, coll);
	}

	void ControlPulleySwitch(short itemNumber)
	{
		auto* switchItem = &g_Level.Items[itemNumber];
		AnimateItem(switchItem);

		bool isPulling = (LaraItem->Animation.AnimNumber == LA_PULLEY_PULL || LaraItem->Animation.AnimNumber == LA_UNDERWATER_PULLEY_PULL);
		bool isReleasing = (LaraItem->Animation.AnimNumber == LA_PULLEY_RELEASE || LaraItem->Animation.AnimNumber == LA_UNDERWATER_PULLEY_UNGRAB);
		bool isBaseFrame = (LaraItem->Animation.FrameNumber == GetAnimData(*LaraItem).frameBase);

		switch (switchItem->Animation.ActiveState)
		{
		case PulleyStatus::PULLEY_ANIMATE:
			// Decrement TriggerFlags when reaching the base frame
			if (isPulling && isBaseFrame && switchItem->TriggerFlags > 0)
			{
				switchItem->TriggerFlags--;
			}

			// If Ctrl is released, switch goes into WAIT state after animation ends
			if (!IsHeld(In::Action) && isPulling && switchItem->TriggerFlags > 0)
			{
				switchItem->Animation.TargetState = PulleyStatus::PULLEY_WAIT;
				LaraItem->Animation.TargetState = LS_PULLEY_UNGRAB;
			}

			// If Ctrl is released, switch goes into complete state if TriggerFlag = 0 and ItemFlag[2] = 0
			if (isPulling && switchItem->TriggerFlags == 0 && switchItem->ItemFlags[PulleyFlags::PullCountReset] == 0)
			{
				switchItem->ItemFlags[PulleyFlags::PullCountReset] = 1;  // Mark switch as activated

				if (switchItem->ItemFlags[PulleyFlags::State] == 1)
				{
					switchItem->Animation.TargetState = PulleyStatus::PULLEY_OFF;
				}
				else if (switchItem->ItemFlags[PulleyFlags::State] == 0)
				{
					switchItem->Animation.TargetState = PulleyStatus::PULLEY_ON;
				}

				LaraItem->Animation.TargetState = LS_PULLEY_UNGRAB;
				
			}
			break;

		case PulleyStatus::PULLEY_ON:
			// Restore TriggerFlags if the switch is re-used
			if (switchItem->ItemFlags[PulleyFlags::PullCount] >= 0 && switchItem->ItemFlags[PulleyFlags::PullCountReset] == 1)
			{
				switchItem->TriggerFlags = abs(switchItem->ItemFlags[PulleyFlags::PullCount]);
				switchItem->ItemFlags[PulleyFlags::PullCountReset] = 0;  // Reset activation flag
				switchItem->ItemFlags[PulleyFlags::State] = 1; //Save switch is active flag
				switchItem->ItemFlags[PulleyFlags::Status] = 1;
			}
			break;

		case PulleyStatus::PULLEY_OFF:
			// Restore TriggerFlags if the switch is re-used
			if (switchItem->ItemFlags[PulleyFlags::PullCount] >= 0 && switchItem->ItemFlags[PulleyFlags::PullCountReset] == 1)
			{
				switchItem->TriggerFlags = abs(switchItem->ItemFlags[PulleyFlags::PullCount]);
				switchItem->ItemFlags[PulleyFlags::PullCountReset] = 0;  // Reset activation flag
				switchItem->ItemFlags[PulleyFlags::State] = 0; //Save switch is Inactive flag
				switchItem->ItemFlags[PulleyFlags::Status] = 1;
			}
			break;

		case PulleyStatus::PULLEY_WAIT:
			// If Ctrl is pressed again, transition to ANIMATE state (handled in collision code)
			break;
		}
	}

	bool TriggerPulley(short itemNumber, short timer)
	{
		auto& item = g_Level.Items[itemNumber];
		const auto& player = Lara;

		if (item.ItemFlags[PulleyFlags::Status] != 0 && item.ItemFlags[PulleyFlags::State] == 1 &&
			player.Control.HandStatus != HandStatus::Busy)
		{
			item.Flags |= IFLAG_ACTIVATION_MASK;
			item.Status = ITEM_ACTIVE;
			item.ItemFlags[PulleyFlags::Status] = 0;
			return true;
		}

		if (item.ItemFlags[PulleyFlags::Status] != 0 && item.ItemFlags[PulleyFlags::State] == 0 &&
			player.Control.HandStatus != HandStatus::Busy)
		{
			if (timer > 0)
			{
				item.Timer = timer;
				item.Status = ITEM_ACTIVE;
				item.ItemFlags[PulleyFlags::Status] = 0;
				if (timer != 1)
					item.Timer = FPS * timer;

				return true;
			}

			item.Flags |= IFLAG_ACTIVATION_MASK;
			item.Status = ITEM_DEACTIVATED;
			item.ItemFlags[PulleyFlags::Status] = 0;
			return true;
		}

		return false;
	}
}