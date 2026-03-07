#include "framework.h"
#include "Game/Lara/lara_tests.h"

#include "Game/Animation/Animation.h"
#include "Game/collision/collide_item.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/floordata.h"
#include "Game/collision/Point.h"
#include "Game/control/control.h"
#include "Game/control/los.h"
#include "Game/items.h"
#include "Game/Lara/Context/Context.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_collide.h"
#include "Game/Lara/lara_flare.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Lara/lara_monkey.h"
#include "Math/Math.h"
#include "Renderer/Renderer.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Specific/configuration.h"
#include "Specific/Input/Input.h"
#include "Specific/level.h"
#include "Specific/trutils.h"

using namespace TEN::Animation;
using namespace TEN::Collision::Floordata;
using namespace TEN::Collision::Point;
using namespace TEN::Player;
using namespace TEN::Input;
using namespace TEN::Math;
using namespace TEN::Utils;

// -----------------------------
// TEST FUNCTIONS
// For State Control & Collision
// -----------------------------

bool TestPlayerInteractAngle(const ItemInfo& item, short testAngle)
{
	return (abs(Geometry::GetShortestAngle(item.Pose.Orientation.y, testAngle)) <= PLAYER_INTERACT_ANGLE_CONSTRAINT);
}

bool TestPlayerInteractAngle(short playerHeadingAngle, short testAngle)
{
	return (abs(Geometry::GetShortestAngle(playerHeadingAngle, testAngle)) <= PLAYER_INTERACT_ANGLE_CONSTRAINT);
}

bool TestLaraWall(const ItemInfo* item, float dist, float height)
{
	auto origin = GameVector(
		Geometry::TranslatePoint(item->Pose.Position, item->Pose.Orientation.y, 0.0f, height),
		item->RoomNumber);
	auto target = GameVector(
		Geometry::TranslatePoint(item->Pose.Position, item->Pose.Orientation.y, dist, height),
		item->RoomNumber);

	return !LOS(&origin, &target);
}

bool TestLaraFacingCorner(const ItemInfo* item, short headingAngle, float dist)
{
	short angleLeft = headingAngle - ANGLE(15.0f);
	short angleRight = headingAngle + ANGLE(15.0f);

	auto origin = GameVector(
		item->Pose.Position.x,
		item->Pose.Position.y - STEPUP_HEIGHT,
		item->Pose.Position.z,
		item->RoomNumber);

	auto target0 = GameVector(
		item->Pose.Position.x + dist * phd_sin(angleLeft),
		item->Pose.Position.y - STEPUP_HEIGHT,
		item->Pose.Position.z + dist * phd_cos(angleLeft),
		item->RoomNumber);

	auto target1 = GameVector(
		item->Pose.Position.x + dist * phd_sin(angleRight),
		item->Pose.Position.y - STEPUP_HEIGHT,
		item->Pose.Position.z + dist * phd_cos(angleRight),
		item->RoomNumber);

	bool result0 = LOS(&origin, &target0);
	bool result1 = LOS(&origin, &target1);
	return (!result0 && !result1);
}

void TestLaraWaterDepth(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	auto pointColl = GetPointCollision(*item);

	if (pointColl.GetWaterBottomHeight() == NO_HEIGHT)
	{
		item->Animation.Velocity.y = 0.0f;
		item->Pose.Position = coll->Setup.PrevPosition;
	}
	else if (pointColl.GetWaterBottomHeight() <= (LARA_HEIGHT - (LARA_HEADROOM / 2)))
	{
		SetAnimation(item, LA_UNDERWATER_TO_STAND);
		ResetPlayerLean(item);
		item->Animation.TargetState = LS_IDLE;
		item->Pose.Position.y = pointColl.GetFloorHeight();
		item->Animation.IsAirborne = false;
		item->Animation.Velocity = Vector3::Zero;
		player.Control.WaterStatus = WaterStatus::Wade;
	}
}

bool TestLaraWeaponType(LaraWeaponType refWeaponType, const std::vector<LaraWeaponType>& weaponTypeList)
{
	return Contains(weaponTypeList, refWeaponType);
}

bool TestLaraTorchFlame(ItemInfo* item, ItemInfo* flameItem)
{
	auto* lara = GetLaraInfo(item);

	bool bothIgnited = (lara->Torch.IsLit == (flameItem->Status == ITEM_ACTIVE));
	bool colorsEqual = (lara->Torch.CurrentColor == (Vector3)flameItem->Model.Color);

	return !bothIgnited || (lara->Torch.IsLit && !colorsEqual);
}

static std::vector<LaraWeaponType> StandingWeaponTypes
{
	LaraWeaponType::Shotgun,
	LaraWeaponType::HK,
	LaraWeaponType::Crossbow,
	LaraWeaponType::GrenadeLauncher,
	LaraWeaponType::HarpoonGun,
	LaraWeaponType::RocketLauncher,
	LaraWeaponType::Snowmobile
};

bool IsStandingWeapon(const ItemInfo* item, LaraWeaponType weaponType)
{
	return (TestLaraWeaponType(weaponType, StandingWeaponTypes));
}

bool IsSideJumpState(int state)
{
	static const std::vector<int> jumpStates
	{
		LS_JUMP_LEFT,
		LS_JUMP_RIGHT,
	};
	return TestState(state, jumpStates);
}

bool IsCrouching(const ItemInfo* item)
{
	bool crouching =
		item->Animation.ActiveState == LS_CROUCH_IDLE ||
		item->Animation.ActiveState == LS_CROUCH_ROLL ||
		item->Animation.ActiveState == LS_CROUCH_TURN_LEFT ||
		item->Animation.ActiveState == LS_CROUCH_TURN_RIGHT ||
		item->Animation.ActiveState == LS_CROUCH_TURN_180 ||
		item->Animation.AnimNumber == LA_STAND_TO_CROUCH_ABORT ||
		item->Animation.AnimNumber == LA_STAND_TO_CROUCH_START;

	// HACK: Unless there's better way to detect animation phase,
	// assume player is crouching if animation is in first 75% of crouch-to-stand animation.
	if (item->Animation.AnimNumber == LA_CROUCH_TO_STAND)
	{
		const auto& anim = GetAnimData(*item);

		int midpoint = anim.EndFrameNumber * 0.75f;
		if (item->Animation.FrameNumber <= midpoint)
			crouching = true;
	}

	return crouching;
}

bool IsVaultState(int state)
{
	static const std::vector<int> vaultStates
	{
		LS_STAND_VAULT,
		LS_STAND_VAULT_2_STEPS_UP,
		LS_STAND_VAULT_3_STEPS_UP,
		LS_STAND_VAULT_1_STEP_UP_TO_CROUCH,
		LS_STAND_VAULT_2_STEPS_UP_TO_CROUCH,
		LS_STAND_VAULT_3_STEPS_UP_TO_CROUCH,
		LS_AUTO_JUMP
	};
	return TestState(state, vaultStates);
}

bool IsJumpState(int state)
{
	static const std::vector<int> jumpStates
	{
		LS_JUMP_FORWARD,
		LS_JUMP_BACK,
		LS_JUMP_LEFT,
		LS_JUMP_RIGHT,
		LS_JUMP_UP,
		LS_FALL_BACK,
		LS_REACH,
		LS_SWAN_DIVE,
		LS_FREEFALL_DIVE,
		LS_FREEFALL
	};
	return TestState(state, jumpStates);
}

bool IsRunJumpQueueableState(int state)
{
	static const auto RUN_JUMP_QUEUABLE_STATES = std::vector<int>
	{
		LS_RUN_FORWARD,
		LS_SPRINT,
		LS_STEP_UP,
		LS_STEP_DOWN
	};

	return TestState(state, RUN_JUMP_QUEUABLE_STATES);
}

bool IsRunJumpCountableState(int state)
{
	static const std::vector<int> runningJumpTimerStates
	{
		LS_WALK_FORWARD,
		LS_RUN_FORWARD,
		LS_SPRINT,
		LS_SPRINT_DIVE,
		LS_JUMP_FORWARD
	};
	return TestState(state, runningJumpTimerStates);
}

bool TestLaraPoleCollision(ItemInfo* item, CollisionInfo* coll, bool goingUp, float offset)
{
	static constexpr auto poleProbeCollRadius = 16.0f;

	bool atLeastOnePoleCollided = false;

	auto collObjects = GetCollidedObjects(*item, true, false, BLOCK(2), ObjectCollectionMode::Items);
	if (!collObjects.IsEmpty())
	{
		auto laraBox = GameBoundingBox(item).ToBoundingOrientedBox(item->Pose);

		// HACK: Because Core implemented upward pole movement as a SetPosition command, we can't precisely
		// check her position. So we add a fixed height offset.

		// Offset a sphere when jumping toward pole.
		auto sphereOffset2D = Vector3::Zero;
		sphereOffset2D = Geometry::TranslatePoint(sphereOffset2D, item->Pose.Orientation.y, coll->Setup.Radius + item->Animation.Velocity.z);

		auto spherePos = laraBox.Center + Vector3(0.0f, (laraBox.Extents.y + poleProbeCollRadius + offset) * (goingUp ? -1 : 1), 0.0f);

		auto sphere = BoundingSphere(spherePos, poleProbeCollRadius);
		auto offsetSphere = BoundingSphere(spherePos + sphereOffset2D, poleProbeCollRadius);

		//DrawDebugSphere(sphere.Center, 16.0f, Vector4(1, 0, 0, 1), RendererDebugPage::CollisionStats);

		for (const auto* itemPtr : collObjects.Items)
		{
			if (itemPtr->ObjectNumber != ID_POLEROPE)
				continue;

			auto poleBox = GameBoundingBox(itemPtr).ToBoundingOrientedBox(itemPtr->Pose);
			poleBox.Extents = poleBox.Extents + Vector3(coll->Setup.Radius, 0.0f, coll->Setup.Radius);

			//DrawDebugBox(poleBox, Vector4(0, 0, 1, 1), RendererDebugPage::CollisionStats);

			if (poleBox.Intersects(sphere) || poleBox.Intersects(offsetSphere))
			{
				atLeastOnePoleCollided = true;
				break;
			}
		}
	}

	return atLeastOnePoleCollided;
}

bool TestLaraPoleUp(ItemInfo* item, CollisionInfo* coll)
{
	if (!TestLaraPoleCollision(item, coll, true, CLICK(1)))
		return false;

	return (coll->Middle.Ceiling < -CLICK(1));
}

bool TestLaraPoleDown(ItemInfo* item, CollisionInfo* coll)
{
	if (!TestLaraPoleCollision(item, coll, false))
		return false;

	return (coll->Middle.Floor > 0);
}
