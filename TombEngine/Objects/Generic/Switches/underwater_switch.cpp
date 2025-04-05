#include "framework.h"
#include "Objects/Generic/Switches/underwater_switch.h"

#include "Game/animation.h"
#include "Game/camera.h"
#include "Game/collision/collide_item.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Objects/Generic/Switches/generic_switch.h"
#include "Specific/Input/Input.h"
#include "Specific/level.h"

using namespace TEN::Input;

namespace TEN::Entities::Switches
{ 
	//TODO Cleanup code for above ground. Lara stops sometimes when CTRL is not pressed.
	const auto GroundSwitchPos = Vector3i(0, 0, 128);
	const auto UnderwaterSwitchPos = Vector3i(0, -560, 108);
	const ObjectCollisionBounds UnderwaterSwitchBounds =
	{
		GameBoundingBox(
			-BLOCK(3.0f / 8), BLOCK(3.0f / 8),
			-BLOCK(1.0f), 0,
			-BLOCK(1.0f / 4), BLOCK(3 / 4.0f)
		),
		std::pair(
			EulerAngles(ANGLE(-80.0f), ANGLE(-80.0f), ANGLE(-80.0f)),
			EulerAngles(ANGLE(80.0f), ANGLE(80.0f), ANGLE(80.0f))
		)
	};

	const auto CeilingUnderwaterSwitchPos1 = Vector3i(0, -736, -416);
	const ObjectCollisionBounds CeilingUnderwaterSwitchBounds1 =
	{
		GameBoundingBox(
			-BLOCK(3.0f / 8), BLOCK(3.0f / 8),
			-BLOCK(17.0f / 16), -BLOCK(1 / 2.0f),
			-BLOCK(1 / 2.0f), 0
		),
		std::pair(
			EulerAngles(ANGLE(-80.0f), ANGLE(-80.0f), ANGLE(-80.0f)),
			EulerAngles(ANGLE(80.0f), ANGLE(80.0f), ANGLE(80.0f))
		)
	};

	const auto CeilingUnderwaterSwitchPos2 = Vector3i(0, -736, 416);
	const ObjectCollisionBounds CeilingUnderwaterSwitchBounds2 =
	{
		GameBoundingBox(
			-BLOCK(3.0f / 8), BLOCK(3.0f / 8),
			-BLOCK(17.0f / 16), -BLOCK(1 / 2.0f),
			0, BLOCK(1 / 2.0f)
		),
		std::pair(
			EulerAngles(ANGLE(-80.0f), ANGLE(-80.0f), ANGLE(-80.0f)),
			EulerAngles(ANGLE(80.0f), ANGLE(80.0f), ANGLE(80.0f))
		)

	
	};
	const auto CeilingSwitchPos = Vector3i(0, BLOCK(1 / 32), 0);
	const ObjectCollisionBounds CeilingSwitchBounds1 =
	{
		GameBoundingBox(
			-BLOCK(3.0f / 8), BLOCK(3.0f / 8),
			-BLOCK(17.0f / 16), 0,
			-BLOCK(1 / 2.0f), 0
		),
		std::pair(
			EulerAngles(ANGLE(-80.0f), ANGLE(-80.0f), ANGLE(-80.0f)),
			EulerAngles(ANGLE(80.0f), ANGLE(80.0f), ANGLE(80.0f))
		)
	};

	const ObjectCollisionBounds CeilingSwitchBounds2 =
	{
		GameBoundingBox(
			-BLOCK(3.0f / 8), BLOCK(3.0f / 8),
			-BLOCK(17.0f / 16), 0,
			0, BLOCK(1 / 2.0f)
		),
		std::pair(
			EulerAngles(ANGLE(-80.0f), ANGLE(-80.0f), ANGLE(-80.0f)),
			EulerAngles(ANGLE(80.0f), ANGLE(80.0f), ANGLE(80.0f))
		)


	};
	void CollideUnderwaterWallSwitch(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll)
	{
		auto* player = GetLaraInfo(laraItem);
		auto* switchItem = &g_Level.Items[itemNumber];

		bool isUnderwater = (player->Control.WaterStatus == WaterStatus::Underwater);

		const auto& position = isUnderwater ? UnderwaterSwitchPos : GroundSwitchPos;

		bool isActionActive = player->Control.IsMoving && player->Context.InteractedItem == itemNumber;
		bool isActionReady = IsHeld(In::Action) && !IsHeld(In::Jump);
		bool isPlayerAvailable = player->Control.HandStatus == HandStatus::Free && switchItem->Status == ITEM_NOT_ACTIVE;

		bool isPlayerIdle = (!isUnderwater && laraItem->Animation.ActiveState == LS_IDLE && laraItem->Animation.AnimNumber == LA_STAND_IDLE) ||
			(isUnderwater && laraItem->Animation.ActiveState == LS_UNDERWATER_IDLE && laraItem->Animation.AnimNumber == LA_UNDERWATER_IDLE);

		if (isActionActive || (isActionReady && isPlayerAvailable && isPlayerIdle))
		{
			if (TestLaraPosition(UnderwaterSwitchBounds, switchItem, laraItem))
			{
				if (MoveLaraPosition(position, switchItem, laraItem))
				{	
					if (switchItem->Animation.ActiveState == SWITCH_OFF)
					{
						SetAnimation(laraItem, isUnderwater ? LA_WATERLEVER_PULL : LA_WALL_LEVER_SWITCH);
						switchItem->Animation.TargetState = SWITCH_ON;
					}
					else
					{
						SetAnimation(laraItem, isUnderwater ? LA_WATERLEVER_PULL : LA_WALL_LEVER_SWITCH);
						switchItem->Animation.TargetState = SWITCH_OFF;
					}

					ResetPlayerFlex(laraItem);
					laraItem->Animation.FrameNumber = GetAnimData(laraItem).frameBase;
					laraItem->Animation.Velocity.y = 0;
					laraItem->Animation.TargetState = isUnderwater ? LS_UNDERWATER_IDLE : LS_IDLE;
					player->Control.IsMoving = false;
					player->Control.HandStatus = HandStatus::Busy;

					AddActiveItem(itemNumber);
					switchItem->Status = ITEM_ACTIVE;
					AnimateItem(switchItem);
				}
				else
					player->Context.InteractedItem = itemNumber;
			}
			else if (player->Control.IsMoving && player->Context.InteractedItem == itemNumber)
			{
				player->Control.IsMoving = false;
				player->Control.HandStatus = HandStatus::Free;
			}

			return;	
		}
	}

	void CollideUnderwaterCeilingSwitch(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll)
	{
		auto* lara = GetLaraInfo(laraItem);
		auto* switchItem = &g_Level.Items[itemNumber];
		bool doInteraction = false;

		bool isUnderwater = (lara->Control.WaterStatus == WaterStatus::Underwater);
		if (isUnderwater)
		{

			if ((IsHeld(In::Action) &&
				laraItem->Animation.ActiveState == LS_UNDERWATER_IDLE &&
				laraItem->Animation.AnimNumber == LA_UNDERWATER_IDLE &&
				lara->Control.WaterStatus == WaterStatus::Underwater &&
				lara->Control.HandStatus == HandStatus::Free &&
				switchItem->Animation.ActiveState == SWITCH_OFF) ||
				(lara->Control.IsMoving && lara->Context.InteractedItem == itemNumber))
			{
				if (TestLaraPosition(CeilingUnderwaterSwitchBounds1, switchItem, laraItem))
				{
					if (MoveLaraPosition(CeilingUnderwaterSwitchPos1, switchItem, laraItem))
						doInteraction = true;
					else
						lara->Context.InteractedItem = itemNumber;
				}
				else
				{
					laraItem->Pose.Orientation.y ^= (short)ANGLE(180.0f);

					if (TestLaraPosition(CeilingUnderwaterSwitchBounds2, switchItem, laraItem))
					{
						if (MoveLaraPosition(CeilingUnderwaterSwitchPos2, switchItem, laraItem))
							doInteraction = true;
						else
							lara->Context.InteractedItem = itemNumber;
					}

					laraItem->Pose.Orientation.y ^= (short)ANGLE(180.0f);
				}

				if (doInteraction)
				{
					SetAnimation(laraItem, LA_UNDERWATER_CEILING_SWITCH_PULL);
					laraItem->Animation.TargetState = LS_UNDERWATER_IDLE;
					laraItem->Animation.Velocity.y = 0;
					lara->Control.IsMoving = false;
					lara->Control.HandStatus = HandStatus::Busy;
					switchItem->Animation.TargetState = SWITCH_ON;
					switchItem->Status = ITEM_ACTIVE;

					AddActiveItem(itemNumber);

					ForcedFixedCamera.x = switchItem->Pose.Position.x - BLOCK(1) * phd_sin(switchItem->Pose.Orientation.y + ANGLE(90.0f));
					ForcedFixedCamera.y = switchItem->Pose.Position.y - BLOCK(1);
					ForcedFixedCamera.z = switchItem->Pose.Position.z - BLOCK(1) * phd_cos(switchItem->Pose.Orientation.y + ANGLE(90.0f));
					ForcedFixedCamera.RoomNumber = switchItem->RoomNumber;
				}
			}
		}
		else
		{

			if ((IsHeld(In::Action) &&
				laraItem->Animation.ActiveState == LS_JUMP_UP &&
				laraItem->Animation.IsAirborne &&
				lara->Control.HandStatus == HandStatus::Free &&
				switchItem->Status != ITEM_ACTIVE ||
				(lara->Control.IsMoving && lara->Context.InteractedItem == itemNumber)))
			{
				if (TestLaraPosition(CeilingSwitchBounds1, switchItem, laraItem))
				{
					if (AlignLaraPosition(CeilingSwitchPos, switchItem, laraItem))
						doInteraction = true;
					else
						lara->Context.InteractedItem = itemNumber;
				}
				else
				{
					laraItem->Pose.Orientation.y ^= (short)ANGLE(180.0f);

					if (TestLaraPosition(CeilingSwitchBounds2, switchItem, laraItem))
					{
						if (AlignLaraPosition(CeilingSwitchPos, switchItem, laraItem))
							doInteraction = true;
						else
							lara->Context.InteractedItem = itemNumber;
					}

					laraItem->Pose.Orientation.y ^= (short)ANGLE(180.0f);
				}

				if (doInteraction)
				{
					ResetPlayerFlex(laraItem);
					laraItem->Animation.Velocity.y = 0;
					laraItem->Animation.IsAirborne = false;
					laraItem->Animation.AnimNumber = LA_CEILING_LEVER_SWITCH;
					laraItem->Animation.FrameNumber = GetAnimData(laraItem).frameBase;
					laraItem->Animation.ActiveState = LS_FREEFALL_BIS;
					lara->Control.HandStatus = HandStatus::Busy;
					AddActiveItem(itemNumber);
					switchItem->Status = ITEM_ACTIVE;
					switchItem->Animation.TargetState = SWITCH_ANIMATE;

					ForcedFixedCamera.x = switchItem->Pose.Position.x - BLOCK(1) * phd_sin(switchItem->Pose.Orientation.y + ANGLE(90.0f));
					ForcedFixedCamera.y = switchItem->Pose.Position.y - BLOCK(1);
					ForcedFixedCamera.z = switchItem->Pose.Position.z - BLOCK(1) * phd_cos(switchItem->Pose.Orientation.y + ANGLE(90.0f));
					ForcedFixedCamera.RoomNumber = switchItem->RoomNumber;
				}
			}
		}
	}
}
