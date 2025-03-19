#include "framework.h"
#include "Objects/Generic/Doors/underwater_door.h"

#include "Game/Animation/Animation.h"
#include "Game/collision/collide_item.h"
#include "Game/control/box.h"
#include "Game/control/control.h"
#include "Game/control/lot.h"
#include "Game/Gui.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Lara/lara_struct.h"
#include "Game/misc.h"
#include "Game/pickup/pickup.h"
#include "Math/Math.h"
#include "Objects/Generic/Doors/generic_doors.h"
#include "Sound/sound.h"
#include "Specific/Input/Input.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Input;

namespace TEN::Entities::Doors
{
	const auto UnderwaterDoorPos = Vector3i(-251, -540, -46);
	const ObjectCollisionBounds UnderwaterDoorBounds =
	{
		GameBoundingBox(
			-BLOCK(3 / 4.0f), BLOCK(3 / 4.0f),
			-BLOCK(1), 0,
			-BLOCK(1 / 2.0f), 0
		),
		std::pair(
			EulerAngles(ANGLE(-80.0f), ANGLE(-80.0f), ANGLE(-80.0f)),
			EulerAngles(ANGLE(80.0f), ANGLE(80.0f), ANGLE(80.0f))
		)
	};

	void UnderwaterDoorCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll)
	{
		auto* laraInfo = GetLaraInfo(laraItem);
		auto* doorItem = &g_Level.Items[itemNumber];

		if (IsHeld(In::Action) &&
			laraItem->Animation.ActiveState == LS_UNDERWATER_IDLE &&
			laraInfo->Control.WaterStatus == WaterStatus::Underwater &&
			doorItem->Status != ITEM_ACTIVE &&
			!doorItem->Animation.IsAirborne &&
			laraInfo->Control.HandStatus == HandStatus::Free ||
			laraInfo->Control.IsMoving && laraInfo->Context.InteractedItem == itemNumber)
		{
			laraItem->Pose.Orientation.y ^= ANGLE(180.0f);

			if (TestLaraPosition(UnderwaterDoorBounds, doorItem, laraItem))
			{
				if (MoveLaraPosition(UnderwaterDoorPos, doorItem, laraItem))
				{
					SetAnimation(*laraItem, LA_UNDERWATER_DOOR_OPEN);
					laraItem->Animation.Velocity.y = 0;
					doorItem->Status = ITEM_ACTIVE;

					AddActiveItem(itemNumber);

					doorItem->Animation.TargetState = LS_RUN_FORWARD;

					AnimateItem(*doorItem);

					laraInfo->Control.IsMoving = false;
					laraInfo->Control.HandStatus = HandStatus::Busy;
				}
				else
					laraInfo->Context.InteractedItem = itemNumber;

				laraItem->Pose.Orientation.y ^= ANGLE(180.0f);
			}
			else
			{
				if (laraInfo->Control.IsMoving && laraInfo->Context.InteractedItem == itemNumber)
				{
					laraInfo->Control.IsMoving = false;
					laraInfo->Control.HandStatus = HandStatus::Free;
				}

				laraItem->Pose.Orientation.y ^= ANGLE(180.0f);
			}
		}
		else if (doorItem->Status == ITEM_ACTIVE)
			ObjectCollision(itemNumber, laraItem, coll);
	}
}
