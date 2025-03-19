#include "framework.h"
#include "tr5_crowdove_switch.h"

#include "Game/Animation/Animation.h"
#include "Game/collision/collide_item.h"
#include "Game/control/control.h"
#include "Game/effects/debris.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Objects/Generic/Switches/generic_switch.h"
#include "Sound/sound.h"
#include "Specific/Input/Input.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Input;

namespace TEN::Entities::Switches
{
	const ObjectCollisionBounds CrowDoveBounds =
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
	const auto CrowDovePos = Vector3i(0, 0, -400);

	void InitializeCrowDoveSwitch(short itemNumber)
	{
		g_Level.Items[itemNumber].MeshBits = 3;
	}

	void CrowDoveSwitchCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll)
	{
		auto* laraInfo = GetLaraInfo(laraItem);
		auto* switchItem = &g_Level.Items[itemNumber];

		if (switchItem->Flags & IFLAG_INVISIBLE ||
			!(switchItem->MeshBits & 4) ||
			(!IsHeld(In::Action) ||
				laraItem->Animation.ActiveState != LS_IDLE ||
				laraItem->Animation.AnimNumber != LA_STAND_IDLE ||
				laraItem->Animation.IsAirborne ||
				laraInfo->Control.HandStatus != HandStatus::Free) &&
			(!laraInfo->Control.IsMoving || laraInfo->Context.InteractedItem != itemNumber))
		{
			if (laraItem->Animation.ActiveState != LS_DOVE_SWITCH)
				ObjectCollision(itemNumber, laraItem, coll);
		}
		else
		{
			int oldYrot = switchItem->Pose.Orientation.y;
			switchItem->Pose.Orientation.y = laraItem->Pose.Orientation.y;
			if (TestLaraPosition(CrowDoveBounds, switchItem, laraItem))
			{
				if (MoveLaraPosition(CrowDovePos, switchItem, laraItem))
				{
					laraItem->Animation.AnimNumber = LA_DOVESWITCH_TURN;
					laraItem->Animation.ActiveState = LS_DOVE_SWITCH;
					laraItem->Animation.FrameNumber = 0;

					AddActiveItem(itemNumber);

					// NOTE: In original TR5 the switch was used together with heavy switches.
					// This little fix make it usable normaly and less hardcoded.
					switchItem->ItemFlags[0] = 0;

					switchItem->Status = ITEM_ACTIVE;
					switchItem->Pose.Orientation.y = oldYrot;
					ResetPlayerFlex(laraItem);
					laraInfo->Control.IsMoving = false;
					laraInfo->Control.HandStatus = HandStatus::Busy;
					laraInfo->Context.InteractedItem = itemNumber;
				}
				else
					laraInfo->Context.InteractedItem = itemNumber;
				
				switchItem->Pose.Orientation.y = oldYrot;
			}
			else
			{
				if (laraInfo->Control.IsMoving && laraInfo->Context.InteractedItem == itemNumber)
				{
					laraInfo->Control.IsMoving = false;
					laraInfo->Control.HandStatus = HandStatus::Free;
				}

				switchItem->Pose.Orientation.y = oldYrot;
			}
		}
	}

	void CrowDoveSwitchControl(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		if (item->MeshBits & 2) 
		{
			ExplodeItemNode(item, 1, 0, 256); 
			SoundEffect(SFX_TR5_RAVEN_SWITCH_EXPLODE, &item->Pose);
			item->MeshBits = 5;	
			RemoveActiveItem(itemNumber);

			// NOTE: In original TR5 the switch was used together with heavy switches.
			// This little fix make it usable normaly and less hardcoded.
			item->ItemFlags[0] = 1; 
		}
		else if (item->ItemFlags[0] == 0)
		{
			if (item->Animation.ActiveState == SWITCH_OFF)
				item->Animation.TargetState = SWITCH_ON;

			AnimateItem(*item);

			if (item->Animation.ActiveState == SWITCH_OFF)
				item->Pose.Orientation.y += ANGLE(90.0f);
		}
	}
}
