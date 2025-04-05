#include "framework.h"
#include "Objects/Generic/Doors/breakable_wall.h"

#include "Game/animation.h"
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

using namespace TEN::Input;

namespace TEN::Entities::Doors
{
	enum WallStates
	{
		BreakUnderwater = 1,
		BreakingDone = 2,
		BreakAboveGround = 3,
	};

	const auto UnderwaterWallPos = Vector3i(0, -512, 0);
	const ObjectCollisionBounds UnderwaterWallBounds =
	{
		GameBoundingBox(
			-BLOCK(3 / 4.0f), BLOCK(3 / 4.0f),
			-BLOCK(1), 0,
			-BLOCK(0.5f), BLOCK(0.25f)
		),
		std::pair(
			EulerAngles(ANGLE(-80.0f), ANGLE(-80.0f), ANGLE(-80.0f)),
			EulerAngles(ANGLE(80.0f), ANGLE(80.0f), ANGLE(80.0f))
		)
	};

	const auto WallPos = Vector3i(0, 0, -220);
	const ObjectCollisionBounds WallBounds =
	{
		GameBoundingBox(
			-BLOCK(0.5f), BLOCK(0.5f),
			0, 0,
			-BLOCK(0.5f), 0
		),
		std::pair(
			EulerAngles(ANGLE(-10.0f), ANGLE(-30.0f), ANGLE(-10.0f)),
			EulerAngles(ANGLE(10.0f), ANGLE(30.0f), ANGLE(10.0f))
		)
	};

	void BreakableWallCollision(short itemNumber, ItemInfo* laraItem, CollisionInfo* coll)
	{
		auto* player = GetLaraInfo(laraItem);
		auto* doorItem = &g_Level.Items[itemNumber];

		bool isUnderwater = (player->Control.WaterStatus == WaterStatus::Underwater);

		const auto& bounds = isUnderwater ? UnderwaterWallBounds : WallBounds;
		const auto& position = isUnderwater ? UnderwaterWallPos : WallPos;

		bool isActionActive = player->Control.IsMoving && player->Context.InteractedItem == itemNumber;
		bool isActionReady = IsHeld(In::Action);
		bool isPlayerAvailable = player->Control.HandStatus == HandStatus::Free && doorItem->Status != ITEM_ACTIVE && !doorItem->Animation.IsAirborne;

		bool isPlayerIdle = (!isUnderwater && laraItem->Animation.ActiveState == LS_IDLE && laraItem->Animation.AnimNumber == LA_STAND_IDLE) ||
			(isUnderwater && laraItem->Animation.ActiveState == LS_UNDERWATER_IDLE && laraItem->Animation.AnimNumber == LA_UNDERWATER_IDLE);

		if (isActionActive || (isActionReady && isPlayerAvailable && isPlayerIdle))
		{
			laraItem->Pose.Orientation.y ^= ANGLE(180.0f);

			if (TestLaraPosition(bounds, doorItem, laraItem))
			{
				if (MoveLaraPosition(position, doorItem, laraItem))
				{
					int animNumber = isUnderwater ? LA_UNDERWATER_WALL_KICK : LA_WALL_PUSH;
					SetAnimation(laraItem, animNumber);
					laraItem->Animation.Velocity.y = 0;
					doorItem->Status = ITEM_ACTIVE;

					AddActiveItem(itemNumber);
					WallStates state = isUnderwater ? BreakUnderwater : BreakAboveGround;
					doorItem->Animation.TargetState = state;

					AnimateItem(doorItem);

					player->Control.IsMoving = false;
					player->Control.HandStatus = HandStatus::Busy;
				}
				else
					player->Context.InteractedItem = itemNumber;

				laraItem->Pose.Orientation.y ^= ANGLE(180.0f);
			}
			else
			{
				if (player->Control.IsMoving && player->Context.InteractedItem == itemNumber)
				{
					player->Control.IsMoving = false;
					player->Control.HandStatus = HandStatus::Free;
				}

				laraItem->Pose.Orientation.y ^= ANGLE(180.0f);
			}
		}
		else if (doorItem->Status == ITEM_ACTIVE)
			ObjectCollision(itemNumber, laraItem, coll);
	}
}
