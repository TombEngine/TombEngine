#include "framework.h"
#include "Objects/TR4/Object/tr4_sarcophagus.h"

#include "Game/collision/collide_item.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/pickup/pickup.h"
#include "Game/Setup.h"
#include "Specific/Input/Input.h"
#include "Specific/level.h"

using namespace TEN::Input;

const auto SarcophagusPosition = Vector3i(0, 0, -300);
const ObjectCollisionBounds SarcophagusBounds =
{
	GameBoundingBox(
		-BLOCK(0.5f), BLOCK(0.5f),
		-100, 100,
		-BLOCK(0.5f), 0
	),
		std::pair(
			EulerAngles(ANGLE(-10.0f), ANGLE(-30.0f), 0),
			EulerAngles(ANGLE(10.0f), ANGLE(30.0f), 0)
		)
};

void SarcophagusCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll)
{
	auto* laraInfo = GetLaraInfo(laraItem);
	auto* sarcItem = &g_Level.Items[itemNumber];

	if (IsHeld(In::Action) &&
		laraItem->Animation.ActiveState == LS_IDLE &&
		laraItem->Animation.AnimNumber == LA_STAND_IDLE &&
		laraInfo->Control.HandStatus == HandStatus::Free &&
		sarcItem->Status != ITEM_ACTIVE ||
		laraInfo->Control.IsMoving && laraInfo->Context.InteractedItem == itemNumber)
	{
		if (TestLaraPosition(SarcophagusBounds, sarcItem, laraItem))
		{
			if (MoveLaraPosition(SarcophagusPosition, sarcItem, laraItem))
			{
				laraItem->Animation.AnimNumber = LA_PICKUP_SARCOPHAGUS;
				laraItem->Animation.ActiveState = LS_MISC_CONTROL;
				laraItem->Animation.FrameNumber = 0;
				sarcItem->Flags |= IFLAG_ACTIVATION_MASK;

				AddActiveItem(itemNumber);
				sarcItem->Status = ITEM_ACTIVE;

				laraInfo->Control.IsMoving = false;
				ResetPlayerFlex(laraItem);
				laraInfo->Control.HandStatus = HandStatus::Busy;
			}
			else
				laraInfo->Context.InteractedItem = itemNumber;
		}
		else if (laraInfo->Control.IsMoving)
		{
			if (laraInfo->Context.InteractedItem == itemNumber)
			{
				laraInfo->Control.IsMoving = false;
				laraInfo->Control.HandStatus = HandStatus::Free;
			}
		}
	}
	else if (laraItem->Animation.AnimNumber != LA_PICKUP_SARCOPHAGUS ||
			 laraItem->Animation.FrameNumber != 113)
	{
		ObjectCollision(itemNumber, laraItem, coll);
	}
	else
	{
		CollectCarriedItems(sarcItem);
	}
}
