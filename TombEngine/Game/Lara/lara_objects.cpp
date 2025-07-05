#include "framework.h"
#include "Game/Lara/lara_objects.h"

#include "Game/animation.h"
#include "Game/camera.h"
#include "Game/collision/collide_room.h"
#include "Game/control/control.h"
#include "Game/items.h"
#include "Game/Lara/PlayerContext.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_collide.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Lara/lara_tests.h"
#include "Objects/Generic/Object/rope.h"
#include "Sound/sound.h"
#include "Specific/Input/Input.h"
#include "Specific/level.h"

using namespace TEN::Entities::Generic;
using namespace TEN::Entities::Player;
using namespace TEN::Input;

// -----------------------------------
// MISCELLANEOUS INTERACTABLE OBJECT
// Control & Collision Functions
// -----------------------------------

// ------
// PICKUP
// ------

// State:		LS_PICKUP (39), LS_MISC_CONTROL (89)
// Collision:	lara_default_col()
void lara_as_pickup(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	lara->Control.Look.Mode = LookMode::None;
	coll->Setup.EnableObjectPush = false;
	coll->Setup.EnableSpasm = false;
	Camera.targetAngle = -ANGLE(130.0f);
	Camera.targetElevation = -ANGLE(15.0f);
	Camera.targetDistance = BLOCK(1);

	if (TestLastFrame(item))
		item->Animation.TargetState = GetNextAnimState(item);
}

void lara_col_pickup(ItemInfo* item, CollisionInfo* coll)
{
	LaraDefaultCollision(item, coll);
	ShiftItem(item, coll);
}

// State:		LS_PICKUP_FLARE (67)
// Collision:	lara_default_col()
void lara_as_pickup_flare(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	lara->Control.Look.Mode = LookMode::None;
	coll->Setup.EnableObjectPush = false;
	coll->Setup.EnableSpasm = false;
	Camera.targetAngle = ANGLE(130.0f);
	Camera.targetElevation = -ANGLE(15.0f);
	Camera.targetDistance = BLOCK(1);

	if (item->Animation.FrameNumber == (GetAnimData(*item).frameEnd - 1))
		lara->Control.HandStatus = HandStatus::Free;
}

// ------
// SWITCH
// ------

// State:		LS_SWITCH_DOWN (40), LS_DOVE_SWITCH (126)
// Collision:	lara_default_col()
void lara_as_switch_on(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	lara->Control.Look.Mode = LookMode::None;
	coll->Setup.EnableObjectPush = false;
	coll->Setup.EnableSpasm = false;
	Camera.targetAngle = ANGLE(80.0f);
	Camera.targetElevation = -ANGLE(25.0f);
	Camera.targetDistance = BLOCK(1);
	Camera.speed = 6;
}

// State:		LS_SWITCH_DOWN (40), LS_DOVE_SWITCH (126)
// Collision:	lara_default_col()
void lara_as_switch_off(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	lara->Control.Look.Mode = LookMode::None;
	coll->Setup.EnableObjectPush = false;
	coll->Setup.EnableSpasm = false;
	Camera.targetAngle = ANGLE(80.0f);
	Camera.targetElevation = -ANGLE(25.0f);
	Camera.targetDistance = BLOCK(1);
	Camera.speed = 6;
}

// State:	LS_ROUND_HANDLE (95)
// Control:	lara_as_controlled_no_look()
void lara_col_turn_switch(ItemInfo* item, CollisionInfo* coll)
{
	if (coll->Setup.PrevPosition.x != item->Pose.Position.x || coll->Setup.PrevPosition.z != item->Pose.Position.z)
	{
		if (item->Animation.AnimNumber == LA_TURNSWITCH_PUSH_COUNTER_CLOCKWISE_CONTINUE)
		{
			SetAnimation(item, LA_TURNSWITCH_PUSH_COUNTER_CLOCKWISE_END);
			item->Pose.Orientation.y -= ANGLE(90.0f);
		}

		if (item->Animation.AnimNumber == LA_TURNSWITCH_PUSH_CLOCKWISE_CONTINUE)
		{
			SetAnimation(item, LA_TURNSWITCH_PUSH_CLOCKWISE_END);
			item->Pose.Orientation.y += ANGLE(90.0f);
		}
	}
}

// ----------
// RECEPTACLE
// ----------

// State:		LS_USE_KEY (42)
// Collision:	lara_default_col()
void lara_as_use_key(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	lara->Control.Look.Mode = LookMode::None;
	coll->Setup.EnableObjectPush = false;
	coll->Setup.EnableSpasm = false;
	Camera.targetAngle = -ANGLE(80.0f);
	Camera.targetElevation = -ANGLE(25.0f);
	Camera.targetDistance = BLOCK(1);
}

// State:		LS_USE_PUZZLE (43)
// Collision:	lara_default_col()
void lara_as_use_puzzle(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	lara->Control.Look.Mode = LookMode::None;
	coll->Setup.EnableObjectPush = false;
	coll->Setup.EnableSpasm = false;
	Camera.targetAngle = -ANGLE(80.0f);
	Camera.targetElevation = -ANGLE(25.0f);
	Camera.targetDistance = BLOCK(1);

	if (TestLastFrame(item) && item->ItemFlags[0])
	{
		item->Animation.ActiveState = LS_MISC_CONTROL;
		item->Animation.AnimNumber = item->ItemFlags[0];
		item->Animation.FrameNumber = GetAnimData(*item).frameBase;
	}
}

// --------
// PUSHABLE
// --------

// State:	  LS_PUSHABLE_PUSH (36)
// Collision: lara_void_func()
void lara_as_pushable_push(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	player.Control.Look.Mode = LookMode::None;
	coll->Setup.EnableObjectPush = false;
	coll->Setup.EnableSpasm = false;
	Camera.targetAngle = ANGLE(35.0f);
	Camera.targetElevation = ANGLE(-25.0f);
	Camera.flags = CF_FOLLOW_CENTER;
}

// State:	  LS_PUSHABLE_PULL (37)
// Collision: lara_void_func()
void lara_as_pushable_pull(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	player.Control.Look.Mode = LookMode::None;
	coll->Setup.EnableObjectPush = false;
	coll->Setup.EnableSpasm = false;
	Camera.targetAngle = ANGLE(35.0f);
	Camera.targetElevation = ANGLE(-25.0f);
	Camera.flags = CF_FOLLOW_CENTER;
}

// State:	  LS_PUSHABLE_GRAB (38)
// Collision: lara_default_col()
void lara_as_pushable_grab(ItemInfo* item, CollisionInfo* coll)
{
	coll->Setup.EnableObjectPush = false;
	coll->Setup.EnableSpasm = false;
	Camera.targetAngle = ANGLE(75.0f);

	if (!IsHeld(In::Action))
		item->Animation.TargetState = LS_IDLE;
}

// State:	  LS_PUSHABLE_PUSH_EDGE_SLIP (190)
// Collision: lara_void_func()
void lara_as_pushable_edge_slip(ItemInfo* item, CollisionInfo* coll)
{
	auto& player = GetLaraInfo(*item);

	player.Control.Look.Mode = LookMode::None;
	coll->Setup.EnableObjectPush = false;
	coll->Setup.EnableSpasm = false;
	Camera.targetAngle = ANGLE(35.0f);
	Camera.targetElevation = ANGLE(-25.0f);
	Camera.flags = CF_FOLLOW_CENTER;
}

// --------------
// HORIZONTAL BAR
// --------------

// State:		LS_HORIZONTAL_BAR_SWING (128)
// Collision:	lara_default_col()
void lara_as_horizontal_bar_swing(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	if (IsHeld(In::Action))
	{
		if (IsHeld(In::Jump))
			item->Animation.TargetState = LS_HORIZONTAL_BAR_LEAP;

		item->Animation.TargetState = LS_HORIZONTAL_BAR_SWING;
		lara->Control.HandStatus = HandStatus::Busy;
		return;
	}

	item->Animation.TargetState = LS_HORIZONTAL_BAR_LEAP;
}

// State:		LS_HORIZONTAL_BAR_LEAP (129)
// Collision:	lara_default_col()
void lara_as_horizontal_bar_leap(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);
	const auto& barItem = g_Level.Items[lara->Context.InteractedItem];

	item->Animation.IsAirborne = true;

	if (item->Animation.FrameNumber == GetAnimData(*item).frameBase)
	{
		int distance = 0;
		if (item->Pose.Orientation.y == barItem.Pose.Orientation.y)
			distance = (barItem.TriggerFlags / 100) - 2;
		else
			distance = (barItem.TriggerFlags % 100) - 2;

		lara->Control.HandStatus = HandStatus::Free;

		item->Animation.Velocity.z = (20 * distance) + 58;
		item->Animation.Velocity.y = -(20 * distance + 64);
	}

	if (TestLastFrame(item))
	{
		SetAnimation(item, LA_REACH);
		TranslateItem(item, item->Pose.Orientation, 700);
		item->Pose.Position.y -= 361;
	}
}

// ---------
// TIGHTROPE
// ---------

// State:		LS_TIGHTROPE_IDLE (119)
// Collision:	lara_default_col()
void lara_as_tightrope_idle(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	lara->Control.Look.Mode = LookMode::Free;

	DoLaraTightropeBalanceRegen(item);
	DoLaraTightropeLean(item);

	if (IsHeld(In::Forward))
	{
		item->Animation.TargetState = LS_TIGHTROPE_WALK;
		return;
	}

	if (IsHeld(In::Roll) || IsHeld(In::Back))
	{
		item->Animation.TargetState = LS_TIGHTROPE_TURN_180;
		return;
	}

	item->Animation.TargetState = LS_TIGHTROPE_IDLE;
}

// State:		LS_TIGHTROPE_DISMOUNT (125)
// Collision:	lara_default_col()
void lara_as_tightrope_dismount(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	coll->Setup.EnableObjectPush = false;
	coll->Setup.EnableSpasm = false;

	DoLaraTightropeBalanceRegen(item);
	DoLaraTightropeLean(item);

	if (item->Animation.AnimNumber == LA_TIGHTROPE_END &&
		TestLastFrame(item))
	{
		item->Pose.Orientation.z = 0;
		lara->ExtraTorsoRot.z = 0;
	}
}

// State:		LS_TIGHTROPE_WALK (121)
// Collision:	lara_default_col()
void lara_as_tightrope_walk(ItemInfo* item, CollisionInfo* coll) 
{
	auto* lara = GetLaraInfo(item);

	if (CanDismountTightrope(*item, *coll))
	{
		item->Animation.TargetState = LS_TIGHTROPE_DISMOUNT;
		DoLaraTightropeBalanceRegen(item);
		return;
	}

	lara->Control.Tightrope.TimeOnTightrope++;
	DoLaraTightropeBalance(item);
	DoLaraTightropeLean(item);

	if (lara->Control.Tightrope.Balance >= 8000)
	{
		item->Animation.TargetState = LS_TIGHTROPE_UNBALANCE_RIGHT;
		return;
	}
	else if (lara->Control.Tightrope.Balance <= -8000)
	{
		item->Animation.TargetState = LS_TIGHTROPE_UNBALANCE_LEFT;
		return;
	}

	if (IsHeld(In::Forward))
	{
		item->Animation.TargetState = LS_TIGHTROPE_WALK;
		return;
	}

	item->Animation.TargetState = LS_TIGHTROPE_IDLE;
}

// State:		TIGHTROPE_UNBALANCE_LEFT (122), TIGHTROPE_UNBALANCE_RIGHT (123)
// Collision:	lara_default_col()
void lara_as_tightrope_fall(ItemInfo* item, CollisionInfo* coll)
{
	DoLaraTightropeBalanceRegen(item);
	DoLaraTightropeLean(item);

	if (TestLastFrame(item))
	{
		// HACK: Set position command can't move Lara laterally?
		if (item->Animation.AnimNumber == LA_TIGHTROPE_FALL_LEFT)
			TranslateItem(item, coll->Setup.ForwardAngle - ANGLE(90.0f), CLICK(1));
		else if (item->Animation.AnimNumber == LA_TIGHTROPE_FALL_RIGHT)
			TranslateItem(item, coll->Setup.ForwardAngle + ANGLE(90.0f), CLICK(1));

		item->Animation.Velocity.y = 10;
	}
}

// ----
// ROPE
// ----

// State:		LS_ROPE_TURN_CLOCKWISE (90)
// Collision:	lara_void_func()
void lara_as_rope_turn_clockwise(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	if (IsHeld(In::Action))
	{
		if (IsHeld(In::Left))
			lara->Control.Rope.Y += ANGLE(1.4f);
		else
			item->Animation.TargetState = LS_ROPE_IDLE;
	}
	else
		FallFromRope(item);
}

// State:		LS_ROPE_TURN_COUNTER_CLOCKWISE (91)
// Collision:	lara_void_func()
void lara_as_rope_turn_counter_clockwise(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	if (IsHeld(In::Action))
	{
		if (IsHeld(In::Right))
			lara->Control.Rope.Y -= ANGLE(1.4f);
		else
			item->Animation.TargetState = LS_ROPE_IDLE;
	}
	else
		FallFromRope(item);
}

// State:		LS_ROPE_IDLE (111), LS_ROPE_SWING (114), LS_ROPE_UNKNOWN (115)
// Collision:	lara_vcol_rope_idle() (111), lara_col_rope_swing() (114, 115)
void lara_as_rope_idle(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	lara->Control.Look.Mode = LookMode::Free;

	if (!IsHeld(In::Action))
		FallFromRope(item);
}

// State:	LS_ROPE_IDLE (111)
// Control:	lara_as_rope_idle()
void lara_col_rope_idle(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	if (IsHeld(In::Action))
	{
		UpdateRopeSwing(item);
		RopeSwingCollision(item, coll, false);

		if (IsHeld(In::Sprint))
		{
			item->Animation.TargetState = LS_ROPE_SWING;
			lara->Control.Rope.DFrame = (GetAnimData(LA_ROPE_SWING).frameBase + 32) << 8;
			lara->Control.Rope.Frame = lara->Control.Rope.DFrame;
		}
		else if (IsHeld(In::Forward) && lara->Control.Rope.Segment > 4)
			item->Animation.TargetState = LS_ROPE_UP;
		else if (IsHeld(In::Back) && lara->Control.Rope.Segment < 21)
		{
			item->Animation.TargetState = LS_ROPE_DOWN;
			lara->Control.Rope.Flag = 0;
			lara->Control.Rope.Count = 0;
		}
		else if (IsHeld(In::Left))
			item->Animation.TargetState = LS_ROPE_TURN_CLOCKWISE;
		else if (IsHeld(In::Right))
			item->Animation.TargetState = LS_ROPE_TURN_COUNTER_CLOCKWISE;
	}
	else
		FallFromRope(item);
}

// State:	LS_ROPE_SWING (114), LS_ROPE_UNKNOWN (115)
// Control:	lara_as_rope_idle()
void lara_col_rope_swing(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	Camera.targetDistance = BLOCK(2);

	UpdateRopeSwing(item);
	RopeSwingCollision(item, coll, true);

	if (item->Animation.AnimNumber == LA_ROPE_SWING)
	{
		if (IsHeld(In::Sprint))
		{
			int velocity;

			if (abs(lara->Control.Rope.LastX) < 9000)
				velocity = 192 * (9000 - abs(lara->Control.Rope.LastX)) / 9000;
			else
				velocity = 0;

			ApplyVelocityToRope(
				lara->Control.Rope.Segment - 2,
				item->Pose.Orientation.y + (lara->Control.Rope.Direction ? 0 : ANGLE(180.0f)),
				velocity >> 5);
		}

		if (lara->Control.Rope.Frame > lara->Control.Rope.DFrame)
		{
			lara->Control.Rope.Frame -= (unsigned short)lara->Control.Rope.FrameRate;
			if (lara->Control.Rope.Frame < lara->Control.Rope.DFrame)
				lara->Control.Rope.Frame = lara->Control.Rope.DFrame;
		}
		else if (lara->Control.Rope.Frame < lara->Control.Rope.DFrame)
		{
			lara->Control.Rope.Frame += (unsigned short)lara->Control.Rope.FrameRate;
			if (lara->Control.Rope.Frame > lara->Control.Rope.DFrame)
				lara->Control.Rope.Frame = lara->Control.Rope.DFrame;
		}

		item->Animation.FrameNumber = lara->Control.Rope.Frame >> 8;

		if (!IsHeld(In::Sprint) &&
			item->Animation.FrameNumber == GetAnimData(LA_ROPE_SWING).frameBase + 32 &&
			lara->Control.Rope.MaxXBackward < 6750 &&
			lara->Control.Rope.MaxXForward < 6750)
		{
			item->Animation.TargetState = LS_ROPE_IDLE;
			item->Animation.ActiveState = LS_ROPE_IDLE;
			item->Animation.AnimNumber = LA_JUMP_UP_TO_ROPE_END;
			item->Animation.FrameNumber = GetAnimData(item).frameBase;
		}

		if (IsHeld(In::Jump))
			JumpOffRope(item);
	}
	else if (item->Animation.FrameNumber == GetAnimData(LA_ROPE_IDLE_TO_SWING).frameBase + 15)
		ApplyVelocityToRope(lara->Control.Rope.Segment, item->Pose.Orientation.y, 128);
}

// State:	LS_ROPE_UP (112)
// Control:	lara_void_func()
void lara_as_rope_up(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	if (IsHeld(In::Roll))
		FallFromRope(item);
	else
	{
		Camera.targetAngle = ANGLE(30.0f);

		if (GetAnimData(*item).frameEnd == item->Animation.FrameNumber)
		{
			item->Animation.FrameNumber = GetAnimData(*item).frameBase;
			lara->Control.Rope.Segment -= 2;
		}

		if (!IsHeld(In::Forward) || lara->Control.Rope.Segment <= 4)
			item->Animation.TargetState = LS_ROPE_IDLE;
	}
}

// State:	LS_ROPE_DOWN (113)
// Control:	lara_void_func()
void lara_as_rope_down(ItemInfo* item, CollisionInfo* coll)
{
	LaraClimbRope(item, coll);
}

// -------------
// VERTICAL POLE
// -------------

// State:		LS_POLE_IDLE (99)
// Collision:	lara_col_pole_idle()
void lara_as_pole_idle(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	lara->Control.Look.Mode = LookMode::Free;
	coll->Setup.EnableSpasm = false;
	coll->Setup.EnableObjectPush = false;

	if (item->HitPoints <= 0)
	{
		item->Animation.TargetState = LS_FREEFALL; // TODO: Death state dispatch.
		return;
	}

	if (IsHeld(In::Action))
	{
		if (item->Animation.ActiveState == LA_POLE_IDLE) // HACK.
		{
			if (IsHeld(In::Left) || IsHeld(In::Right))
				ModulateLaraTurnRateY(item, LARA_POLE_TURN_RATE_ACCEL, 0, LARA_POLE_TURN_RATE_MAX, true);
		}

		// TODO: Add forward jump.
		if (IsHeld(In::Jump))
		{
			item->Animation.TargetState = LS_JUMP_BACK;
			return;
		}

		if (IsHeld(In::Forward) && TestLaraPoleUp(item, coll))
		{
			item->Animation.TargetState = LS_POLE_UP;
			return;
		}
		else if (IsHeld(In::Back) && TestLaraPoleDown(item, coll))
		{
			item->Animation.TargetState = LS_POLE_DOWN;
			return;
		}

		if (IsHeld(In::Left))
		{
			item->Animation.TargetState = LS_POLE_TURN_CLOCKWISE;
			return;
		}
		else if (IsHeld(In::Right))
		{
			item->Animation.TargetState = LS_POLE_TURN_COUNTER_CLOCKWISE;
			return;
		}

		item->Animation.TargetState = LS_POLE_IDLE;
		return;
	}

	GetCollisionInfo(coll, item); // HACK: Lara may step off poles in mid-air upon reload without this.
	if (coll->Middle.Floor <= 0 &&
		item->Animation.AnimNumber != LA_POLE_JUMP_BACK) // Hack.
	{
		item->Animation.TargetState = LS_IDLE;
		return;
	}
	else if (item->Animation.AnimNumber == LA_POLE_IDLE)
	{
		item->Animation.TargetState = LS_FREEFALL;

		// TODO: This shouldn't be required, but the set position command doesn't move Lara correctly.
		item->Pose.Position.x -= phd_sin(item->Pose.Orientation.y) * 64;
		item->Pose.Position.z -= phd_cos(item->Pose.Orientation.y) * 64;
	}
}

// State:		LS_POLE_IDLE (99)
// Control:		lara_as_pole_idle()
void lara_col_pole_idle(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	lara->Control.MoveAngle = item->Pose.Orientation.y;
	coll->Setup.LowerFloorBound = NO_LOWER_BOUND;
	coll->Setup.UpperFloorBound = -STEPUP_HEIGHT;
	coll->Setup.LowerCeilingBound = BAD_JUMP_CEILING;
	coll->Setup.ForwardAngle = lara->Control.MoveAngle;
	coll->Setup.Radius = LARA_RADIUS;
	coll->Setup.BlockFloorSlopeUp = true;
	GetCollisionInfo(coll, item);

	// TODO: There's a visible snap if Lara hits the ground at a high velocity.
	if (coll->Middle.Floor < 0)
		item->Pose.Position.y += coll->Middle.Floor;
}

// State:		LS_POLE_UP (100)
// Collision:	lara_col_pole_up()
void lara_as_pole_up(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	coll->Setup.EnableObjectPush = false;
	coll->Setup.EnableSpasm = false;

	if (item->HitPoints <= 0)
	{
		item->Animation.TargetState = LS_POLE_IDLE; // TODO: Death state dispatch.
		return;
	}

	if (IsHeld(In::Action))
	{
		if (IsHeld(In::Left) || IsHeld(In::Right))
			ModulateLaraTurnRateY(item, LARA_POLE_TURN_RATE_ACCEL, 0, LARA_POLE_TURN_RATE_MAX, true);

		if (IsHeld(In::Jump))
		{
			item->Animation.TargetState = LS_POLE_IDLE;
			return;
		}

		if (IsHeld(In::Forward) && TestLaraPoleUp(item, coll))
		{
			item->Animation.TargetState = LS_POLE_UP;
			return;
		}

		item->Animation.TargetState = LS_POLE_IDLE;
		return;
	}

	item->Animation.TargetState = LS_POLE_IDLE; // TODO: Dispatch to freefall?
}

// State:		LS_POLE_UP (100)
// Control:		lara_as_pole_up()
void lara_col_pole_up(ItemInfo* item, CollisionInfo* coll)
{
	lara_col_pole_idle(item, coll);
}

// State:		LS_POLE_DOWN (101)
// Collision:	lara_col_pole_down()
void lara_as_pole_down(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	coll->Setup.EnableSpasm = false;
	coll->Setup.EnableObjectPush = false;

	if (item->HitPoints <= 0)
	{
		item->Animation.TargetState = LS_POLE_IDLE; // TODO: Death state dispatch.
		return;
	}

	// TODO: In WAD.
	SoundEffect(SFX_TR4_LARA_POLE_SLIDE_LOOP, &item->Pose);

	if (IsHeld(In::Action))
	{
		if (IsHeld(In::Left) || IsHeld(In::Right))
			ModulateLaraTurnRateY(item, LARA_POLE_TURN_RATE_ACCEL, 0, LARA_POLE_TURN_RATE_MAX, true);

		if (IsHeld(In::Jump))
		{
			item->Animation.TargetState = LS_POLE_IDLE;
			return;
		}

		if (IsHeld(In::Back) && TestLaraPoleDown(item, coll))
		{
			item->Animation.TargetState = LS_POLE_DOWN;
			return;
		}

		item->Animation.TargetState = LS_POLE_IDLE;
		item->Animation.Velocity.y = 0;
		return;
	}

	item->Animation.TargetState = LS_POLE_IDLE; // TODO: Dispatch to freefall?
}

// State:		LS_POLE_DOWN (101)
// Control:		lara_as_pole_down()
void lara_col_pole_down(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	lara->Control.MoveAngle = item->Pose.Orientation.y;
	coll->Setup.LowerFloorBound = NO_LOWER_BOUND;
	coll->Setup.UpperFloorBound = -STEPUP_HEIGHT;
	coll->Setup.LowerCeilingBound = 0;
	coll->Setup.BlockFloorSlopeUp = true;
	coll->Setup.ForwardAngle = lara->Control.MoveAngle;
	coll->Setup.Radius = LARA_RADIUS;
	GetCollisionInfo(coll, item);

	// TODO: Pitch modulation might be a fun idea.

	if (item->Animation.AnimNumber == LA_POLE_DOWN_END)
		item->Animation.Velocity.y -= 8;
	else
		item->Animation.Velocity.y += 1;

	if (item->Animation.Velocity.y < 0)
		item->Animation.Velocity.y = 0;
	else if (item->Animation.Velocity.y > 64)
		item->Animation.Velocity.y = 64;
	
	// TODO: Do something about that ugly snap at the bottom.
	if ((coll->Middle.Floor + item->Animation.Velocity.y) < 0)
		item->Pose.Position.y += coll->Middle.Floor;
	else if (TestLaraPoleCollision(item, coll, false))
		item->Pose.Position.y += item->Animation.Velocity.y;
}

// State:		LS_POLE_TURN_CLOCKWISE (102)
// Collision:	lara_col_pole_turn_clockwise()
void lara_as_pole_turn_clockwise(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	lara->Control.Look.Mode = LookMode::Vertical;
	coll->Setup.EnableObjectPush = false;
	coll->Setup.EnableSpasm = false;

	if (item->HitPoints <= 0)
	{
		item->Animation.TargetState = LS_POLE_IDLE; // TODO: Death state dispatch.
		return;
	}

	if (IsHeld(In::Action))
	{
		if (IsHeld(In::Forward) && TestLaraPoleUp(item, coll))
		{
			item->Animation.TargetState = LS_POLE_IDLE; // TODO: Dispatch to climp up.
			return;
		}
		else if (IsHeld(In::Back) && TestLaraPoleDown(item, coll))
		{
			item->Animation.TargetState = LS_POLE_IDLE; // TODO: Dispatch to climb down.
			return;
		}

		if (IsHeld(In::Left))
		{
			item->Animation.TargetState = LS_POLE_TURN_CLOCKWISE;
			ModulateLaraTurnRateY(item, LARA_POLE_TURN_RATE_ACCEL, 0, LARA_POLE_TURN_RATE_MAX, true);
			return;
		}

		item->Animation.TargetState = LS_POLE_IDLE;
		return;
	}

	item->Animation.TargetState = LS_POLE_IDLE; // TODO: Dispatch to freefall.
}

// State:		LS_POLE_TURN_CLOCKWISE (102)
// Control:		lara_as_pole_turn_clockwise()
void lara_col_pole_turn_clockwise(ItemInfo* item, CollisionInfo* coll)
{
	lara_col_pole_idle(item, coll);
}

// State:		LS_POLE_TURN_COUNTER_CLOCKWISE (103)
// Collision:	lara_col_pole_turn_counter_clockwise()
void lara_as_pole_turn_counter_clockwise(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	lara->Control.Look.Mode = LookMode::Vertical;
	coll->Setup.EnableObjectPush = false;
	coll->Setup.EnableSpasm = false;

	if (item->HitPoints <= 0)
	{
		item->Animation.TargetState = LS_POLE_IDLE; // TODO: Death state dispatch.
		return;
	}

	if (IsHeld(In::Action))
	{
		if (IsHeld(In::Forward) && TestLaraPoleUp(item, coll))
		{
			item->Animation.TargetState = LS_POLE_IDLE; // TODO: Dispatch to climb up.
			return;
		}
		else if (IsHeld(In::Back) && TestLaraPoleDown(item, coll))
		{
			item->Animation.TargetState = LS_POLE_IDLE; // TODO: Dispatch to climb down.
			return;
		}

		if (IsHeld(In::Right))
		{
			item->Animation.TargetState = LS_POLE_TURN_COUNTER_CLOCKWISE;
			ModulateLaraTurnRateY(item, LARA_POLE_TURN_RATE_ACCEL, 0, LARA_POLE_TURN_RATE_MAX, true);
			return;
		}

		item->Animation.TargetState = LS_POLE_IDLE;
		return;
	}

	item->Animation.TargetState = LS_POLE_IDLE; // TODO: Dispatch to freefall.
}

// State:		LS_POLE_TURN_COUNTER_CLOCKWISE (103)
// Control:		lara_col_pole_turn_counter_clockwise()
void lara_col_pole_turn_counter_clockwise(ItemInfo* item, CollisionInfo* coll)
{
	lara_col_pole_idle(item, coll);
}

// --------
// ZIP-LINE
// --------

// State:		LS_ZIP_LINE (70)
// Collision:	lara_void_func()
void lara_as_zip_line(ItemInfo* item, CollisionInfo* coll)
{
	auto* lara = GetLaraInfo(item);

	lara->Control.Look.Mode = LookMode::Free;
	Camera.targetAngle = ANGLE(70.0f);

	if (!IsHeld(In::Action))
	{
		item->Animation.TargetState = LS_JUMP_FORWARD;
		AnimateItem(item);

		item->Animation.Velocity.z = 100;
		item->Animation.Velocity.y = 40;
		item->Animation.IsAirborne = true;
		lara->Control.MoveAngle = item->Pose.Orientation.y;
	}
}
