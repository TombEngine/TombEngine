#include "framework.h"
#include "Game/Lara/lara_crawl.h"

#include "Game/Animation/Animation.h"
#include "Game/camera.h"
#include "Game/collision/collide_room.h"
#include "Game/control/control.h"
#include "Game/control/los.h"
#include "Game/items.h"
#include "Game/Lara/PlayerContext.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_tests.h"
#include "Game/Lara/lara_collide.h"
#include "Game/Lara/lara_flare.h"
#include "Game/Lara/lara_helpers.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Specific/Input/Input.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Entities::Player;
using namespace TEN::Input;

// State:	  LS_CRAWL_VAULT (194)
// Collision: lara_void_func()
void lara_as_crawl_vault(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	// Setup
	player.Control.CanLook = false;
	coll->Setup.EnableObjectPush = false;
	coll->Setup.EnableSpasm = false;
	Camera.targetDistance = BLOCK(1);
	Camera.flags = CF_FOLLOW_CENTER;

	item->Animation.TargetState = LS_CRAWL_IDLE;
}

// -------
// CROUCH:
// -------

// State:	  LS_CROUCH_IDLE (71)
// Collision: lara_col_crouch_idle()
void lara_as_crouch_idle(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	player.Control.Look.Mode = LookMode::Free;
	coll->Setup.EnableObjectPush = true;
	coll->Setup.EnableSpasm = false;
	Camera.targetDistance = BLOCK(1);

	AlignLaraToSurface(item);

	// HACK: Prevent pickup interference.
	if (item->Animation.TargetState == LS_PICKUP ||
		item->Animation.TargetState == LS_PICKUP_FLARE)
	{
		return;
	}

	if (item->HitPoints <= 0)
	{
		item->Animation.TargetState = LS_DEATH;
		return;
	}

	if (player.Control.Look.IsUsingBinoculars)
		return;

	if (IsHeld(In::Left) || IsHeld(In::Right))
		ModulateLaraTurnRateY(item, LARA_TURN_RATE_ACCEL, 0, LARA_CRAWL_TURN_RATE_MAX);

	if ((IsHeld(In::Crouch) || player.Control.KeepLow) && CanCrouch(*item, *coll))
	{
		if (IsHeld(In::Roll) || (IsHeld(In::Forward) && IsHeld(In::Back)))
		{
			item->Animation.TargetState = LS_CROUCH_TURN_180;
			return;
		}

		if (IsHeld(In::Sprint) && CanCrouchRoll(*item, *coll))
		{
			item->Animation.TargetState = LS_CROUCH_ROLL;
			return;
		}

		if (IsHeld(In::Look) && CanPlayerLookAround(*item))
		{
			item->Animation.TargetState = LS_CROUCH_IDLE;
			return;
		}

		if ((IsHeld(In::Forward) || IsHeld(In::Back)) && CanCrouchToCrawl(*item, *coll))
		{
			item->Animation.TargetState = LS_CRAWL_IDLE;
			return;
		}

		if (IsHeld(In::Left))
		{
			item->Animation.TargetState = LS_CROUCH_TURN_LEFT;
			return;
		}
		else if (IsHeld(In::Right))
		{
			item->Animation.TargetState = LS_CROUCH_TURN_RIGHT;
			return;
		}

		item->Animation.TargetState = LS_CROUCH_IDLE;
		return;
	}

	item->Animation.TargetState = LS_IDLE;
}

// State:	LS_CROUCH_IDLE (71)
// Control: lara_as_crouch_idle()
void lara_col_crouch_idle(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	item->Animation.Velocity.y = 0.0f;
	item->Animation.IsAirborne = false;
	player.Control.KeepLow = IsInLowSpace(*item, *coll);
	player.Control.IsLow = true;
	player.Control.MoveAngle = item->Pose.Orientation.y;
	player.ExtraTorsoRot = EulerAngles::Identity;
	coll->Setup.Height = LARA_HEIGHT_CRAWL;
	coll->Setup.ForwardAngle = item->Pose.Orientation.y;
	coll->Setup.LowerFloorBound = CRAWL_STEPUP_HEIGHT;
	coll->Setup.UpperFloorBound = -CRAWL_STEPUP_HEIGHT;
	coll->Setup.LowerCeilingBound = 0;
	coll->Setup.BlockFloorSlopeUp = true;
	coll->Setup.BlockFloorSlopeDown = true;
	GetCollisionInfo(coll, item);

	if (CanFall(*item, *coll))
	{
		SetLaraFallAnimation(item);
		player.Control.HandStatus = HandStatus::Free;
		return;
	}

	if (CanSlide(*item, *coll))
	{
		SetLaraSlideAnimation(item, coll);
		return;
	}

	ShiftItem(item, coll);

	if (CanChangeElevation(*item, *coll))
	{
		HandlePlayerElevationChange(item, coll);
		return;
	}
}

// State:	  LS_CROUCH_ROLL (72)
// Collision: lara_as_crouch_roll()
void lara_as_crouch_roll(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	player.Control.Look.Mode = LookMode::Horizontal;
	coll->Setup.EnableObjectPush = true;
	coll->Setup.EnableSpasm = false;
	Camera.targetDistance = BLOCK(1);

	AlignLaraToSurface(item);

	if (IsHeld(In::Left) || IsHeld(In::Right))
	{
		ModulateLaraTurnRateY(item, LARA_TURN_RATE_ACCEL, 0, LARA_CROUCH_ROLL_TURN_RATE_MAX);
		HandlePlayerLean(item, coll, LARA_LEAN_RATE, LARA_LEAN_MAX);
	}

	item->Animation.TargetState = LS_CROUCH_IDLE;
}

// State:	LS_CROUCH_ROLL (72)
// Control: lara_as_crouch_roll()
void lara_col_crouch_roll(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	item->Animation.Velocity.y = 0.0f;
	item->Animation.IsAirborne = false;
	player.Control.KeepLow = IsInLowSpace(*item, *coll);
	player.Control.IsLow = true;
	player.Control.MoveAngle = item->Pose.Orientation.y;
	coll->Setup.Height = LARA_HEIGHT_CRAWL;
	coll->Setup.LowerFloorBound = CRAWL_STEPUP_HEIGHT;
	coll->Setup.UpperFloorBound = -CRAWL_STEPUP_HEIGHT;
	coll->Setup.ForwardAngle = item->Pose.Orientation.y;
	coll->Setup.LowerCeilingBound = 0;
	coll->Setup.BlockFloorSlopeUp = true;
	GetCollisionInfo(coll, item);

	// TODO: With sufficient speed, Lara can still roll off ledges. This is particularly a problem in the uncommon scenario where
	// she becomes IsAirborne within a crawlspace; collision handling will push her back very rapidly and potentially cause a softlock. @Sezz 2021.11.02
	if (LaraDeflectEdgeCrawl(item, coll))
		item->Pose.Position = coll->Setup.PrevPosition;

	if (CanFall(*item, *coll))
	{
		SetLaraFallAnimation(item);
		player.Control.HandStatus = HandStatus::Free;
		item->Animation.Velocity.z /= 3; // Truncate speed to prevent flying off.
		return;
	}

	if (CanSlide(*item, *coll))
	{
		SetLaraSlideAnimation(item, coll);
		return;
	}

	ShiftItem(item, coll);

	if (TestLaraHitCeiling(coll))
	{
		SetLaraHitCeiling(item, coll);
		return;
	}

	if (CanChangeElevation(*item, *coll))
	{
		HandlePlayerElevationChange(item, coll);
		return;
	}
}

// State:	  LS_CROUCH_TURN_LEFT (105)
// Collision: lara_col_crouch_turn_left()
void lara_as_crouch_turn_left(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	player.Control.Look.Mode = LookMode::Vertical;
	coll->Setup.EnableSpasm = false;
	Camera.targetDistance = BLOCK(1);

	AlignLaraToSurface(item);

	if (item->HitPoints <= 0)
	{
		item->Animation.TargetState = LS_DEATH;
		return;
	}

	if ((IsHeld(In::Crouch) || player.Control.KeepLow) && CanCrouch(*item, *coll))
	{
		if (IsHeld(In::Sprint) && CanCrouchRoll(*item, *coll))
		{
			item->Animation.TargetState = LS_CROUCH_ROLL;
			return;
		}

		if ((IsHeld(In::Forward) || IsHeld(In::Back)) && CanCrouchToCrawl(*item, *coll))
		{
			item->Animation.TargetState = LS_CRAWL_IDLE;
			return;
		}

		if (IsHeld(In::Left))
		{
			item->Animation.TargetState = LS_CROUCH_TURN_LEFT;
			ModulateLaraTurnRateY(item, LARA_TURN_RATE_ACCEL, 0, LARA_CRAWL_TURN_RATE_MAX);
			return;
		}

		item->Animation.TargetState = LS_CROUCH_IDLE;
		return;
	}

	item->Animation.TargetState = LS_IDLE;
}

// State:	LS_CRAWL_TURN_LEFT (105)
// Control: lara_as_crouch_turn_left()
void lara_col_crouch_turn_left(ItemInfo* item, CollisionInfo* coll)
{
	lara_col_crouch_idle(item, coll);
}

// State:	  LS_CROUCH_TURN_RIGHT (106)
// Collision: lara_col_crouch_turn_right()
void lara_as_crouch_turn_right(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	player.Control.Look.Mode = LookMode::Vertical;
	coll->Setup.EnableSpasm = false;
	Camera.targetDistance = BLOCK(1);

	AlignLaraToSurface(item);

	if (item->HitPoints <= 0)
	{
		item->Animation.TargetState = LS_DEATH;
		return;
	}

	if ((IsHeld(In::Crouch) || player.Control.KeepLow) && CanCrouch(*item, *coll))
	{
		if (IsHeld(In::Sprint) && CanCrouchRoll(*item, *coll))
		{
			item->Animation.TargetState = LS_CROUCH_ROLL;
			return;
		}

		if ((IsHeld(In::Forward) || IsHeld(In::Back)) && CanCrouchToCrawl(*item, *coll))
		{
			item->Animation.TargetState = LS_CRAWL_IDLE;
			return;
		}

		if (IsHeld(In::Right))
		{
			item->Animation.TargetState = LS_CROUCH_TURN_RIGHT;
			ModulateLaraTurnRateY(item, LARA_TURN_RATE_ACCEL, 0, LARA_CRAWL_TURN_RATE_MAX);
			return;
		}

		item->Animation.TargetState = LS_CROUCH_IDLE;
		return;
	}

	item->Animation.TargetState = LS_IDLE;
}

// State:	LS_CRAWL_TURN_RIGHT (106)
// Control: lara_as_crouch_turn_right()
void lara_col_crouch_turn_right(ItemInfo* item, CollisionInfo* coll)
{
	lara_col_crouch_idle(item, coll);
}

// State:	  LS_CROUCH_TURN_180 (171)
// Collision: lara_col_crouch_turn_180()
void lara_as_crouch_turn_180(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	player.Control.Look.Mode = LookMode::None;
	coll->Setup.EnableSpasm = false;
	Camera.targetDistance = BLOCK(1);

	AlignLaraToSurface(item);

	if ((IsHeld(In::Crouch) || player.Control.KeepLow) && CanCrouch(*item, *coll))
	{
		if ((IsHeld(In::Forward) || IsHeld(In::Back)) && CanCrouchToCrawl(*item, *coll))
		{
			item->Animation.TargetState = LS_CRAWL_IDLE;
			return;
		}

		item->Animation.TargetState = LS_CROUCH_IDLE;
		return;
	}

	item->Animation.TargetState = LS_CROUCH_IDLE;
}

// State:	LS_CROUCH_TURN_180 (171)
// Control: lara_as_crouch_turn_180()
void lara_col_crouch_turn_180(ItemInfo* item, CollisionInfo* coll)
{
	lara_col_crouch_idle(item, coll);
}

// ------
// CRAWL:
// ------

// State:	  LS_CRAWL_IDLE (80)
// Collision: lara_col_crawl_idle()
void lara_as_crawl_idle(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	player.Control.Look.Mode = LookMode::Free;
	coll->Setup.EnableObjectPush = true;
	coll->Setup.EnableSpasm = false;
	Camera.targetDistance = BLOCK(1);

	AlignLaraToSurface(item);

	// TODO: Dispatch pickups from within states.
	if (item->Animation.TargetState == LS_PICKUP ||
		item->Animation.TargetState == LS_CROUCH_IDLE)
	{
		return;
	}

	player.Control.HandStatus = HandStatus::Busy;

	if (item->HitPoints <= 0)
	{
		item->Animation.TargetState = LS_DEATH;
		return;
	}

	if (player.Control.Look.IsUsingBinoculars)
		return;

	if (IsHeld(In::Left) || IsHeld(In::Right))
		ModulateLaraTurnRateY(item, LARA_TURN_RATE_ACCEL, 0, LARA_CRAWL_TURN_RATE_MAX);

	if ((IsHeld(In::Crouch) || player.Control.KeepLow) && CanCrouch(*item, *coll))
	{
		if (IsHeld(In::Roll) || (IsHeld(In::Forward) && IsHeld(In::Back)))
		{
			item->Animation.TargetState = LS_CRAWL_TURN_180;
			return;
		}

		if ((IsHeld(In::Sprint) && CanCrouchRoll(*item, *coll)) ||
			((IsHeld(In::Draw) || (IsHeld(In::Flare) && player.Inventory.TotalFlares != 0)) &&
				!IsStandingWeapon(item, player.Control.Weapon.GunType) && TestStateDispatch(*item, LS_CROUCH_IDLE)))
		{
			item->Animation.TargetState = LS_CROUCH_IDLE;
			player.Control.HandStatus = HandStatus::Free;
			return;
		}

		if (IsHeld(In::Look) && CanPlayerLookAround(*item))
		{
			item->Animation.TargetState = LS_CRAWL_IDLE;
			return;
		}

		if (IsHeld(In::Forward))
		{
			auto crawlVaultContext = TestLaraCrawlVault(item, coll);

			if ((IsHeld(In::Action) || IsHeld(In::Jump)) && crawlVaultContext.Success)
			{
				item->Animation.TargetState = crawlVaultContext.TargetState;
				ResetPlayerTurnRateY(*item);
				ResetPlayerFlex(item);
				return;
			}

			if (CanCrawlForward(*item, *coll))
			{
				item->Animation.TargetState = LS_CRAWL_FORWARD;
				return;
			}
		}
		else if (IsHeld(In::Back))
		{
			if (IsHeld(In::Action) && TestLaraCrawlToHang(item, coll))
			{
				item->Animation.TargetState = LS_CRAWL_TO_HANG;
				DoLaraCrawlToHangSnap(item, coll);
				return;
			}

			if (CanCrawlBackward(*item, *coll))
			{
				item->Animation.TargetState = LS_CRAWL_BACK;
				return;
			}
		}

		if (IsHeld(In::Left))
		{
			item->Animation.TargetState = LS_CRAWL_TURN_LEFT;
			return;
		}
		else if (IsHeld(In::Right))
		{
			item->Animation.TargetState = LS_CRAWL_TURN_RIGHT;
			return;
		}

		item->Animation.TargetState = LS_CRAWL_IDLE;
		return;
	}

	item->Animation.TargetState = LS_CROUCH_IDLE;
	player.Control.HandStatus = HandStatus::Free;
}

// State:	LS_CRAWL_IDLE (80)
// Control: lara_as_crawl_idle()
void lara_col_crawl_idle(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	item->Animation.Velocity.y = 0.0;
	item->Animation.IsAirborne = false;
	player.Control.KeepLow = IsInLowSpace(*item, *coll);
	player.Control.IsLow = true;
	player.Control.MoveAngle = item->Pose.Orientation.y;
	player.ExtraTorsoRot.x = 0;
	player.ExtraTorsoRot.y = 0;
	coll->Setup.ForwardAngle = player.Control.MoveAngle;
	coll->Setup.Radius = LARA_RADIUS_CRAWL;
	coll->Setup.Height = LARA_HEIGHT_CRAWL;
	coll->Setup.LowerFloorBound = CRAWL_STEPUP_HEIGHT;
	coll->Setup.UpperFloorBound = -CRAWL_STEPUP_HEIGHT;
	coll->Setup.LowerCeilingBound = LARA_HEIGHT_CRAWL;
	coll->Setup.BlockFloorSlopeUp = true;
	coll->Setup.BlockFloorSlopeDown = true;
	GetCollisionInfo(coll, item);

	if (CanFall(*item, *coll))
	{
		SetLaraFallAnimation(item);
		return;
	}

	if (CanSlide(*item, *coll))
	{
		SetLaraSlideAnimation(item, coll);
		return;
	}

	ShiftItem(item, coll);

	if (CanChangeElevation(*item, *coll))
	{
		HandlePlayerElevationChange(item, coll);
		return;
	}
}

// State:	  LS_CRAWL_FORWARD (81)
// Collision: lara_col_crawl_forward()
void lara_as_crawl_forward(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	player.Control.Look.Mode = LookMode::Horizontal;
	player.Control.HandStatus = HandStatus::Busy;
	coll->Setup.EnableObjectPush = true;
	coll->Setup.EnableSpasm = false;
	Camera.targetDistance = BLOCK(1);

	AlignLaraToSurface(item);

	if (item->HitPoints <= 0)
	{
		item->Animation.TargetState = LS_DEATH;
		return;
	}

	if (IsHeld(In::Left) || IsHeld(In::Right))
	{
		ModulateLaraTurnRateY(item, LARA_CRAWL_MOVE_TURN_RATE_ACCEL, 0, LARA_CRAWL_MOVE_TURN_RATE_MAX);
		HandlePlayerCrawlFlex(*item);
	}

	if ((IsHeld(In::Crouch) || player.Control.KeepLow) && CanCrouch(*item, *coll))
	{
		if (IsHeld(In::Sprint) && CanCrouchRoll(*item, *coll))
		{
			item->Animation.TargetState = LS_CRAWL_IDLE;
			return;
		}

		if (IsHeld(In::Forward))
		{
			item->Animation.TargetState = LS_CRAWL_FORWARD;
			return;
		}

		item->Animation.TargetState = LS_CRAWL_IDLE;
		return;
	}

	item->Animation.TargetState = LS_CRAWL_IDLE;
}

// State:	LS_CRAWL_FORWARD (81)
// Control: lara_as_crawl_forward()
void lara_col_crawl_forward(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	item->Animation.Velocity.y = 0.0f;
	item->Animation.IsAirborne = false;
	player.Control.KeepLow = IsInLowSpace(*item, *coll);
	player.Control.IsLow = true;
	player.Control.MoveAngle = item->Pose.Orientation.y;
	player.ExtraTorsoRot.x = 0;
	player.ExtraTorsoRot.y = 0;
	coll->Setup.Radius = LARA_RADIUS_CRAWL;
	coll->Setup.Height = LARA_HEIGHT_CRAWL;
	coll->Setup.LowerFloorBound = CRAWL_STEPUP_HEIGHT;
	coll->Setup.UpperFloorBound = -CRAWL_STEPUP_HEIGHT;
	coll->Setup.LowerCeilingBound = LARA_HEIGHT_CRAWL;
	coll->Setup.BlockFloorSlopeDown = true;
	coll->Setup.BlockFloorSlopeUp = true;
	coll->Setup.BlockDeathFloorDown = true;
	coll->Setup.ForwardAngle = player.Control.MoveAngle;
	GetCollisionInfo(coll, item, true);

	if (LaraDeflectEdgeCrawl(item, coll))
		LaraCollideStopCrawl(item, coll);

	if (CanFall(*item, *coll))
	{
		SetLaraFallAnimation(item);
		return;
	}

	if (CanSlide(*item, *coll))
	{
		SetLaraSlideAnimation(item, coll);
		return;
	}

	ShiftItem(item, coll);

	if (CanChangeElevation(*item, *coll))
	{
		HandlePlayerElevationChange(item, coll);
		return;
	}
}

// State:	  LS_CRAWL_BACK (86)
// Collision: lara_col_crawl_back()
void lara_as_crawl_back(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	player.Control.Look.Mode = LookMode::Horizontal;
	player.Control.HandStatus = HandStatus::Busy;
	coll->Setup.EnableObjectPush = true;
	coll->Setup.EnableSpasm = false;
	Camera.targetDistance = BLOCK(1);

	AlignLaraToSurface(item);

	if (item->HitPoints <= 0)
	{
		item->Animation.TargetState = LS_DEATH;
		return;
	}

	if (IsHeld(In::Left) || IsHeld(In::Right))
	{
		ModulateLaraTurnRateY(item, LARA_CRAWL_MOVE_TURN_RATE_ACCEL, 0, LARA_CRAWL_MOVE_TURN_RATE_MAX);
		HandlePlayerCrawlFlex(*item);
	}

	if ((IsHeld(In::Crouch) || player.Control.KeepLow) && CanCrouch(*item, *coll))
	{
		if (IsHeld(In::Back))
		{
			item->Animation.TargetState = LS_CRAWL_BACK;
			return;
		}

		item->Animation.TargetState = LS_CRAWL_IDLE;
		return;
	}

	item->Animation.TargetState = LS_CRAWL_IDLE;
}

// State:	LS_CRAWL_BACK (86)
// Control: lara_as_crawl_back()
void lara_col_crawl_back(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	item->Animation.Velocity.y = 0.0f;
	item->Animation.IsAirborne = false;
	player.Control.KeepLow = IsInLowSpace(*item, *coll);
	player.Control.IsLow = true;
	player.Control.MoveAngle = item->Pose.Orientation.y + ANGLE(180.0f);
	coll->Setup.Radius = LARA_RADIUS_CRAWL;
	coll->Setup.Height = LARA_HEIGHT_CRAWL;
	coll->Setup.LowerFloorBound = CRAWL_STEPUP_HEIGHT;
	coll->Setup.UpperFloorBound = -CRAWL_STEPUP_HEIGHT;
	coll->Setup.LowerCeilingBound = LARA_HEIGHT_CRAWL;
	coll->Setup.BlockFloorSlopeDown = true;
	coll->Setup.BlockFloorSlopeUp = true;
	coll->Setup.BlockDeathFloorDown = true;
	coll->Setup.ForwardAngle = player.Control.MoveAngle;
	GetCollisionInfo(coll, item, true);

	if (LaraDeflectEdgeCrawl(item, coll))
		LaraCollideStopCrawl(item, coll);

	if (CanFall(*item, *coll))
	{
		SetLaraFallAnimation(item);
		return;
	}

	if (CanSlide(*item, *coll))
	{
		SetLaraSlideAnimation(item, coll);
		return;
	}

	ShiftItem(item, coll);

	if (CanChangeElevation(*item, *coll))
	{
		HandlePlayerElevationChange(item, coll);
		return;
	}
}

// State:	  LS_CRAWL_TURN_LEFT (84)
// Collision: lara_col_crawl_turn_left()
void lara_as_crawl_turn_left(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	player.Control.Look.Mode = LookMode::Vertical;
	player.Control.HandStatus = HandStatus::Busy;
	coll->Setup.EnableObjectPush = true;
	coll->Setup.EnableSpasm = false;
	Camera.targetDistance = BLOCK(1);

	AlignLaraToSurface(item);

	if (item->HitPoints <= 0)
	{
		item->Animation.TargetState = LS_DEATH;
		return;
	}

	if ((IsHeld(In::Crouch) || player.Control.KeepLow) && CanCrouch(*item, *coll))
	{
		if (IsHeld(In::Sprint) && CanCrouchRoll(*item, *coll))
		{
			item->Animation.TargetState = LS_CRAWL_IDLE;
			return;
		}

		if (IsHeld(In::Forward) && CanCrawlForward(*item, *coll))
		{
			item->Animation.TargetState = LS_CRAWL_FORWARD;
			return;
		}

		if (IsHeld(In::Back) && CanCrawlBackward(*item, *coll))
		{
			item->Animation.TargetState = LS_CRAWL_BACK;
			return;
		}

		if (IsHeld(In::Left))
		{
			item->Animation.TargetState = LS_CRAWL_TURN_LEFT;
			ModulateLaraTurnRateY(item, LARA_TURN_RATE_ACCEL, 0, LARA_CRAWL_MOVE_TURN_RATE_MAX);
			return;
		}

		item->Animation.TargetState = LS_CRAWL_IDLE;
		return;
	}

	item->Animation.TargetState = LS_CRAWL_IDLE;
}

// State:	LS_CRAWL_TURN_LEFT (84)
// Control: lara_as_crawl_turn_left()
void lara_col_crawl_turn_left(ItemInfo* item, CollisionInfo* coll)
{
	lara_col_crawl_idle(item, coll);
}

// State:	  LS_CRAWL_TURN_RIGHT (85)
// Collision: lara_col_crawl_turn_right()
void lara_as_crawl_turn_right(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	player.Control.Look.Mode = LookMode::Vertical;
	player.Control.HandStatus = HandStatus::Busy;
	coll->Setup.EnableObjectPush = true;
	coll->Setup.EnableSpasm = false;
	Camera.targetDistance = BLOCK(1);

	AlignLaraToSurface(item);

	if (item->HitPoints <= 0)
	{
		item->Animation.TargetState = LS_DEATH;
		return;
	}

	if ((IsHeld(In::Crouch) || player.Control.KeepLow) && CanCrouch(*item, *coll))
	{
		if (IsHeld(In::Sprint) && CanCrouchRoll(*item, *coll))
		{
			item->Animation.TargetState = LS_CRAWL_IDLE;
			return;
		}

		if (IsHeld(In::Forward) && CanCrawlForward(*item, *coll))
		{
			item->Animation.TargetState = LS_CRAWL_FORWARD;
			return;
		}

		if (IsHeld(In::Back) && CanCrawlBackward(*item, *coll))
		{
			item->Animation.TargetState = LS_CRAWL_BACK;
			return;
		}

		if (IsHeld(In::Right))
		{
			item->Animation.TargetState = LS_CRAWL_TURN_RIGHT;
			ModulateLaraTurnRateY(item, LARA_TURN_RATE_ACCEL, 0, LARA_CRAWL_MOVE_TURN_RATE_MAX);
			return;
		}

		item->Animation.TargetState = LS_CRAWL_IDLE;
		return;
	}

	item->Animation.TargetState = LS_CRAWL_IDLE;
}

// State:	LS_CRAWL_TURN_RIGHT (85)
// Control: lara_as_crawl_turn_right()
void lara_col_crawl_turn_right(ItemInfo* item, CollisionInfo* coll)
{
	lara_col_crawl_idle(item, coll);
}

// State:	  LS_CRAWL_TURN_180 (172)
// Collision: lara_col_crawl_turn_180()
void lara_as_crawl_turn_180(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	player.Control.Look.Mode = LookMode::None;
	player.Control.HandStatus = HandStatus::Busy;
	coll->Setup.EnableSpasm = false;
	Camera.targetDistance = BLOCK(1);

	AlignLaraToSurface(item);

	if ((IsHeld(In::Crouch) || player.Control.KeepLow) && CanCrouch(*item, *coll))
	{
		item->Animation.TargetState = LS_CRAWL_IDLE;
		return;
	}

	item->Animation.TargetState = LS_CROUCH_IDLE;
	player.Control.HandStatus = HandStatus::Free;
}

// State:	LS_CRAWL_TURN_180 (172)
// Control: lara_as_crawl_turn_180()
void lara_col_crawl_turn_180(ItemInfo* item, CollisionInfo* coll)
{
	lara_col_crawl_idle(item, coll);
}

void lara_col_crawl_to_hang(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	coll->Setup.EnableObjectPush = true;
	coll->Setup.EnableSpasm = false;
	Camera.targetAngle = 0;
	Camera.targetDistance = BLOCK(1);
	Camera.flags = CF_FOLLOW_CENTER;

	ResetPlayerLean(item, 1 / 6.0f);

	if (item->Animation.AnimNumber == LA_CRAWL_TO_HANG_END)
	{
		player.Control.MoveAngle = item->Pose.Orientation.y;
		coll->Setup.Height = LARA_HEIGHT_STRETCH;
		coll->Setup.LowerFloorBound = NO_LOWER_BOUND;
		coll->Setup.UpperFloorBound = -STEPUP_HEIGHT;
		coll->Setup.LowerCeilingBound = BAD_JUMP_CEILING;
		coll->Setup.ForwardAngle = player.Control.MoveAngle;

		item->Pose.Translate(item->Pose.Orientation.y, -BLOCK(1.0f / 4));
		GetCollisionInfo(coll, item);
		SnapItemToLedge(item, coll);
		SetAnimation(*item, LA_REACH_TO_HANG, 12);

		GetCollisionInfo(coll, item);
		item->Animation.IsAirborne = true;
		item->Animation.Velocity.z = 2.0f;
		item->Animation.Velocity.y = 1.0f;
		item->Pose.Position.y += coll->Front.Floor - GameBoundingBox(item).Y1 - 20;
		player.Control.HandStatus = HandStatus::Busy;
	}
}
