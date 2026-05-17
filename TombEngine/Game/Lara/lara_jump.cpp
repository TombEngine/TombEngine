#include "framework.h"
#include "Game/Lara/lara_jump.h"

#include "Game/camera.h"
#include "Game/control/control.h"
#include "Game/Lara/PlayerContext.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_collide.h"
#include "Game/Lara/lara_tests.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Lara/lara_basic.h"
#include "Game/Lara/lara_overhang.h"
#include "Game/Lara/lara_slide.h"
#include "Game/Setup.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Sound/sound.h"
#include "Specific/Input/Input.h"
#include "Specific/level.h"

using namespace TEN::Entities::Player;
using namespace TEN::Input;

// -----------------------------
// JUMP
// Control & Collision Functions
// -----------------------------

// TODO: Unused? Naming is also completely mismatched.
// State:		LS_GRAB_TO_FALL
// Collision:	lara_void_func()
void lara_col_land(ItemInfo* item, CollisionInfo* coll)
{
	lara_col_idle(item, coll);
}

// State:		LS_JUMP_FORWARD (3)
// Collision:	lara_col_jump_forward()
void lara_as_jump_forward(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	player.Control.Look.Mode = LookMode::Horizontal;

	// Update running jump counter in preparation for possible jump action soon after landing.
	player.Control.Count.Run++;
	if (player.Control.Count.Run > PLAYER_RUN_JUMP_TIME / 2)
		player.Control.Count.Run = PLAYER_RUN_JUMP_TIME / 2;

	if (item->HitPoints <= 0)
	{
		if (CanLand(*item, *coll))
		{
			item->Animation.TargetState = LS_DEATH;
			SetLaraLand(item, coll);
		}

		return;
	}

	if (IsHeld(In::Left) || IsHeld(In::Right))
		ModulateLaraTurnRateY(item, LARA_TURN_RATE_ACCEL, 0, LARA_JUMP_TURN_RATE_MAX);

	if (CanLand(*item, *coll))
	{
		DoLaraFallDamage(item);

		if (item->HitPoints <= 0)
			item->Animation.TargetState = LS_DEATH;
		else if (IsHeld(In::Forward) && !IsHeld(In::Walk) &&
			player.Control.WaterStatus != WaterStatus::Wade)
		{
			item->Animation.TargetState = LS_RUN_FORWARD;
		}
		else
			item->Animation.TargetState = LS_IDLE;

		SetLaraLand(item, coll);
		return;
	}

	if (item->Animation.Velocity.y >= LARA_FREEFALL_VELOCITY)
	{
		item->Animation.TargetState = LS_FREEFALL;
		return;
	}

	if (IsHeld(In::Action) &&
		player.Control.HandStatus == HandStatus::Free)
	{
		item->Animation.TargetState = LS_REACH;
		return;
	}

	if (IsHeld(In::Roll) || IsHeld(In::Back))
	{
		item->Animation.TargetState = LS_JUMP_ROLL_180;
		return;
	}

	if (IsHeld(In::Walk) &&
		player.Control.HandStatus == HandStatus::Free)
	{
		item->Animation.TargetState = LS_SWAN_DIVE;
		return;
	}

	item->Animation.TargetState = LS_JUMP_FORWARD;
}

// State:		LS_JUMP_FORWARD (3)
// Control:		lara_as_jump_forward()
void lara_col_jump_forward(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	player.Control.MoveAngle = (item->Animation.Velocity.z > 0.0f) ? item->Pose.Orientation.y : item->Pose.Orientation.y + ANGLE(180.0f);
	coll->Setup.LowerFloorBound = NO_LOWER_BOUND;
	coll->Setup.UpperFloorBound = -STEPUP_HEIGHT;
	coll->Setup.LowerCeilingBound = BAD_JUMP_CEILING;
	coll->Setup.ForwardAngle = player.Control.MoveAngle;
	GetCollisionInfo(coll, item);

	LaraDeflectEdgeJump(item, coll);

	// TODO: Why??
	player.Control.MoveAngle = (item->Animation.Velocity.z < 0.0f) ? item->Pose.Orientation.y : player.Control.MoveAngle;
}

// State:		LS_FREEFALL (9)
// Collision:	lara_col_freefall()
void lara_as_freefall(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	item->Animation.Velocity.z *= 0.95f;
	player.Control.Look.Mode = LookMode::Free;

	ModulateLaraTurnRateY(item, 0, 0, 0);

	if (item->Animation.Velocity.y == LARA_DEATH_VELOCITY && item->HitPoints > 0)
		SoundEffect(SFX_TR4_LARA_FALL, &item->Pose);

	if (CanLand(*item, *coll))
	{
		DoLaraFallDamage(item);

		if (item->HitPoints <= 0)
			item->Animation.TargetState = LS_DEATH;
		else
			item->Animation.TargetState = LS_IDLE;

		SetLaraLand(item, coll);
		StopSoundEffect(SFX_TR4_LARA_FALL);
		return;
	}

	item->Animation.TargetState = LS_FREEFALL;
}

// State:		LS_FREEFALL (9)
// Control:		lara_as_freefall()
void lara_col_freefall(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	item->Animation.IsAirborne = true;
	coll->Setup.LowerFloorBound = NO_LOWER_BOUND;
	coll->Setup.UpperFloorBound = -STEPUP_HEIGHT;
	coll->Setup.LowerCeilingBound = BAD_JUMP_CEILING;
	coll->Setup.ForwardAngle = player.Control.MoveAngle;
	GetCollisionInfo(coll, item);

	LaraSlideEdgeJump(item, coll);
}

// State:		LS_REACH (11)
// Collision:	lara_col_reach()
void lara_as_reach(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	player.Control.Look.Mode = LookMode::Horizontal;
	Camera.targetAngle = ANGLE(85.0f);

	if (item->HitPoints <= 0)
	{
		if (CanLand(*item, *coll))
		{
			item->Animation.TargetState = LS_DEATH;
			SetLaraLand(item, coll);
		}

		return;
	}

	if (IsHeld(In::Left) || IsHeld(In::Right))
		ModulateLaraTurnRateY(item, LARA_TURN_RATE_ACCEL, 0, LARA_JUMP_TURN_RATE_MAX / 2);

	if (CanLand(*item, *coll))
	{
		DoLaraFallDamage(item);

		if (item->HitPoints <= 0)
			item->Animation.TargetState = LS_DEATH;
		else
			item->Animation.TargetState = LS_IDLE;

		SetLaraLand(item, coll);
		return;
	}

	if (item->Animation.Velocity.y >= LARA_FREEFALL_VELOCITY)
	{
		item->Animation.TargetState = LS_FREEFALL;
		return;
	}

	item->Animation.TargetState = LS_REACH;
}

// State:		LS_REACH (11)
// Control:		lara_as_reach()
void lara_col_reach(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	if (player.Control.Rope.Ptr == -1)
		item->Animation.IsAirborne = true;

	player.Control.MoveAngle = item->Pose.Orientation.y;

	// HACK: height is altered according to VerticalVelocity to fix "issues" with physically impossible
	// 6-click high ceiling running jumps. While TEN model is physically correct, original engines
	// allowed certain margin of deflection due to bug caused by hacky inclusion of headroom in coll checks.

	coll->Setup.Height = item->Animation.Velocity.y > 0 ? LARA_HEIGHT_REACH : LARA_HEIGHT;
	coll->Setup.LowerFloorBound = NO_LOWER_BOUND;
	coll->Setup.UpperFloorBound = 0;
	coll->Setup.LowerCeilingBound = BAD_JUMP_CEILING;
	coll->Setup.ForwardAngle = player.Control.MoveAngle;
	coll->Setup.Radius = coll->Setup.Radius * 1.2f;
	coll->Setup.Mode = CollisionProbeMode::FreeForward;
	GetCollisionInfo(coll, item);

	// Overhang hook.
	SlopeReachExtra(item, coll);

	if (TestLaraHangJump(item, coll))
		return;

	LaraSlideEdgeJump(item, coll);

	GetCollisionInfo(coll, item);
	ShiftItem(item, coll);
}

// State:		LS_JUMP_PREPARE (15)
// Collision:	lara_col_jump_prepare()
void lara_as_jump_prepare(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	player.Control.Look.Mode = LookMode::Free;

	// TODO: I need to revise the directional jump system to work with changes done for OIS. @Sezz 2022.07.05
	ModulateLaraTurnRateY(item, 0, 0, 0);

	if (item->HitPoints <= 0)
	{
		item->Animation.TargetState = LS_IDLE;
		return;
	}

	// JUMP key repressed without directional key; cancel directional jump lock.
	if (IsClicked(In::Jump) && !IsDirectionalActionHeld())
		player.Control.JumpDirection = JumpDirection::None;

	if (((IsHeld(In::Forward) &&
			!(IsHeld(In::Back) && player.Control.JumpDirection == JumpDirection::Back)) ||	// Back jump takes priority in this exception.
		!IsDirectionalActionHeld() && player.Control.JumpDirection == JumpDirection::Forward) &&
		CanJumpForward(*item, *coll))
	{
		item->Animation.TargetState = LS_JUMP_FORWARD;
		player.Control.JumpDirection = JumpDirection::Forward;
		return;
	}
	else if ((IsHeld(In::Back) ||
		!IsDirectionalActionHeld() && player.Control.JumpDirection == JumpDirection::Back) &&
		CanJumpBackward(*item, *coll))
	{
		item->Animation.TargetState = LS_JUMP_BACK;
		player.Control.JumpDirection = JumpDirection::Back;
		return;
	}

	if ((IsHeld(In::Left) ||
		!IsDirectionalActionHeld() && player.Control.JumpDirection == JumpDirection::Left) &&
		CanJumpLeft(*item, *coll))
	{
		item->Animation.TargetState = LS_JUMP_LEFT;
		player.Control.JumpDirection = JumpDirection::Left;
		return;
	}
	else if ((IsHeld(In::Right) ||
		!IsDirectionalActionHeld() && player.Control.JumpDirection == JumpDirection::Right) &&
		CanJumpRight(*item, *coll))
	{
		item->Animation.TargetState = LS_JUMP_RIGHT;
		player.Control.JumpDirection = JumpDirection::Right;
		return;
	}

	// No directional key pressed AND no directional lock; commit to jump up.
	if (CanJumpUp(*item, *coll))
	{
		item->Animation.TargetState = LS_JUMP_UP;
		player.Control.JumpDirection = JumpDirection::Up;
		return;
	}

	item->Animation.TargetState = LS_IDLE;
	player.Control.JumpDirection = JumpDirection::None;
}

// State:		LS_JUMP_PREPARE (15)
// Collision:	lara_as_jump_prepare()
void lara_col_jump_prepare(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	bool isSwamp = TestEnvironment(ENV_FLAG_SWAMP, item);

	player.Control.MoveAngle = item->Pose.Orientation.y;
	switch (player.Control.JumpDirection)
	{
	case JumpDirection::Back:
		player.Control.MoveAngle += ANGLE(180.0f);
		break;

	case JumpDirection::Left:
		player.Control.MoveAngle -= ANGLE(90.0f);
		break;

	case JumpDirection::Right:
		player.Control.MoveAngle += ANGLE(90.0f);
		break;

	default:
		break;
	}
	
	coll->Setup.LowerFloorBound = isSwamp ? NO_LOWER_BOUND : STEPUP_HEIGHT;	// Security.
	coll->Setup.UpperFloorBound = -STEPUP_HEIGHT;
	coll->Setup.LowerCeilingBound = 0;
	coll->Setup.BlockFloorSlopeDown = !isSwamp;	// Security.
	coll->Setup.BlockFloorSlopeUp = !isSwamp;	// Security.
	coll->Setup.ForwardAngle = player.Control.MoveAngle;
	GetCollisionInfo(coll, item);

	if (TestLaraHitCeiling(coll))
	{
		SetLaraHitCeiling(item, coll);
		return;
	}

	if (CanFall(*item, *coll))
	{
		SetLaraFallAnimation(item);
		return;
	}

	if (CanSlide(*item, *coll))
	{
		SetLaraSlideAnimation(item, coll);
		SetLaraLand(item, coll);
		return;
	}

	ShiftItem(item, coll);

	if (CanChangeElevation(*item, *coll))
	{
		HandlePlayerElevationChange(item, coll);
		return;
	}
}

// State:		LS_JUMP_BACK (25)
// Collision:	lara_col_jump_back()
void lara_as_jump_back(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	player.Control.Look.Mode = LookMode::Horizontal;
	Camera.targetAngle = ANGLE(135.0f);

	if (item->HitPoints <= 0)
	{
		if (CanLand(*item, *coll))
		{
			item->Animation.TargetState = LS_DEATH;
			SetLaraLand(item, coll);
		}

		return;
	}

	if (IsHeld(In::Left) || IsHeld(In::Right))
		ModulateLaraTurnRateY(item, LARA_TURN_RATE_ACCEL, 0, LARA_JUMP_TURN_RATE_MAX);

	if (CanLand(*item, *coll))
	{
		DoLaraFallDamage(item);

		if (item->HitPoints <= 0)
			item->Animation.TargetState = LS_DEATH;
		else
			item->Animation.TargetState = LS_IDLE;

		SetLaraLand(item, coll);
		return;
	}

	if (item->Animation.Velocity.y >= LARA_FREEFALL_VELOCITY)
	{
		item->Animation.TargetState = LS_FREEFALL;
		return;
	}

	if (IsHeld(In::Roll) || IsHeld(In::Forward))
	{
		item->Animation.TargetState = LS_JUMP_ROLL_180;
		return;
	}

	item->Animation.TargetState = LS_JUMP_BACK;
}

// State:		LS_JUMP_BACK (25)
// Control:		lara_as_jump_back()
void lara_col_jump_back(ItemInfo* item, CollisionInfo* coll)
{
	LaraJumpCollision(item, coll, item->Pose.Orientation.y + ANGLE(180.0f));
}

// State:		LS_JUMP_RIGHT (26)
// Collision:	lara_col_jump_right()
void lara_as_jump_right(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	player.Control.Look.Mode = LookMode::Vertical;

	if (item->HitPoints <= 0)
	{
		if (CanLand(*item, *coll))
		{
			item->Animation.TargetState = LS_DEATH;
			SetLaraLand(item, coll);
		}

		return;
	}

	if (CanLand(*item, *coll))
	{
		DoLaraFallDamage(item);

		if (item->HitPoints <= 0)
			item->Animation.TargetState = LS_DEATH;
		else
			item->Animation.TargetState = LS_IDLE;

		SetLaraLand(item, coll);
		return;
	}

	if (item->Animation.Velocity.y >= LARA_FREEFALL_VELOCITY)
	{
		item->Animation.TargetState = LS_FREEFALL;
		return;
	}

	// TODO: It appears Core planned this feature. Add animations to make it possible.
	/*if (TrInput & (IN_ROLL | IN_LEFT))
	{
		item->TargetState = LS_JUMP_ROLL_180;
		return;
	}*/

	item->Animation.TargetState = LS_JUMP_RIGHT;
}

// State:		LS_JUMP_RIGHT (26)
// Control:		lara_as_jump_right()
void lara_col_jump_right(ItemInfo* item, CollisionInfo* coll)
{
	LaraJumpCollision(item, coll, item->Pose.Orientation.y + ANGLE(90.0f));
}

// State:		LS_JUMP_LEFT (27)
// Collision:	lara_as_jump_left()
void lara_as_jump_left(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	player.Control.Look.Mode = LookMode::Vertical;

	if (item->HitPoints <= 0)
	{
		if (CanLand(*item, *coll))
		{
			item->Animation.TargetState = LS_DEATH;
			SetLaraLand(item, coll);
		}

		return;
	}

	if (CanLand(*item, *coll))
	{
		DoLaraFallDamage(item);

		if (item->HitPoints <= 0)
			item->Animation.TargetState = LS_DEATH;
		else
			item->Animation.TargetState = LS_IDLE;

		SetLaraLand(item, coll);
		return;
	}

	if (item->Animation.Velocity.y >= LARA_FREEFALL_VELOCITY)
	{
		item->Animation.TargetState = LS_FREEFALL;
		return;
	}

	// TODO: It appears Core planned this feature. Add animations to make it possible.
	/*if (TrInput & (IN_ROLL | IN_RIGHT))
	{
		item->TargetState = LS_JUMP_ROLL_180;
		return;
	}*/

	item->Animation.TargetState = LS_JUMP_LEFT;
}

// State:		LS_JUMP_LEFT (27)
// Control:		lara_as_jump_left()
void lara_col_jump_left(ItemInfo* item, CollisionInfo* coll)
{
	LaraJumpCollision(item, coll, item->Pose.Orientation.y - ANGLE(90.0f));
}

// State:		LS_JUMP_UP (28)
// Collision:	lara_col_jump_up()
void lara_as_jump_up(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	player.Control.Look.Mode = LookMode::Free;

	if (item->HitPoints <= 0)
	{
		if (CanLand(*item, *coll))
		{
			item->Animation.TargetState = LS_DEATH;
			SetLaraLand(item, coll);
		}

		return;
	}

	if (CanLand(*item, *coll))
	{
		if (item->HitPoints <= 0)
			item->Animation.TargetState = LS_DEATH;
		else
			item->Animation.TargetState = LS_IDLE;

		SetLaraLand(item, coll);
		return;
	}

	if (item->Animation.Velocity.y >= LARA_FREEFALL_VELOCITY)
	{
		item->Animation.TargetState = LS_FREEFALL;
		return;
	}

	if (IsHeld(In::Forward))
	{
		item->Animation.Velocity.z += 2.0f;
		if (item->Animation.Velocity.z > 5.0f)
			item->Animation.Velocity.z = 5.0f;
	}
	else if (IsHeld(In::Back))
	{
		item->Animation.Velocity.z -= 2.0f;
		if (item->Animation.Velocity.z < -5.0f)
			item->Animation.Velocity.z = -5.0f;
	}
	else
		item->Animation.Velocity.z = (item->Animation.Velocity.z < 0.0f) ? -2.0f : 2.0f;

	if (item->Animation.Velocity.z < 0.0f)
	{
		// TODO: Holding BACK + LEFT/RIGHT results in Lara flexing more.
		item->Pose.Orientation.x += std::min<short>(LARA_LEAN_RATE / 3, abs(ANGLE(item->Animation.Velocity.z) - item->Pose.Orientation.x) / 3);
		player.ExtraHeadRot.y += (ANGLE(10.0f) - item->Pose.Orientation.z) / 3;
	}

	item->Animation.TargetState = LS_JUMP_UP;
}

// State:		LS_JUMP_UP (28)
// Control:		lara_as_jump_up()
void lara_col_jump_up(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	player.Control.MoveAngle = item->Pose.Orientation.y;
	coll->Setup.Height = LARA_HEIGHT_STRETCH;
	coll->Setup.LowerFloorBound = NO_LOWER_BOUND;
	coll->Setup.UpperFloorBound = -STEPUP_HEIGHT;
	coll->Setup.LowerCeilingBound = BAD_JUMP_CEILING;
	coll->Setup.ForwardAngle = (item->Animation.Velocity.z >= 0) ? player.Control.MoveAngle : player.Control.MoveAngle + ANGLE(180.0f);
	coll->Setup.Mode = CollisionProbeMode::FreeForward;
	GetCollisionInfo(coll, item);

	if (TestLaraHangJumpUp(item, coll))
		return;

	LaraDeflectTopSide(item, coll);

	if (coll->Middle.Ceiling >= 0 ||
		coll->CollisionType == CollisionType::Top ||
		coll->CollisionType == CollisionType::TopFront ||
		coll->CollisionType == CollisionType::Clamp)
	{
		item->Animation.Velocity.y = 1;
	}

	ShiftItem(item, coll);
}

// State:		LS_FALL_BACK (29)
// Collision:	lara_col_fall_back()
void lara_as_fall_back(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	player.Control.Look.Mode = LookMode::Free;

	if (item->HitPoints <= 0)
	{
		if (CanLand(*item, *coll))
		{
			item->Animation.TargetState = LS_DEATH;
			SetLaraLand(item, coll);
		}

		return;
	}

	if (IsHeld(In::Left) || IsHeld(In::Right))
		ModulateLaraTurnRateY(item, LARA_TURN_RATE_ACCEL, 0, LARA_JUMP_TURN_RATE_MAX / 2);

	if (CanLand(*item, *coll))
	{
		DoLaraFallDamage(item);

		if (item->HitPoints <= 0)
			item->Animation.TargetState = LS_DEATH;
		else
			item->Animation.TargetState = LS_IDLE;

		SetLaraLand(item, coll);
		return;
	}

	if (item->Animation.Velocity.y >= LARA_FREEFALL_VELOCITY)
	{
		item->Animation.TargetState = LS_FREEFALL;
		return;
	}

	if (IsHeld(In::Action) &&
		player.Control.HandStatus == HandStatus::Free)
	{
		item->Animation.TargetState = LS_REACH;
		return;
	}

	item->Animation.TargetState = LS_FALL_BACK;
}

// State:		LS_FALL_BACK (29)
// Collision:	lara_col_fall_back()
void lara_col_fall_back(ItemInfo* item, CollisionInfo* coll)
{
	lara_col_jump_back(item, coll);
}

// State:		LS_SWAN_DIVE (52)
// Collision:	lara_col_swan_dive()
void lara_as_swan_dive(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	player.Control.HandStatus = HandStatus::Busy;
	player.Control.Look.Mode = LookMode::Horizontal;
	coll->Setup.EnableObjectPush = true;
	coll->Setup.EnableSpasm = false;

	if (item->HitPoints <= 0)
	{
		if (CanLand(*item, *coll))
		{
			item->Animation.TargetState = LS_DEATH;
			SetLaraLand(item, coll);
		}

		if (item->Animation.Velocity.y >= LARA_FREEFALL_VELOCITY)
		{
			item->Animation.TargetState = LS_FREEFALL_DIVE;
			return;
		}

		return;
	}

	if (IsHeld(In::Left) || IsHeld(In::Right))
	{
		ModulateLaraTurnRateY(item, LARA_TURN_RATE_ACCEL, 0, LARA_JUMP_TURN_RATE_MAX);
		HandlePlayerLean(item, coll, LARA_LEAN_RATE / 2, LARA_LEAN_MAX);
	}

	if (CanLand(*item, *coll))
	{
		DoLaraFallDamage(item);

		if (item->HitPoints <= 0)
			item->Animation.TargetState = LS_DEATH;
		else if ((IsHeld(In::Crouch) || CanCrawlspaceDive(*item, *coll)) &&
			g_GameFlow->GetSettings()->Animations.CrawlspaceDive)
		{
			item->Animation.TargetState = LS_CROUCH_IDLE;
			item->Pose.Translate(coll->Setup.ForwardAngle, CLICK(0.5f)); // HACK: Move forward to avoid standing up or falling out on an edge.
		}
		else
			item->Animation.TargetState = LS_IDLE;

		SetLaraLand(item, coll);
		player.Control.HandStatus = HandStatus::Free;
		return;
	}

	if (item->Animation.Velocity.y >= LARA_FREEFALL_VELOCITY)
	{
		item->Animation.TargetState = LS_FREEFALL_DIVE;
		return;
	}

	item->Animation.TargetState = LS_SWAN_DIVE;
}

// State:		LS_SWAN_DIVE (52)
// Control:		lara_as_swan_dive()
void lara_col_swan_dive(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	auto bounds = GameBoundingBox(item);
	int realHeight = g_GameFlow->GetSettings()->Animations.CrawlspaceDive ? (bounds.GetHeight() * 0.7f) : LARA_HEIGHT;

	player.Control.MoveAngle = item->Pose.Orientation.y;
	coll->Setup.Height = std::max(LARA_HEIGHT_CRAWL, realHeight);
	coll->Setup.LowerFloorBound = NO_LOWER_BOUND;
	coll->Setup.UpperFloorBound = -STEPUP_HEIGHT;
	coll->Setup.LowerCeilingBound = BAD_JUMP_CEILING;
	coll->Setup.ForwardAngle = player.Control.MoveAngle;
	GetCollisionInfo(coll, item);

	if (LaraDeflectEdgeJump(item, coll))
	{
		// Reset position to avoid embedding inside sloped ceilings meeting the floor.
		item->Pose.Position = coll->Setup.PrevPosition;
		player.Control.HandStatus = HandStatus::Free;
	}
}

// State:		LS_FREEFALL_DIVE (53)
// Collision:	lara_col_freefall_dive()
void lara_as_freefall_dive(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	item->Animation.Velocity.z *= 0.95f;
	player.Control.Look.Mode = LookMode::Free;
	coll->Setup.EnableObjectPush = true;
	coll->Setup.EnableSpasm = false;

	ModulateLaraTurnRateY(item, 0, 0, 0);

	if (item->HitPoints <= 0)
	{
		if (CanLand(*item, *coll))
		{
			item->Animation.TargetState = LS_DEATH;
			SetLaraLand(item, coll);
		}

		return;
	}

	if (CanLand(*item, *coll))
	{
		if (item->Animation.Velocity.y >= LARA_DIVE_DEATH_VELOCITY ||
			item->HitPoints <= 0)
		{
			item->Animation.TargetState = LS_DEATH;
			Rumble(0.5f, 0.2f);
		}
		else
			item->Animation.TargetState = LS_IDLE;

		SetLaraLand(item, coll);
		return;
	}

	if (IsHeld(In::Roll))
	{
		item->Animation.TargetState = LS_JUMP_ROLL_180;
		return;
	}

	item->Animation.TargetState = LS_FREEFALL_DIVE;
}

// State:		LS_FREEFALL_DIVE (53)
// Control:		lara_as_freefall_dive()
void lara_col_freefall_dive(ItemInfo* item, CollisionInfo* coll)
{
	lara_col_jump_forward(item, coll);
}
