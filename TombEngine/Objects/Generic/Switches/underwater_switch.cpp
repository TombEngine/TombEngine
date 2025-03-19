#include "framework.h"
#include "Objects/Generic/Switches/underwater_switch.h"

#include "Game/Animation/Animation.h"
#include "Game/camera.h"
#include "Game/collision/collide_item.h"
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
	const auto UnderwaterSwitchPos = Vector3i(0, 0, 108);
	const ObjectCollisionBounds UnderwaterSwitchBounds =
	{
		GameBoundingBox(
			-BLOCK(3.0f / 8), BLOCK(3.0f / 8),
			-BLOCK(3.0f / 8), BLOCK(3.0f / 8),
			0, BLOCK(3 / 4.0f)
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

	void CollideUnderwaterWallSwitch(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll)
	{
		auto* lara = GetLaraInfo(laraItem);
		auto* switchItem = &g_Level.Items[itemNumber];

		if (IsHeld(In::Action) && !IsHeld(In::Jump) &&
			switchItem->Status == ITEM_NOT_ACTIVE &&
			lara->Control.WaterStatus == WaterStatus::Underwater &&
			lara->Control.HandStatus == HandStatus::Free &&
			laraItem->Animation.ActiveState == LS_UNDERWATER_IDLE)
		{
			if (TestLaraPosition(UnderwaterSwitchBounds, switchItem, laraItem))
			{
				if (switchItem->Animation.ActiveState == SWITCH_ON ||
					switchItem->Animation.ActiveState == SWITCH_OFF)
				{
					if (MoveLaraPosition(UnderwaterSwitchPos, switchItem, laraItem))
					{
						SetAnimation(*laraItem, LA_WATERLEVER_PULL);
						laraItem->Animation.Velocity.y = 0;
						laraItem->Animation.TargetState = LS_UNDERWATER_IDLE;
						
						lara->Control.HandStatus = HandStatus::Busy;
						switchItem->Animation.TargetState = switchItem->Animation.ActiveState != SWITCH_ON;
						switchItem->Status = ITEM_ACTIVE;

						AddActiveItem(itemNumber);
						AnimateItem(*switchItem);
					}
				}
			}
		}
	}

	void CollideUnderwaterCeilingSwitch(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll)
	{
		auto* lara = GetLaraInfo(laraItem);
		auto* switchItem = &g_Level.Items[itemNumber];

		bool doInteraction = false;

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
				SetAnimation(*laraItem, LA_UNDERWATER_CEILING_SWITCH_PULL);
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
}
