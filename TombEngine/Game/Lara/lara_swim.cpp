#include "framework.h"
#include "Game/Lara/lara_swim.h"

#include "Game/Animation/Animation.h"
#include "Game/camera.h"
#include "Game/collision/Point.h"
#include "Game/control/control.h"
#include "Game/items.h"
#include "Game/Lara/lara_collide.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Lara/lara_tests.h"
#include "Game/Lara/lara.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Scripting/Include/ScriptInterfaceLevel.h"
#include "Sound/sound.h"
#include "Specific/configuration.h"
#include "Specific/level.h"
#include "Specific/Input/Input.h"

using namespace TEN::Animation;
using namespace TEN::Collision::Point;
// using namespace TEN::Config; // sezz; develop uses g_Configuration without namespace.
using namespace TEN::Input;

// -----------------------------
// UNDERWATER SWIM
// Control & Collision Functions
// -----------------------------

// State:	  LS_UNDERWATER_IDLE (13)
// Collision: lara_col_underwater_idle()
void lara_as_underwater_idle(ItemInfo* item, CollisionInfo* coll)
{
	constexpr auto TURN_FLAGS = (int)PlayerTurnFlags::TurnX | (int)PlayerTurnFlags::TurnY | (int)PlayerTurnFlags::SwimFlex;

	auto& player = GetLaraInfo(*item);
	const auto& level = *g_GameFlow->GetLevel(CurrentLevel);

	bool hasDivesuit = (level.GetLaraType() == LaraType::Divesuit);

	player.Control.Look.Mode = LookMode::Free;

	if (item->HitPoints <= 0)
	{
		item->Animation.TargetState = LS_WATER_DEATH;
		return;
	}

	if ((IsHeld(In::Roll) || (HasOppositeAction(*item) && g_Configuration.EnableOppositeActionRoll)) &&
		!hasDivesuit)
	{
		SetAnimation(item, LA_UNDERWATER_ROLL_180_START);
		return;
	}

	if (g_Configuration.IsUsingModernControls())
	{
		if (g_Configuration.IsUsingOmnidirectionalSwimControls())
		{
			if (IsHeld(In::Forward) || IsHeld(In::Back) || IsHeld(In::Left) || IsHeld(In::Right))
			{
				// Turn.
				HandlePlayerTurn(*item, PLAYER_SWIM_TURN_ALPHA, LARA_LEAN_MAX, false, TURN_FLAGS);

				item->Animation.TargetState = LS_UNDERWATER_SWIM_FORWARD;
			}
		}
		else if (g_Configuration.IsUsingPlanarSwimControls())
		{
			if (IsHeld(In::Forward) || IsHeld(In::Back) || IsHeld(In::Left) || IsHeld(In::Right))
			{
				// Turn.
				HandlePlayerTurn(*item, PLAYER_SWIM_TURN_ALPHA, LARA_LEAN_MAX, false, TURN_FLAGS);

				item->Animation.TargetState = LS_UNDERWATER_SWIM_FORWARD;
			}
			else if (IsHeld(In::Jump) || IsHeld(In::Crouch))
			{
				item->Animation.TargetState = LS_UNDERWATER_SWIM_FORWARD;
			}
		}
	}
	else
	{
		hasDivesuit ? ModulateLaraSubsuitSwimTurnRates(item) : ModulateLaraSwimTurnRates(item, coll);

		if (IsHeld(In::Jump))
			item->Animation.TargetState = LS_UNDERWATER_SWIM_FORWARD;
	}

	item->Animation.Velocity.y -= g_GameFlow->GetSettings()->Physics.SwimVelocity * LARA_SWIM_VELOCITY_DECEL_COEFF;
	if (item->Animation.Velocity.y < 0.0f)
		item->Animation.Velocity.y = 0.0f;

	if (player.Control.HandStatus == HandStatus::Busy)
		player.Control.HandStatus = HandStatus::Free;

	// Reset.
	//item->Animation.TargetState = LS_UNDERWATER_IDLE;
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
	constexpr auto TURN_FLAGS = (int)PlayerTurnFlags::TurnX | (int)PlayerTurnFlags::TurnY | (int)PlayerTurnFlags::SwimFlex;

	auto& player = GetLaraInfo(*item);
	const auto& level = *g_GameFlow->GetLevel(CurrentLevel);

	bool hasDivesuit = (level.GetLaraType() == LaraType::Divesuit);

	player.Control.Look.Mode = LookMode::Horizontal;

	if (item->HitPoints <= 0)
	{
		item->Animation.TargetState = LS_WATER_DEATH;
		return;
	}

	if (IsHeld(In::Roll) && !hasDivesuit)
	{
		SetAnimation(item, LA_UNDERWATER_ROLL_180_START);
		return;
	}

	if (g_Configuration.IsUsingModernControls())
	{
		if (g_Configuration.IsUsingOmnidirectionalSwimControls())
		{
			if (IsHeld(In::Forward) || IsHeld(In::Back) || IsHeld(In::Left) || IsHeld(In::Right))
			{
				// Turn.
				HandlePlayerTurn(*item, PLAYER_SWIM_TURN_ALPHA, LARA_LEAN_MAX, false, TURN_FLAGS);

				item->Animation.TargetState = LS_UNDERWATER_SWIM_FORWARD;
			}
			else
			{
				item->Animation.TargetState = LS_UNDERWATER_INERTIA;
			}
		}
		else if (g_Configuration.IsUsingPlanarSwimControls())
		{
			if (IsHeld(In::Forward) || IsHeld(In::Back) || IsHeld(In::Left) || IsHeld(In::Right))
			{
				// Turn.
				item->Pose.Orientation.Lerp(EulerAngles(0, item->Pose.Orientation.y, item->Pose.Orientation.z), PLAYER_SWIM_TURN_ALPHA);
				HandlePlayerTurn(*item, PLAYER_SWIM_TURN_ALPHA, LARA_LEAN_MAX, false, TURN_FLAGS);

				item->Animation.TargetState = LS_UNDERWATER_SWIM_FORWARD;
			}
			else if (IsHeld(In::Jump))
			{
				item->Pose.Orientation.Lerp(EulerAngles(ANGLE(-90.0f), item->Pose.Orientation.y, item->Pose.Orientation.z), PLAYER_SWIM_TURN_ALPHA);
				HandlePlayerSwimTurnFlex(*item, PLAYER_SWIM_TURN_ALPHA * 2);

				item->Animation.TargetState = LS_UNDERWATER_SWIM_FORWARD;
			}
			else if (IsHeld(In::Crouch))
			{
				item->Pose.Orientation.Lerp(EulerAngles(ANGLE(90.0f), item->Pose.Orientation.y, item->Pose.Orientation.z), PLAYER_SWIM_TURN_ALPHA);
				HandlePlayerSwimTurnFlex(*item, PLAYER_SWIM_TURN_ALPHA * 2);

				item->Animation.TargetState = LS_UNDERWATER_SWIM_FORWARD;
			}
			else
			{
				item->Animation.TargetState = LS_UNDERWATER_INERTIA;
			}
		}

		/*HandlePlayerTurn(*item, PLAYER_SWIM_TURN_ALPHA, LARA_LEAN_MAX, false, TURN_FLAGS);

		if (!IsHeld(In::Forward) && !IsHeld(In::Back) && !IsHeld(In::Left) && !IsHeld(In::Right))
			item->Animation.TargetState = LS_UNDERWATER_INERTIA;*/
	}
	else
	{
		hasDivesuit ? ModulateLaraSubsuitSwimTurnRates(item) : ModulateLaraSwimTurnRates(item, coll);

		if (!IsHeld(In::Jump))
			item->Animation.TargetState = LS_UNDERWATER_INERTIA;
	}

	float baseVel = g_GameFlow->GetSettings()->Physics.SwimVelocity;

	item->Animation.Velocity.y += baseVel * LARA_SWIM_VELOCITY_ACCEL_COEFF;
	if (item->Animation.Velocity.y > baseVel * LARA_SWIM_VELOCITY_MAX_COEFF)
		item->Animation.Velocity.y = baseVel * LARA_SWIM_VELOCITY_MAX_COEFF;
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
	constexpr auto TURN_FLAGS = (int)PlayerTurnFlags::TurnX | (int)PlayerTurnFlags::TurnY | (int)PlayerTurnFlags::SwimFlex;

	auto& player = GetLaraInfo(*item);
	const auto& level = *g_GameFlow->GetLevel(CurrentLevel);

	bool hasDivesuit = (level.GetLaraType() == LaraType::Divesuit);

	player.Control.Look.Mode = LookMode::Horizontal;

	if (item->HitPoints <= 0)
	{
		item->Animation.TargetState = LS_WATER_DEATH;
		return;
	}

	if (IsHeld(In::Roll) && !hasDivesuit)
	{
		SetAnimation(item, LA_UNDERWATER_ROLL_180_START);
		return;
	}

	if (g_Configuration.IsUsingModernControls())
	{
		HandlePlayerTurn(*item, PLAYER_SWIM_TURN_ALPHA, LARA_LEAN_MAX, false, TURN_FLAGS);

		if (IsHeld(In::Forward) || IsHeld(In::Back) || IsHeld(In::Left) || IsHeld(In::Right))
			item->Animation.TargetState = LS_UNDERWATER_SWIM_FORWARD;
	}
	else
	{
		hasDivesuit ? ModulateLaraSubsuitSwimTurnRates(item) : ModulateLaraSwimTurnRates(item, coll);

		if (IsHeld(In::Jump))
			item->Animation.TargetState = LS_UNDERWATER_SWIM_FORWARD;
	}

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
	auto& player = GetLaraInfo(*item);
	const auto& level = *g_GameFlow->GetLevel(CurrentLevel);

	bool hasDivesuit = (level.GetLaraType() == LaraType::Divesuit);

	player.Control.Look.Mode = LookMode::None;

	item->Animation.Velocity.y -= g_GameFlow->GetSettings()->Physics.SwimVelocity * LARA_SWIM_VELOCITY_DECEL_COEFF;
	if (item->Animation.Velocity.y < 0.0f)
		item->Animation.Velocity.y = 0.0f;

	if (item->Pose.Orientation.x < -ANGLE(2.0f) ||
		item->Pose.Orientation.x > ANGLE(2.0f))
	{
		if (item->Pose.Orientation.x >= 0)
		{
			item->Pose.Orientation.x -= ANGLE(2.0f);
		}
		else
		{
			item->Pose.Orientation.x += ANGLE(2.0f);
		}
	}
	else
	{
		item->Pose.Orientation.x = 0;
	}
}

// State:		LS_WATER_DEATH (44)
// Control:	lara_as_underwater_death()
void lara_col_underwater_death(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	item->HitPoints = NO_VALUE;
	player.Control.HandStatus = HandStatus::Busy;

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
