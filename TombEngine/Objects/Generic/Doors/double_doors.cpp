#include "framework.h"

#include "Game/animation.h"
#include "Game/collision/collide_item.h"
#include "Game/control/box.h"
#include "Game/control/control.h"
#include "Game/control/lot.h"
#include "Game/Gui.h"
#include "Game/Hud/Hud.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Lara/lara_struct.h"
#include "Game/misc.h"
#include "Game/pickup/pickup.h"
#include "Math/Math.h"
#include "Specific/Input/Input.h"
#include "Specific/level.h"
#include "Sound/sound.h"
#include "Objects/Generic/Doors/double_doors.h"
#include "Objects/Generic/Doors/generic_doors.h"

using namespace TEN::Hud;
using namespace TEN::Input;

namespace TEN::Entities::Doors
{
	const auto DoubleDoorPos = Vector3i(0, 0, 220);
	const ObjectCollisionBounds DoubleDoorBounds =
	{
		GameBoundingBox(
			-384, 384,
			0, 0, 
			-BLOCK(1), BLOCK(0.5f)
		),
		std::pair(
			EulerAngles(ANGLE(-10.0f), ANGLE(-30.0f), ANGLE(-10.0f)),
			EulerAngles(ANGLE(10.0f), ANGLE(30.0f), ANGLE(10.0f))
		)
	};

	void DoubleDoorCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll)
	{
		auto* laraInfo = GetLaraInfo(laraItem);
		auto* doorItem = &g_Level.Items[itemNumber];

		g_Hud.InteractionHighlighter.Test(*laraItem, *doorItem, InteractionMode::Activation);

		if (IsHeld(In::Action) &&
			laraItem->Animation.ActiveState == LS_IDLE &&
			laraItem->Animation.AnimNumber == LA_STAND_IDLE &&
			!laraItem->HitStatus &&
			doorItem->Status != ITEM_ACTIVE &&
			!doorItem->Animation.IsAirborne &&
			laraInfo->Control.HandStatus == HandStatus::Free ||
			laraInfo->Control.IsMoving && laraInfo->Context.InteractedItem == itemNumber)
		{
			doorItem->Pose.Orientation.y ^= ANGLE(180.0f);

			if (TestLaraPosition(DoubleDoorBounds, doorItem, laraItem))
			{
				if (MoveLaraPosition(DoubleDoorPos, doorItem, laraItem))
				{
					SetAnimation(laraItem, LA_DOUBLEDOOR_OPEN_PUSH);

					AddActiveItem(itemNumber);

					ResetPlayerFlex(laraItem);
					laraInfo->Control.IsMoving = false;
					laraInfo->Control.HandStatus = HandStatus::Busy;
					doorItem->Status = ITEM_ACTIVE;
				}
				else
					laraInfo->Context.InteractedItem = itemNumber;

				doorItem->Pose.Orientation.y ^= ANGLE(180.0f);
			}
			else
			{
				if (laraInfo->Control.IsMoving &&
					laraInfo->Context.InteractedItem == itemNumber)
				{
					laraInfo->Control.IsMoving = false;
					laraInfo->Control.HandStatus = HandStatus::Free;
				}

				doorItem->Pose.Orientation.y ^= ANGLE(180.0f);
			}
		}
	}
}
