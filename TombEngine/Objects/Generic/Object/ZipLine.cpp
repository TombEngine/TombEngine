#include "framework.h"
#include "Objects/Generic/Object/ZipLine.h"

#include "Game/Animation/Animation.h"
#include "Game/collision/collide_item.h"
#include "Game/collision/Interaction.h"
#include "Game/collision/Point.h"
#include "Game/control/box.h"
#include "Game/Hud/Hud.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Sound/sound.h"
#include "Specific/Input/Input.h"

using namespace TEN::Animation;
using namespace TEN::Collision::Interaction;
using namespace TEN::Collision::Point;
using namespace TEN::Hud;
using namespace TEN::Input;
using namespace TEN::Math;

namespace TEN::Entities::Traps
{
	const auto ZIP_LINE_INTERACT_BASIS = InteractionBasis(
		Vector3i(0, 0, BLOCK(3 / 8.0f)),
		EulerAngles::Identity,
		GameBoundingBox(
			-BLOCK(0.25f), BLOCK(0.25f),
			-CLICK(1), CLICK(1),
			BLOCK(0.25f), BLOCK(0.5f)).ToBoundingOrientedBox(Pose::Zero),
		std::pair(
			EulerAngles(ANGLE(-10.0f), -LARA_GRAB_THRESHOLD, ANGLE(-10.0f)),
			EulerAngles(ANGLE(10.0f), LARA_GRAB_THRESHOLD, ANGLE(10.0f))));

	void InitializeZipLine(short itemNumber)
	{
		auto& zipLineItem = g_Level.Items[itemNumber];
		zipLineItem.Data = GameVector();
		auto& pos = *(GameVector*)zipLineItem.Data;

		pos = GameVector(zipLineItem.Pose.Position, zipLineItem.RoomNumber);
	}

	void ControlZipLine(short itemNumber)
	{
		constexpr auto VEL_ACCEL   = 5.0f;
		constexpr auto VEL_MAX	   = 100.0f;

		auto& zipLineItem = g_Level.Items[itemNumber];
		auto& laraItem = *LaraItem;

		if (zipLineItem.Status != ITEM_ACTIVE)
			return;

		if (!(zipLineItem.Flags & IFLAG_INVISIBLE))
		{
			auto& prevPos = *(GameVector*)zipLineItem.Data;

			zipLineItem.Pose.Position = prevPos.ToVector3i();

			if (prevPos.RoomNumber != zipLineItem.RoomNumber)
				ItemNewRoom(itemNumber, prevPos.RoomNumber);

			zipLineItem.Status = ITEM_NOT_ACTIVE;
			SetAnimation(zipLineItem, 0);

			RemoveActiveItem(itemNumber);
			return;
		}

		if (zipLineItem.Animation.ActiveState == 1)
		{
			AnimateItem(zipLineItem);
			return;
		}

		AnimateItem(zipLineItem);

		// Accelerate.
		if (zipLineItem.Animation.Velocity.y < VEL_MAX)
			zipLineItem.Animation.Velocity.y += VEL_ACCEL;

		// Translate.
		// TODO: Use proper calculation of the trajectory instead of bitwise operation.
		auto headingOrient = EulerAngles(0, zipLineItem.Pose.Orientation.y, 0);
		zipLineItem.Pose.Translate(headingOrient, zipLineItem.Animation.Velocity.y);
		zipLineItem.Pose.Position.y += ((int)zipLineItem.Animation.Velocity.y >> 2);

		int vPos = zipLineItem.Pose.Position.y + CLICK(0.25f);
		auto pointColl = GetPointCollision(zipLineItem, zipLineItem.Pose.Orientation.y, zipLineItem.Animation.Velocity.y);

		// Update zip line room number.
		if (pointColl.GetRoomNumber() != zipLineItem.RoomNumber)
			ItemNewRoom(itemNumber, pointColl.GetRoomNumber());

		if (pointColl.GetFloorHeight() <= (vPos + CLICK(1)) || pointColl.GetCeilingHeight() >= (vPos - CLICK(1)))
		{
			// Dismount.
			if (laraItem.Animation.ActiveState == LS_ZIP_LINE)
			{
				laraItem.Animation.TargetState = LS_JUMP_FORWARD;
				laraItem.Animation.IsAirborne = true;
				laraItem.Animation.Velocity.y = zipLineItem.Animation.Velocity.y / 4;
				laraItem.Animation.Velocity.z = zipLineItem.Animation.Velocity.y;
			}

			SoundEffect(SFX_TR2_ZIPLINE_STOP, &zipLineItem.Pose);
			RemoveActiveItem(itemNumber);
			zipLineItem.Status = ITEM_NOT_ACTIVE;
			zipLineItem.Flags -= IFLAG_INVISIBLE;
		}
		else
		{
			// "Parent" player to zip line.
			if (laraItem.Animation.ActiveState == LS_ZIP_LINE)
				laraItem.Pose.Position = zipLineItem.Pose.Position;

			// Whizz sound.
			SoundEffect(SFX_TR2_ZIPLINE_GO, &zipLineItem.Pose);
		}
	}

	static bool TestPlayerZipLineInteraction(const ItemInfo& playerItem, const ItemInfo& zipLineItem)
	{
		auto& player = GetLaraInfo(playerItem);

		// Check zip line status.
		if (zipLineItem.Status != ITEM_NOT_ACTIVE)
			return false;

		// Test for ongoing interaction.
		if (player.Control.IsMoving && player.Context.InteractedItem == zipLineItem.Index)
			return true;

		// Test for new interaction.
		return (IsHeld(In::Action) &&
			playerItem.Animation.ActiveState == LS_IDLE &&
			!playerItem.Animation.IsAirborne &&
			player.Control.HandStatus == HandStatus::Free);
	}

	static void SetPlayerZipLineInteraction(ItemInfo& playerItem, ItemInfo& zipLineItem)
	{
		auto& player = GetLaraInfo(playerItem);

		SetAnimation(playerItem, LA_ZIPLINE_MOUNT);
		ResetPlayerFlex(&playerItem);

		player.Control.IsMoving = false;
		player.Control.HandStatus = HandStatus::Busy;

		// Activate zip line if mount state was reached.
		if (playerItem.Animation.ActiveState == LS_GRABBING)
		{
			if (!zipLineItem.Active)
				AddActiveItem(zipLineItem.Index);

			zipLineItem.Status = ITEM_ACTIVE;
			zipLineItem.Flags |= IFLAG_INVISIBLE;
		}
	}

	void CollideZipLine(short itemNumber, ItemInfo* collided, CollisionInfo* coll)
	{
		auto& zipLineItem = g_Level.Items[itemNumber];

		if (zipLineItem.Status != ITEM_NOT_ACTIVE)
			return;

		// Spawn interaction highlight.
		g_Hud.InteractionHighlighter.Test(*collided, zipLineItem, InteractionMode::Custom);

		// Interact.
		if (collided->IsLara())
		{
			if (TestInteraction(*collided, zipLineItem, ZIP_LINE_INTERACT_BASIS))
			{
				if (TestPlayerZipLineInteraction(*collided, zipLineItem))
					SetInteraction(*collided, zipLineItem, ZIP_LINE_INTERACT_BASIS, SetPlayerZipLineInteraction, InteractionBasisType::Walk);
			}
		}
	}
}
