#include "framework.h"
#include "Objects/Generic/Doors/generic_doors.h"
#include "Specific/level.h"
#include "Game/control/control.h"
#include "Game/control/box.h"
#include "Game/items.h"
#include "Game/control/lot.h"
#include "Game/Gui.h"
#include "Specific/Input/Input.h"
#include "Game/pickup/pickup.h"
#include "Sound/sound.h"
#include "Game/Animation/Animation.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Lara/lara_struct.h"
#include "Game/Lara/lara.h"
#include "Math/Math.h"
#include "Game/misc.h"
#include "Objects/Generic/Doors/pushpull_kick_door.h"
#include "Game/collision/collide_item.h"
#include "Game/itemdata/door_data.h"

using namespace TEN::Animation;
using namespace TEN::Input;

namespace TEN::Entities::Doors
{
	enum STATES_PUSHPULL_KICK_DOOR
	{
		STATE_PUSHPULL_KICK_DOOR_OPEN = 0,
		STATE_PUSHPULL_KICK_DOOR_CLOSED = 1,
		STATE_PUSHPULL_KICK_DOOR_PUSH = 2,
		STATE_PUSHPULL_KICK_DOOR_PULL = 3
	};

	const Vector3i PullDoorPos(-201, 0, 322);
	const Vector3i PushDoorPos(201, 0, -702);
	const Vector3i KickDoorPos(0, 0, -917);

	const ObjectCollisionBounds PushPullKickDoorBounds =
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

	void PushPullKickDoorCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll)
	{
		auto* laraInfo = GetLaraInfo(laraItem);
		auto* doorItem = &g_Level.Items[itemNumber];

		if (IsHeld(In::Action) &&
			laraItem->Animation.ActiveState == LS_IDLE &&
			laraItem->Animation.AnimNumber == LA_STAND_IDLE &&
			!laraItem->HitStatus &&
			doorItem->Status != ITEM_ACTIVE &&
			laraInfo->Control.HandStatus == HandStatus::Free ||
			laraInfo->Control.IsMoving && laraInfo->Context.InteractedItem == itemNumber)
		{
			bool pull = false;

			if (laraItem->RoomNumber == doorItem->RoomNumber)
			{
				doorItem->Pose.Orientation.y ^= ANGLE(180.0f);
				pull = true;
			}

			if (TestLaraPosition(PushPullKickDoorBounds, doorItem, laraItem))
			{
				bool openTheDoor = false;

				if (pull)
				{
					if (MoveLaraPosition(PullDoorPos, doorItem, laraItem))
					{
						SetAnimation(*laraItem, LA_DOOR_OPEN_PULL);
						doorItem->Animation.TargetState = STATE_PUSHPULL_KICK_DOOR_PULL;
						openTheDoor = true;
					}
					else
						laraInfo->Context.InteractedItem = itemNumber;
				}
				else
				{
					if (doorItem->ObjectNumber >= ID_KICK_DOOR1)
					{
						if (MoveLaraPosition(KickDoorPos, doorItem, laraItem))
						{
							SetAnimation(*laraItem, LA_DOOR_OPEN_KICK);
							doorItem->Animation.TargetState = STATE_PUSHPULL_KICK_DOOR_PUSH;
							openTheDoor = true;
						}
						else
							laraInfo->Context.InteractedItem = itemNumber;
					}
					else
					{
						if (MoveLaraPosition(PushDoorPos, doorItem, laraItem))
						{
							SetAnimation(*laraItem, LA_DOOR_OPEN_PUSH);
							doorItem->Animation.TargetState = STATE_PUSHPULL_KICK_DOOR_PUSH;
							openTheDoor = true;
						}
						else
							laraInfo->Context.InteractedItem = itemNumber;
					}
				}

				if (openTheDoor)
				{
					AddActiveItem(itemNumber);

					laraItem->Animation.ActiveState = LS_MISC_CONTROL;
					laraItem->Animation.TargetState = LS_IDLE;
					laraInfo->Control.IsMoving = false;
					laraInfo->Control.HandStatus = HandStatus::Busy;
					doorItem->Status = ITEM_ACTIVE;
				}
			}
			else if (laraInfo->Control.IsMoving &&
				laraInfo->Context.InteractedItem == itemNumber)
			{
				laraInfo->Control.IsMoving = false;
				laraInfo->Control.HandStatus = HandStatus::Free;
			}

			if (pull)
				doorItem->Pose.Orientation.y ^= ANGLE(180.0f);
		}
		else if (doorItem->Animation.ActiveState <= STATE_PUSHPULL_KICK_DOOR_CLOSED)
			DoorCollision(itemNumber, laraItem, coll);
	}

	void PushPullKickDoorControl(short itemNumber)
	{
		auto* doorItem = &g_Level.Items[itemNumber];
		auto* doorData = (DOOR_DATA*)doorItem->Data;

		if (!doorData->opened)
		{
			OpenThatDoor(&doorData->d1, doorData);
			OpenThatDoor(&doorData->d2, doorData);
			OpenThatDoor(&doorData->d1flip, doorData);
			OpenThatDoor(&doorData->d2flip, doorData);
			doorData->opened = true;
		}

		AnimateItem(*doorItem);
	}
}