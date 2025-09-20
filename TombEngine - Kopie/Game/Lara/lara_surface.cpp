#include "framework.h"
#include "Game/Lara/lara_surface.h"

#include "Game/camera.h"
#include "Game/collision/collide_room.h"
#include "Game/control/control.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_collide.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Lara/lara_swim.h"
#include "Game/Lara/lara_tests.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Specific/level.h"
#include "Specific/Input/Input.h"

using namespace TEN::Input;

// -----------------------------
// WATER SURFACE TREAD
// Control & Collision Functions
// -----------------------------

// State:		LS_ONWATER_DIVE (35)
// Collision:	lara_col_surface_dive()
void lara_as_surface_dive(ItemInfo* item, CollisionInfo* coll)
{
	if (IsHeld(In::Forward))
		item->Pose.Orientation.x -= ANGLE(1.0f);
}

// State:		LS_ONWATER_DIVE (35)
// Control:		lara_as_surface_dive()
void lara_col_surface_dive(ItemInfo* item, CollisionInfo* coll)
{
	LaraSwimCollision(item, coll);
}

// State:		LS_ONWATER_IDLE (33)
// Collision:	lara_col_surface_idle()
void lara_as_surface_idle(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	lara->Control.Look.Mode = LookMode::Free;

	item->Animation.Velocity.y -= g_GameFlow->GetSettings()->Physics.SwimVelocity * LARA_SWIM_VELOCITY_DECEL_COEFF;
	if (item->Animation.Velocity.y < 0)
		item->Animation.Velocity.y = 0;

	if (item->HitPoints <= 0)
	{
		item->Animation.TargetState = LS_WATER_DEATH;
		return;
	}

	if (IsHeld(In::Left) || IsHeld(In::Right))
		ModulateLaraTurnRateY(item, LARA_TURN_RATE_ACCEL * 1.25f, 0, LARA_MED_TURN_RATE_MAX);

	if (IsClicked(In::Jump))
	{
		SetLaraSwimDiveAnimation(item);
		return;
	}
	
	if (IsHeld(In::Roll) || (IsHeld(In::Forward) && IsHeld(In::Back)))
	{
		item->Animation.TargetState = LS_ROLL_180_FORWARD;
		return;
	}

	if (IsHeld(In::Forward))
	{
		item->Animation.TargetState = LS_ONWATER_FORWARD;
		return;
	}
	else if (IsHeld(In::Back))
	{
		item->Animation.TargetState = LS_ONWATER_BACK;
		return;
	}
	
	if (IsHeld(In::StepLeft) || (IsHeld(In::Walk) && IsHeld(In::Left)))
	{
		item->Animation.TargetState = LS_ONWATER_LEFT;
		return;
	}
	else if (IsHeld(In::StepRight) || (IsHeld(In::Walk) && IsHeld(In::Right)))
	{
		item->Animation.TargetState = LS_ONWATER_RIGHT;
		return;
	}

	item->Animation.TargetState = LS_ONWATER_IDLE;
}

// State:		LS_ONWATER_IDLE (33)
// Control:		lara_as_surface_idle()
void lara_col_surface_idle(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	lara->Control.MoveAngle = item->Pose.Orientation.y;
	LaraSurfaceCollision(item, coll);
}

// State:		LS_ONWATER_FORWARD (34)
// Collision:	lara_col_surface_swim_forward()
void lara_as_surface_swim_forward(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	lara->Control.Look.Mode = LookMode::Horizontal;

	if (item->HitPoints <= 0)
	{
		item->Animation.TargetState = LS_WATER_DEATH;
		return;
	}

	if (IsHeld(In::Left) || IsHeld(In::Right))
		ModulateLaraTurnRateY(item, LARA_TURN_RATE_ACCEL * 1.25f, 0, LARA_MED_TURN_RATE_MAX);

	if (!IsHeld(In::Forward))
		item->Animation.TargetState = LS_ONWATER_IDLE;

	if (IsClicked(In::Jump))
		SetLaraSwimDiveAnimation(item);

	float baseVel = g_GameFlow->GetSettings()->Physics.SwimVelocity;

	item->Animation.Velocity.y += baseVel * LARA_SWIM_VELOCITY_ACCEL_COEFF;
	if (item->Animation.Velocity.y > baseVel * LARA_TREAD_VELOCITY_MAX_COEFF)
		item->Animation.Velocity.y = baseVel * LARA_TREAD_VELOCITY_MAX_COEFF;
}

// State:		LS_ONWATER_FORWARD (34)
// Control:		lara_as_surface_swim_forward()
void lara_col_surface_swim_forward(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	lara->Control.MoveAngle = item->Pose.Orientation.y;
	coll->Setup.UpperFloorBound = -STEPUP_HEIGHT;
	LaraSurfaceCollision(item, coll);
	TestLaraWaterClimbOut(item, coll);
	TestLaraLadderClimbOut(item, coll);
}

// State:		LS_ONWATER_LEFT (48)
// Collision:	lara_col_surface_swim_left()
void lara_as_surface_swim_left(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	lara->Control.Look.Mode = LookMode::Vertical;

	if (item->HitPoints <= 0)
	{
		item->Animation.TargetState = LS_WATER_DEATH;
		return;
	}

	if (!IsHeld(In::Walk))	// WALK locks orientation.
	{
		if (IsHeld(In::Left) || IsHeld(In::Right))
			ModulateLaraTurnRateY(item, LARA_TURN_RATE_ACCEL * 1.25f, 0, LARA_SLOW_MED_TURN_RATE_MAX);
	}

	if (!(IsHeld(In::StepLeft) || (IsHeld(In::Walk) && IsHeld(In::Left))))
		item->Animation.TargetState = LS_ONWATER_IDLE;

	if (IsClicked(In::Jump))
		SetLaraSwimDiveAnimation(item);

	float baseVel = g_GameFlow->GetSettings()->Physics.SwimVelocity;

	item->Animation.Velocity.y += baseVel * LARA_SWIM_VELOCITY_ACCEL_COEFF;
	if (item->Animation.Velocity.y > baseVel * LARA_TREAD_VELOCITY_MAX_COEFF)
		item->Animation.Velocity.y = baseVel * LARA_TREAD_VELOCITY_MAX_COEFF;
}

// State:		LS_ONWATER_LEFT (48)
// Control:		lara_as_surface_swim_left()
void lara_col_surface_swim_left(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	lara->Control.MoveAngle = item->Pose.Orientation.y - ANGLE(90.0f);
	LaraSurfaceCollision(item, coll);
}

// State:		LS_ONWATER_RIGHT (49)
// Collision:	lara_col_surface_swim_right()
void lara_as_surface_swim_right(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	lara->Control.Look.Mode = LookMode::Vertical;

	if (item->HitPoints <= 0)
	{
		item->Animation.TargetState = LS_WATER_DEATH;
		return;
	}

	if (!IsHeld(In::Walk))	// WALK locks orientation.
	{
		if (IsHeld(In::Left) || IsHeld(In::Right))
			ModulateLaraTurnRateY(item, LARA_TURN_RATE_ACCEL * 1.25f, 0, LARA_SLOW_MED_TURN_RATE_MAX);
	}

	if (!(IsHeld(In::StepRight) || (IsHeld(In::Walk) && IsHeld(In::Right))))
		item->Animation.TargetState = LS_ONWATER_IDLE;

	if (IsClicked(In::Jump))
		SetLaraSwimDiveAnimation(item);

	float baseVel = g_GameFlow->GetSettings()->Physics.SwimVelocity;

	item->Animation.Velocity.y += baseVel * LARA_SWIM_VELOCITY_ACCEL_COEFF;
	if (item->Animation.Velocity.y > baseVel * LARA_TREAD_VELOCITY_MAX_COEFF)
		item->Animation.Velocity.y = baseVel * LARA_TREAD_VELOCITY_MAX_COEFF;
}

// State:		LS_ONWATER_RIGHT (49)
// Conrol:		lara_as_surface_swim_right()
void lara_col_surface_swim_right(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	lara->Control.MoveAngle = item->Pose.Orientation.y + ANGLE(90.0f);
	LaraSurfaceCollision(item, coll);
}

// State:		LS_ONWATER_BACK (47)
// Collision:	lara_col_surface_swim_back()
void lara_as_surface_swim_back(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	lara->Control.Look.Mode = LookMode::Horizontal;

	if (item->HitPoints <= 0)
	{
		item->Animation.TargetState = LS_WATER_DEATH;
		return;
	}

	if (IsHeld(In::Left) || IsHeld(In::Right))
		ModulateLaraTurnRateY(item, LARA_TURN_RATE_ACCEL * 1.25f, 0, LARA_SLOW_MED_TURN_RATE_MAX);

	if (IsClicked(In::Jump))
		SetLaraSwimDiveAnimation(item);

	if (!IsHeld(In::Back))
		item->Animation.TargetState = LS_ONWATER_IDLE;

	float baseVel = g_GameFlow->GetSettings()->Physics.SwimVelocity;

	item->Animation.Velocity.y += baseVel * LARA_SWIM_VELOCITY_ACCEL_COEFF;
	if (item->Animation.Velocity.y > baseVel * LARA_TREAD_VELOCITY_MAX_COEFF)
		item->Animation.Velocity.y = baseVel * LARA_TREAD_VELOCITY_MAX_COEFF;
}

// State:		LS_ONWATER_BACK (47)
// Control:		lara_as_surface_swim_back()
void lara_col_surface_swim_back(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	lara->Control.MoveAngle = item->Pose.Orientation.y + ANGLE(180.0f);
	LaraSurfaceCollision(item, coll);
}

// State:		LS_ONWATER_EXIT (55)
// Collision:	lara_default_col()
void lara_as_surface_climb_out(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	player.Control.Look.Mode = LookMode::None;
	coll->Setup.EnableObjectPush = false;
	coll->Setup.EnableSpasm = false;
	Camera.flags = CF_FOLLOW_CENTER;	// Forces the camera to follow Lara instead of snapping.
}
