#include "framework.h"
#include "Game/Lara/lara_swim.h"

#include "Game/Animation/Animation.h"
#include "Game/camera.h"
#include "Game/collision/Point.h"
#include "Game/control/control.h"
#include "Game/items.h"
#include "Game/Lara/lara_collide.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Lara/lara.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Scripting/Include/ScriptInterfaceLevel.h"
#include "Sound/sound.h"
#include "Specific/level.h"
#include "Specific/Input/Input.h"

using namespace TEN::Animation;
using namespace TEN::Collision::Point;
using namespace TEN::Input;

// -----------------------------
// UNDERWATER SWIM
// Control & Collision Functions
// -----------------------------

// State:		LS_UNDERWATER_IDLE (13)
// Collision:	lara_col_underwater_idle()
void lara_as_underwater_idle(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	lara->Control.Look.Mode = LookMode::Free;

	auto* level = g_GameFlow->GetLevel(CurrentLevel);
	auto laraType = g_GameFlow->GetLevel(CurrentLevel)->GetLaraType();

	if (item->HitPoints <= 0)
	{
		item->Animation.TargetState = LS_WATER_DEATH;
		return;
	}

	if ((IsHeld(In::Roll) || (IsHeld(In::Forward) && IsHeld(In::Back))) && laraType != LaraType::Divesuit)
	{
		SetAnimation(*item, LA_UNDERWATER_ROLL_180_START);
		return;
	}

	if (laraType == LaraType::Divesuit)
		ModulateLaraSubsuitSwimTurnRates(item);
	else
		ModulateLaraSwimTurnRates(item, coll);

	if (IsHeld(In::Jump))
		item->Animation.TargetState = LS_UNDERWATER_SWIM_FORWARD;

	item->Animation.Velocity.y -= g_GameFlow->GetSettings()->Physics.SwimVelocity * LARA_SWIM_VELOCITY_DECEL_COEFF;
	if (item->Animation.Velocity.y < 0.0f)
		item->Animation.Velocity.y = 0.0f;

	if (lara->Control.HandStatus == HandStatus::Busy)
		lara->Control.HandStatus = HandStatus::Free;
}

// State:		LS_UNDERWATER_IDLE (13)
// Control:		lara_as_underwater_idle()
void lara_col_underwater_idle(ItemInfo* item, CollisionInfo* coll)
{
	LaraSwimCollision(item, coll);
}

// State:		LS_UNDERWATER_SWIM_FORWARD (17)
// Collision:	lara_col_underwater_swim_forward()
void lara_as_underwater_swim_forward(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	player.Control.Look.Mode = LookMode::Horizontal;

	auto laraType = g_GameFlow->GetLevel(CurrentLevel)->GetLaraType();

	if (item->HitPoints <= 0)
	{
		item->Animation.TargetState = LS_WATER_DEATH;
		return;
	}

	if (IsHeld(In::Roll) && laraType != LaraType::Divesuit)
	{
		SetAnimation(*item, LA_UNDERWATER_ROLL_180_START);
		return;
	}

	if (laraType != LaraType::Divesuit)
		ModulateLaraSwimTurnRates(item, coll);
	else
		ModulateLaraSubsuitSwimTurnRates(item);

	float baseVel = g_GameFlow->GetSettings()->Physics.SwimVelocity;

	item->Animation.Velocity.y += baseVel * LARA_SWIM_VELOCITY_ACCEL_COEFF;
	if (item->Animation.Velocity.y > baseVel)
		item->Animation.Velocity.y = baseVel;

	if (!IsHeld(In::Jump))
		item->Animation.TargetState = LS_UNDERWATER_INERTIA;
}

// State:		LS_UNDERWATER_SWIM_FORWARD (17)
// Control:		lara_as_underwater_swim_forward()
void lara_col_underwater_swim_forward(ItemInfo* item, CollisionInfo* coll)
{
	LaraSwimCollision(item, coll);
}

// State:		LS_UNDERWATER_INERTIA (18)
// Collision:	lara_col_underwater_inertia()
void lara_as_underwater_inertia(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	player.Control.Look.Mode = LookMode::Horizontal;

	auto laraType = g_GameFlow->GetLevel(CurrentLevel)->GetLaraType();

	if (item->HitPoints <= 0)
	{
		item->Animation.TargetState = LS_WATER_DEATH;
		return;
	}

	if (IsHeld(In::Roll) && laraType != LaraType::Divesuit)
	{
		SetAnimation(*item, LA_UNDERWATER_ROLL_180_START);
		return;
	}

	if (laraType != LaraType::Divesuit)
		ModulateLaraSwimTurnRates(item, coll);
	else
		ModulateLaraSubsuitSwimTurnRates(item);

	if (IsHeld(In::Jump))
		item->Animation.TargetState = LS_UNDERWATER_SWIM_FORWARD;

	const auto& settings = g_GameFlow->GetSettings()->Physics;

	item->Animation.Velocity.y -= settings.SwimVelocity * LARA_SWIM_VELOCITY_DECEL_COEFF;
	if (item->Animation.Velocity.y < 0.0f)
		item->Animation.Velocity.y = 0.0f;

	if (item->Animation.Velocity.y < (settings.SwimVelocity * LARA_SWIM_INTERTIA_VELOCITY_MIN_COEFF))
		item->Animation.TargetState = LS_UNDERWATER_IDLE;
}

// State:		LS_UNDERWATER_INERTIA (18)
// Collision:	lara_as_underwater_inertia()
void lara_col_underwater_inertia(ItemInfo* item, CollisionInfo* coll)
{
	LaraSwimCollision(item, coll);
}

// State:		LS_WATER_DEATH (44)
// Collision:	lara_col_underwater_death()
void lara_as_underwater_death(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	lara->Control.Look.Mode = LookMode::None;

	item->Animation.Velocity.y -= g_GameFlow->GetSettings()->Physics.SwimVelocity * LARA_SWIM_VELOCITY_DECEL_COEFF;
	if (item->Animation.Velocity.y < 0.0f)
		item->Animation.Velocity.y = 0.0f;

	if (item->Pose.Orientation.x < -ANGLE(2.0f) ||
		item->Pose.Orientation.x > ANGLE(2.0f))
	{
		if (item->Pose.Orientation.x >= 0)
			item->Pose.Orientation.x -= ANGLE(2.0f);
		else
			item->Pose.Orientation.x += ANGLE(2.0f);
	}
	else
		item->Pose.Orientation.x = 0;
}

// State:		LS_WATER_DEATH (44)
// Control:	lara_as_underwater_death()
void lara_col_underwater_death(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	item->HitPoints = -1;
	lara->Control.HandStatus = HandStatus::Busy;

	int waterHeight = GetPointCollision(*item).GetWaterTopHeight();
	if (waterHeight < (item->Pose.Position.y - (CLICK(0.4f) - 2)) &&
		waterHeight != NO_HEIGHT)
	{
		item->Pose.Position.y -= 5;
	}

	LaraSwimCollision(item, coll);
}

// State:		LS_UNDERWATER_ROLL (66)
// Collision:	lara_col_underwater_roll_180()
void lara_as_underwater_roll_180(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	item->Animation.Velocity.y = 0.0f;
	player.Control.Look.Mode = LookMode::None;
}

// State:		LS_UNDERWATER_ROLL (66)
// Control:		lara_as_underwater_roll_180()
void lara_col_underwater_roll_180(ItemInfo* item, CollisionInfo* coll)
{
	LaraSwimCollision(item, coll);
}
