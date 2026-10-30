#include "framework.h"
#include "Game/Lara/RemasteredControls.h"

#include "Game/camera.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/floordata.h"
#include "Game/collision/Point.h"
#include "Game/control/control.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_fire.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/Lara/lara_struct.h"
#include "Game/Lara/lara_tests.h"
#include "Math/Math.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Scripting/Include/ScriptInterfaceLevel.h"
#include "Specific/configuration.h"
#include "Specific/Input/Input.h"

using namespace TEN::Collision::Floordata;
using namespace TEN::Collision::Point;
using namespace TEN::Input;
using namespace TEN::Math;

// Remastered controls: input rewrite and heading solver for camera-relative movement. -- divezzi, 10.10.26

enum class RemasteredInputScope
{
	None,
	Ground,

	// Mid-air. The solver turns the player, and the transform rewrites the actions.
	Airborne,

	// Mid-air. The transform rewrites the actions, and the solver does not turn the player.
	AirborneNoSolve,

	// Weapons ready, standing. The transform presses the turn keys and the stock turn states turn the player.
	ArmedSynthesis
};

// Grab and weapon button: Fire under Remastered controls, Action under Classic controls.
// A script clear of Action holds it off, and a script push of Action presses it.
bool IsRemasteredGrabHeld()
{
	if (!g_Configuration.IsUsingRemasteredControls())
		return IsHeld(In::Action);

	if (IsCleared(In::Action))
		return false;

	if (IsPushed(In::Action))
		return true;

	return IsHeld(In::Fire);
}

// Vehicle gun button: Action under Remastered controls, where Accelerate and Fire share the gamepad's right trigger.
// Fire under Classic controls. A script clear of Fire holds it off, and a script push of Fire presses it.
bool IsRemasteredVehicleFireHeld()
{
	if (!g_Configuration.IsUsingRemasteredControls())
		return IsHeld(In::Fire);

	if (IsCleared(In::Fire))
		return false;

	if (IsPushed(In::Fire))
		return true;

	return IsHeld(In::Action);
}

// Steps an angle toward a target by a share of the remaining error, clamped between a minimum and a maximum step.
// An exact half-turn error keeps the previous turn direction, stored in prevSign.
static short ApproachHeadingAngle(short fromAngle, short toAngle, float rate, short stepMax, int& prevSign)
{
	constexpr auto STEP_MIN = ANGLE(0.5f);

	short deltaAngle = Geometry::GetShortestAngle(fromAngle, toAngle);
	int deltaAngleAbs = abs((int)deltaAngle);
	if (deltaAngleAbs == 0)
		return toAngle;

	float step = ((float)deltaAngleAbs * rate) / PI;
	step = std::clamp(step, (float)STEP_MIN, (float)stepMax);
	if (step >= (float)deltaAngleAbs)
		return toAngle;

	int sign = (deltaAngle < 0) ? -1 : 1;
	if (deltaAngle == SHRT_MIN && prevSign != 0)
		sign = prevSign;

	prevSign = sign;
	return (short)(fromAngle + (short)((int)step * sign));
}

// Weapons drawn and ready. A torch counts as holstered.
static bool IsPlayerRemasteredWeaponReady(const ItemInfo& item)
{
	const auto& player = GetLaraInfo(item);
	return (player.Control.HandStatus == HandStatus::WeaponReady &&
			player.Control.Weapon.GunType != LaraWeaponType::Torch);
}

// Weapons drawn or being drawn. A torch counts as holstered.
static bool IsPlayerInRemasteredCombatBackStance(const ItemInfo& item)
{
	const auto& player = GetLaraInfo(item);
	return ((player.Control.HandStatus == HandStatus::WeaponReady ||
			 player.Control.HandStatus == HandStatus::WeaponDraw) &&
			player.Control.Weapon.GunType != LaraWeaponType::Torch);
}

static bool IsRemasteredStandingState(const ItemInfo& item)
{
	switch (item.Animation.ActiveState)
	{
	case LS_IDLE:
	case LS_SOFT_SPLAT:
	case LS_TURN_LEFT_SLOW:
	case LS_TURN_RIGHT_SLOW:
	case LS_TURN_LEFT_FAST:
	case LS_TURN_RIGHT_FAST:
	case LS_JUMP_PREPARE:
		return true;

	default:
		return false;
	}
}

static bool IsPlayerLockedOnTarget(const ItemInfo& item)
{
	const auto& player = GetLaraInfo(item);

	if (player.TargetEntity == nullptr)
		return false;

	return (player.LeftArm.Locked || player.RightArm.Locked);
}

// Hanging, shimmying, monkey swing, ladder, rope and pole states.
static bool DoesRemasteredInputOwnGrip(const ItemInfo& item)
{
	if (!g_Configuration.IsUsingRemasteredControls())
		return false;

	switch (item.Animation.ActiveState)
	{
	case LS_MONKEY_IDLE:
	case LS_MONKEY_FORWARD:
	case LS_MONKEY_BACK:
	case LS_MONKEY_TURN_LEFT:
	case LS_MONKEY_TURN_RIGHT:
	case LS_MONKEY_TURN_180:
	case LS_MONKEY_SHIMMY_LEFT:
	case LS_MONKEY_SHIMMY_RIGHT:
	case LS_HANG:
	case LS_SHIMMY_LEFT:
	case LS_SHIMMY_RIGHT:
	case LS_SHIMMY_OUTER_LEFT:
	case LS_SHIMMY_OUTER_RIGHT:
	case LS_SHIMMY_INNER_LEFT:
	case LS_SHIMMY_INNER_RIGHT:
	case LS_SHIMMY_45_OUTER_LEFT:
	case LS_SHIMMY_45_OUTER_RIGHT:
	case LS_SHIMMY_45_INNER_LEFT:
	case LS_SHIMMY_45_INNER_RIGHT:
	case LS_LADDER_IDLE:
	case LS_LADDER_STOP:
	case LS_LADDER_UP:
	case LS_LADDER_DOWN:
	case LS_LADDER_LEFT:
	case LS_LADDER_RIGHT:
	case LS_ROPE_IDLE:
	case LS_ROPE_UP:
	case LS_ROPE_SWING:
	case LS_ROPE_UNKNOWN:
	case LS_ROPE_TURN_CLOCKWISE:
	case LS_ROPE_TURN_COUNTER_CLOCKWISE:
	case LS_ROPE_DOWN:
	case LS_POLE_IDLE:
	case LS_POLE_UP:
	case LS_POLE_DOWN:
	case LS_POLE_TURN_CLOCKWISE:
	case LS_POLE_TURN_COUNTER_CLOCKWISE:
		return true;

	default:
		return false;
	}
}

// The grip in a hanging state. Roll always lets go. With the grab toggle off, releasing the grab button lets go too.
static bool GetRemasteredGrip(bool isGrabHeld, bool isRollHeld)
{
	if (isRollHeld)
		return false;

	return (g_Configuration.EnableGrabToggle || isGrabHeld);
}

static bool IsPlayerRemasteredOnKnees(const ItemInfo& item)
{
	switch (item.Animation.ActiveState)
	{
	case LS_CRAWL_IDLE:
	case LS_CRAWL_FORWARD:
	case LS_CRAWL_BACK:
	case LS_CRAWL_TURN_LEFT:
	case LS_CRAWL_TURN_RIGHT:
	case LS_CRAWL_TURN_180:
		return true;

	default:
		return false;
	}
}

// Walk with a pure sideways push, read from the raw keys. The stock crawl turn plays.
static bool IsPlayerRemasteredCrawlTurnHeld()
{
	return (IsRawActionHeld(In::Walk) && (IsRawActionHeld(In::Left) || IsRawActionHeld(In::Right)) &&
			!IsRawActionHeld(In::Forward) && !IsRawActionHeld(In::Back));
}

// A script pushed a direction this frame. That tick plays classic.
static bool IsPlayerRemasteredDirectionPushed()
{
	return (IsPushed(In::Forward) || IsPushed(In::Back) || IsPushed(In::Left) || IsPushed(In::Right));
}

// Turn rate and step cap of the heading solver in the player's current state.
// Scope None leaves the state to play classic.
static RemasteredInputScope GetPlayerRemasteredTurnParams(const ItemInfo& item, float& rate, short& stepMax)
{
	constexpr auto TURN_RATE_IDLE		= 1.0f;
	constexpr auto TURN_RATE_WALK		= 1.0f;
	constexpr auto TURN_RATE_RUN		= 1.0f;
	constexpr auto TURN_RATE_SPRINT		= 0.2f;
	constexpr auto TURN_RATE_WADE		= 0.6f;
	constexpr auto TURN_RATE_CRAWL_IDLE = 0.5f;
	constexpr auto TURN_RATE_CRAWL		= 0.2f;

	constexpr auto TURN_STEP_MAX_IDLE		= ANGLE(90.0f);
	constexpr auto TURN_STEP_MAX_WALK		= ANGLE(45.0f);
	constexpr auto TURN_STEP_MAX_RUN		= ANGLE(30.0f);
	constexpr auto TURN_STEP_MAX_SPRINT		= ANGLE(5.0f);
	constexpr auto TURN_STEP_MAX_WADE		= ANGLE(15.0f);
	constexpr auto TURN_STEP_MAX_CRAWL_IDLE = ANGLE(90.0f);
	constexpr auto TURN_STEP_MAX_CRAWL		= ANGLE(7.0f);

	constexpr auto TURN_RATE_MONKEY		= 0.2f;
	constexpr auto TURN_STEP_MAX_MONKEY = ANGLE(7.0f);

	constexpr auto TURN_RATE_JUMP_PREPARE	  = 1.0f;
	constexpr auto TURN_STEP_MAX_JUMP_PREPARE = (short)SHRT_MAX;
	constexpr auto TURN_RATE_AIRBORNE		  = 0.1f;
	constexpr auto TURN_STEP_MAX_AIRBORNE	  = ANGLE(2.0f);

	constexpr auto TURN_RATE_AIM_LOCKED		= 0.25f;
	constexpr auto TURN_STEP_MAX_AIM_LOCKED = ANGLE(7.0f);

	if (IsPlayerRemasteredDirectionPushed())
		return RemasteredInputScope::None;

	if (IsPlayerRemasteredWeaponReady(item))
	{
		switch (item.Animation.ActiveState)
		{
		case LS_IDLE:
		case LS_SOFT_SPLAT:
		case LS_TURN_LEFT_SLOW:
		case LS_TURN_RIGHT_SLOW:
		case LS_TURN_LEFT_FAST:
		case LS_TURN_RIGHT_FAST:
			return RemasteredInputScope::ArmedSynthesis;
		}
	}

	switch (item.Animation.ActiveState)
	{
	case LS_IDLE:
	case LS_SOFT_SPLAT:
	case LS_TURN_LEFT_SLOW:
	case LS_TURN_RIGHT_SLOW:
	case LS_TURN_LEFT_FAST:
	case LS_TURN_RIGHT_FAST:
		rate = TURN_RATE_IDLE;
		stepMax = TURN_STEP_MAX_IDLE;
		return RemasteredInputScope::Ground;

	case LS_WALK_FORWARD:
		rate = TURN_RATE_WALK;
		stepMax = TURN_STEP_MAX_WALK;
		return RemasteredInputScope::Ground;

	case LS_RUN_FORWARD:
		if (IsPlayerLockedOnTarget(item))
		{
			rate = TURN_RATE_AIM_LOCKED;
			stepMax = TURN_STEP_MAX_AIM_LOCKED;
		}
		else
		{
			rate = TURN_RATE_RUN;
			stepMax = TURN_STEP_MAX_RUN;
		}

		return RemasteredInputScope::Ground;

	case LS_SPRINT:
		rate = TURN_RATE_SPRINT;
		stepMax = TURN_STEP_MAX_SPRINT;
		return RemasteredInputScope::Ground;

	case LS_WADE_FORWARD:
		rate = TURN_RATE_WADE;
		stepMax = TURN_STEP_MAX_WADE;
		return RemasteredInputScope::Ground;

	case LS_STEP_LEFT:
	case LS_STEP_RIGHT:
		rate = TURN_RATE_IDLE;
		stepMax = TURN_STEP_MAX_IDLE;
		return RemasteredInputScope::Ground;

	case LS_CRAWL_IDLE:
	case LS_CRAWL_TURN_LEFT:
	case LS_CRAWL_TURN_RIGHT:
		if (IsPlayerRemasteredCrawlTurnHeld())
			return RemasteredInputScope::None;

		rate = TURN_RATE_CRAWL_IDLE;
		stepMax = TURN_STEP_MAX_CRAWL_IDLE;
		return RemasteredInputScope::Ground;

	case LS_CRAWL_FORWARD:
	case LS_CRAWL_BACK:
		rate = TURN_RATE_CRAWL;
		stepMax = TURN_STEP_MAX_CRAWL;
		return RemasteredInputScope::Ground;

	case LS_MONKEY_IDLE:
	case LS_MONKEY_FORWARD:
	case LS_MONKEY_BACK:
	case LS_MONKEY_TURN_LEFT:
	case LS_MONKEY_TURN_RIGHT:
	case LS_MONKEY_SHIMMY_LEFT:
	case LS_MONKEY_SHIMMY_RIGHT:
		rate = TURN_RATE_MONKEY;
		stepMax = TURN_STEP_MAX_MONKEY;
		return RemasteredInputScope::Ground;

	// Jump wind-up. Holstered, she turns toward the stick before the launch. Armed, it plays classic.
	case LS_JUMP_PREPARE:
		if (IsPlayerRemasteredWeaponReady(item))
			return RemasteredInputScope::None;

		rate = TURN_RATE_JUMP_PREPARE;
		stepMax = TURN_STEP_MAX_JUMP_PREPARE;
		return RemasteredInputScope::Ground;

	// The back jump keeps its launch heading, armed or not, so its roll push is read the same way.
	case LS_JUMP_BACK:
		return RemasteredInputScope::AirborneNoSolve;

	case LS_JUMP_LEFT:
	case LS_JUMP_RIGHT:
		if (IsPlayerRemasteredWeaponReady(item))
			return RemasteredInputScope::None;

		return RemasteredInputScope::AirborneNoSolve;

	// The forward jump is the only jump the solver steers mid-air.
	case LS_JUMP_FORWARD:
		rate = TURN_RATE_AIRBORNE;
		stepMax = TURN_STEP_MAX_AIRBORNE;
		return RemasteredInputScope::Airborne;

	case LS_JUMP_UP:
	case LS_FREEFALL:
	case LS_REACH:
	case LS_SWAN_DIVE:
		return RemasteredInputScope::AirborneNoSolve;

	default:
		return RemasteredInputScope::None;
	}
}

// True if the heading solver turns the player in the current state.
bool DoesPlayerRemasteredSolverOwnHeading(const ItemInfo& item)
{
	float rate = 0.0f;
	short stepMax = 0;
	auto scope = GetPlayerRemasteredTurnParams(item, rate, stepMax);
	return (scope == RemasteredInputScope::Ground || scope == RemasteredInputScope::Airborne);
}

// True if the camera turns toward the player's movement. Ground states only.
bool DoesPlayerRemasteredCameraAssistApply(const ItemInfo& item)
{
	float rate = 0.0f;
	short stepMax = 0;
	return (GetPlayerRemasteredTurnParams(item, rate, stepMax) == RemasteredInputScope::Ground);
}

// Turn rate and step cap of the heading solver in the water states it steers. False in any other state,
// on a script direction push, and with the divesuit, which keeps its own pitch control.
// isUnderwater is set for the states that also follow the camera pitch and swim on Jump: underwater and the fly cheat.
static bool GetPlayerRemasteredWaterTurnParams(const ItemInfo& item, float& rate, short& stepMax, bool& isUnderwater)
{
	constexpr auto TURN_RATE_UNDERWATER = 1.0f;
	constexpr auto TURN_RATE_SURFACE	= 0.5f;
	constexpr auto TURN_STEP_MAX		= ANGLE(179.0f); // Not reached: a step is at most 57 degrees at rate 1.0.

	isUnderwater = false;

	if (IsPlayerRemasteredDirectionPushed())
		return false;

	const auto& level = *g_GameFlow->GetLevel(CurrentLevel);
	if (level.GetLaraType() == LaraType::Divesuit)
		return false;

	switch (item.Animation.ActiveState)
	{
	case LS_FLY_CHEAT:
	case LS_UNDERWATER_IDLE:
	case LS_UNDERWATER_SWIM_FORWARD:
	case LS_UNDERWATER_INERTIA:
		rate = TURN_RATE_UNDERWATER;
		stepMax = TURN_STEP_MAX;
		isUnderwater = true;
		return true;

	case LS_ONWATER_IDLE:
	case LS_ONWATER_FORWARD:
	case LS_ONWATER_LEFT:
	case LS_ONWATER_RIGHT:
	case LS_ONWATER_BACK:
		rate = TURN_RATE_SURFACE;
		stepMax = TURN_STEP_MAX;
		return true;

	default:
		return false;
	}
}

// True if the heading solver turns the player in the current water state.
bool DoesRemasteredSolverOwnWaterHeading(const ItemInfo& item)
{
	if (!g_Configuration.IsUsingRemasteredControls())
		return false;

	float rate = 0.0f;
	short stepMax = 0;
	bool isUnderwater = false;
	return GetPlayerRemasteredWaterTurnParams(item, rate, stepMax, isUnderwater);
}

// Heading the stick points to, relative to the camera. The player's own heading when the stick is centered.
short GetPlayerHeadingAngleY(const ItemInfo& item)
{
	if (!g_Configuration.IsUsingRemasteredControls())
		return item.Pose.Orientation.y;

	const auto& player = GetLaraInfo(item);
	auto dir = player.Control.RefMoveAxis;
	if (dir == Vector2::Zero)
		return item.Pose.Orientation.y;

	dir.Normalize();
	short moveAxisAngle = FROM_RAD(std::atan2(dir.x, dir.y));
	return (short)(player.Control.RefCameraOrient.y + moveAxisAngle);
}

// A diagonal step or wall met within 45 degrees of square-on resets the side collision, so the player
// moves on until the front probe stops her instead of being deflected every tick. Returns true if reset.
bool HandlePlayerRemasteredDiagonalStep(const ItemInfo& item, CollisionInfo& coll)
{
	if (!g_Configuration.IsUsingRemasteredControls())
		return false;

	switch (item.Animation.ActiveState)
	{
	case LS_WALK_FORWARD:
	case LS_RUN_FORWARD:
	case LS_SPRINT:
	case LS_WADE_FORWARD:
		break;

	default:
		return false;
	}

	bool isLeft = (coll.CollisionType == CollisionType::Left);
	if (!isLeft && coll.CollisionType != CollisionType::Right)
		return false;

	auto& sidePos = isLeft ? coll.MiddleLeft : coll.MiddleRight;
	bool isTriangle = isLeft ? coll.TriangleAtLeft() : coll.TriangleAtRight();
	if (!isTriangle || !sidePos.HasDiagonalSplit())
		return false;

	int quarter = ((unsigned short)coll.Setup.ForwardAngle / ANGLE(90.0f)) % 2;
	bool isFacingStep = (sidePos.HasFlippedDiagonalSplit() != (quarter != 0));
	if (!isFacingStep)
		return false;

	coll.CollisionType = CollisionType::None;
	return true;
}

// Safety grab probe: taken behind the player, its base 381 units below her feet. The lip must be level
// across her within 78 units and square to the grid within 35 degrees.
constexpr auto SAFETY_GRAB_PROBE_HEADING_OFFSET	 = (short)0x7ff8;
constexpr auto SAFETY_GRAB_PROBE_VERTICAL_OFFSET = 0x17d;
constexpr auto SAFETY_GRAB_LIP_TOLERANCE		 = 0x4e;
constexpr auto SAFETY_GRAB_ANGLE_TOLERANCE		 = ANGLE(35.0f);

static bool IsAngleNearCardinal(short angle, short toleranceAngle)
{
	short nearestCardinalAngle = (short)(GetQuadrant(angle) * ANGLE(90.0f));
	return (abs((int)Geometry::GetShortestAngle(angle, nearestCardinalAngle)) <= toleranceAngle);
}

// Angle of the diagonal lip under the player, and her distance to it. False if the sector has no diagonal split.
static bool GetRemasteredDiagonalLip(const ItemInfo& item, short& lipAngle, float& lipDistance)
{
	auto pointColl = GetPointCollision(item, 0, 0, -LARA_HEIGHT / 2);
	const auto& sector = pointColl.GetSector();

	short splitAngle = sector.FloorSurface.SplitAngle;
	if (!sector.IsSurfaceSplit(true) ||
		(splitAngle != SectorSurfaceData::SPLIT_ANGLE_0 && splitAngle != SectorSurfaceData::SPLIT_ANGLE_1))
	{
		return false;
	}

	int triID = sector.GetSurfaceTriangleID(item.Pose.Position.x, item.Pose.Position.z, true);
	lipAngle = (short)(splitAngle + ANGLE(triID * 180.0f) + ANGLE(90.0f));

	auto center = GetNearestSectorCenter(item.Pose.Position);
	lipDistance = ((center.x - item.Pose.Position.x) * phd_sin(lipAngle)) +
				  ((center.z - item.Pose.Position.z) * phd_cos(lipAngle));
	return true;
}

// A diagonal lip is accepted when it is level across the player and she faces it within the tolerance.
static bool TestRemasteredSafetyGrabDiagonal(ItemInfo& item, const CollisionInfo& probe, short& turnAngle)
{
	short lipAngle = 0;
	float lipDistance = 0.0f;
	if (!GetRemasteredDiagonalLip(item, lipAngle, lipDistance))
		return false;

	auto lipProbe = probe;
	lipProbe.Setup.Mode = CollisionProbeMode::FreeFlat;
	lipProbe.Setup.ForwardAngle = lipAngle;
	GetCollisionInfo(&lipProbe, &item, Vector3i(0, SAFETY_GRAB_PROBE_VERTICAL_OFFSET, 0));

	bool isLipLevel = (abs(lipProbe.FrontLeft.Floor - lipProbe.FrontRight.Floor) < SAFETY_GRAB_LIP_TOLERANCE);
	bool isFacingLip = (abs((int)Geometry::GetShortestAngle(probe.Setup.ForwardAngle, lipAngle)) <= SAFETY_GRAB_ANGLE_TOLERANCE);
	if (!isLipLevel || !isFacingLip)
		return false;

	turnAngle = lipAngle;
	return true;
}

// Moves the player back from a diagonal lip to the reach radius, if the floor and ceiling there allow it.
static void ApplyRemasteredSafetyGrabStandOff(ItemInfo& item)
{
	constexpr auto STAND_OFF_DISTANCE = LARA_RADIUS * 1.2f;

	short lipAngle = 0;
	float lipDistance = 0.0f;
	if (!GetRemasteredDiagonalLip(item, lipAngle, lipDistance))
		return;

	short pushAngle = (short)(lipAngle + ANGLE(180.0f));
	float pushDistance = STAND_OFF_DISTANCE - lipDistance;
	if (pushDistance <= 0.0f)
		return;

	auto pointColl = GetPointCollision(item, pushAngle, pushDistance, -LARA_HEIGHT / 2);
	int floorHeight = pointColl.GetFloorHeight();
	if (floorHeight == NO_HEIGHT || floorHeight <= item.Pose.Position.y)
		return;

	if (pointColl.GetCeilingHeight() > (item.Pose.Position.y - LARA_HEIGHT))
		return;

	item.Pose.Translate(pushAngle, pushDistance);
}

// Safety grab: running off a drop of about a block with the grab button held puts the player into a reach
// facing the lip she ran off, and the stock reach code catches the ledge.
bool TestRemasteredSafetyGrab(ItemInfo& item, CollisionInfo& coll)
{
	constexpr auto DROP_HEIGHT_MIN		   = BLOCK(1);
	constexpr auto DROP_HEIGHT_TOLERANCE   = CLICK(0.25f);
	constexpr auto PROBE_RADIUS_SCALE	   = 1.3f;
	constexpr auto PROBE_FLOOR_BOUND_LOWER = 0x7f00;
	constexpr auto PROBE_FLOOR_BOUND_UPPER = -0x180;
	constexpr auto PROBE_HEIGHT			   = 0x2fa;
	constexpr auto WATER_DEPTH_MIN		   = 0x1ff;
	constexpr auto CAUGHT_VELOCITY		   = 10.0f;

	if (!g_Configuration.IsUsingRemasteredControls())
		return false;

	auto& player = GetLaraInfo(item);

	if (item.Animation.ActiveState != LS_RUN_FORWARD)
		return false;

	if (coll.Middle.Floor < (DROP_HEIGHT_MIN - DROP_HEIGHT_TOLERANCE))
		return false;

	if (!IsRemasteredGrabHeld())
		return false;

	if (player.Control.HandStatus != HandStatus::Free)
		return false;

	auto probe = coll;
	probe.Setup.Radius = (int)(coll.Setup.Radius * PROBE_RADIUS_SCALE);
	probe.Setup.LowerFloorBound = PROBE_FLOOR_BOUND_LOWER;
	probe.Setup.UpperFloorBound = PROBE_FLOOR_BOUND_UPPER;
	probe.Setup.LowerCeilingBound = 0;
	probe.Setup.Height = PROBE_HEIGHT;
	probe.Setup.ForwardAngle = (short)(item.Pose.Orientation.y + SAFETY_GRAB_PROBE_HEADING_OFFSET);
	GetCollisionInfo(&probe, &item, Vector3i(0, SAFETY_GRAB_PROBE_VERTICAL_OFFSET, 0));

	bool isLipLevel = (abs(probe.FrontLeft.Floor - probe.FrontRight.Floor) < SAFETY_GRAB_LIP_TOLERANCE);
	bool isSquareOn = IsAngleNearCardinal(probe.Setup.ForwardAngle, SAFETY_GRAB_ANGLE_TOLERANCE);

	short turnAngle = (short)(GetQuadrant(probe.Setup.ForwardAngle) * ANGLE(90.0f));
	bool isDiagonalLip = (!isLipLevel || !isSquareOn);
	if (isDiagonalLip && !TestRemasteredSafetyGrabDiagonal(item, probe, turnAngle))
		return false;

	int waterHeight = GetPointCollision(item).GetWaterSurfaceHeight();
	if (waterHeight != NO_HEIGHT && (waterHeight - item.Pose.Position.y) < WATER_DEPTH_MIN)
		return false;

	player.Control.RefMoveAxis = Vector2::Zero;
	player.Control.Remastered.SafetyGrabTurnAngle = turnAngle;
	player.Control.Remastered.SafetyGrabTurnYaw = item.Pose.Orientation.y;
	player.Control.Remastered.IsSafetyGrabTurning = true;

	if (isDiagonalLip)
		ApplyRemasteredSafetyGrabStandOff(item);

	SetAnimation(item, LA_REACH);
	item.Animation.IsAirborne = true;
	item.Animation.ActiveState = LS_REACH;
	item.Animation.TargetState = LS_REACH;
	item.Animation.Velocity.z = CAUGHT_VELOCITY;
	item.Animation.Velocity.y = 0.0f;
	return true;
}

// Tightrope idle: a held push back turns the player once, and held on after the turn she walks.
// Forward releases the turn latch. False leaves the state to the stock checks.
bool HandleRemasteredTightropeIdle(ItemInfo& item)
{
	auto& remastered = GetLaraInfo(item).Control.Remastered;

	if (IsHeld(In::Forward))
	{
		remastered.IsTightropeTurnLatched = false;
		return false;
	}

	if (IsHeld(In::Back) && !IsHeld(In::Roll))
	{
		if (remastered.IsTightropeTurnLatched)
		{
			item.Animation.TargetState = LS_TIGHTROPE_WALK;
			return true;
		}

		remastered.IsTightropeTurnLatched = true;
		item.Animation.TargetState = LS_TIGHTROPE_TURN_180;
		return true;
	}

	if (!IsHeld(In::Back))
		remastered.IsTightropeTurnLatched = false;

	return false;
}

// A script clear of Sprint, Look or Crouch resets its toggle.
static void ResetPlayerRemasteredClearedLatches(ItemInfo& item)
{
	auto& player = GetLaraInfo(item);

	if (IsCleared(In::Sprint))
		player.Control.Remastered.IsSprintToggled = false;

	if (IsCleared(In::Look))
		player.Control.Remastered.IsLookToggled = false;

	if (IsCleared(In::Crouch))
	{
		player.Control.Remastered.CrouchStage = 0;
		player.Control.Remastered.IsCrouchStageUpPending = false;
	}
}

static void ResetRemasteredArmedTurn(ItemInfo& item)
{
	auto& remastered = GetLaraInfo(item).Control.Remastered;
	remastered.IsArmedTurning = false;
	remastered.ArmedTurnErrorY = 0;
}

// A script clear holds the action off, even where the rewrite would set it.
static void MaskRemasteredClearedInput(ItemInfo& item, PlayerInput& input)
{
	ApplyClearedActions(input);

	// A script clear of the turn key the armed turn pressed stops the armed turn.
	auto& player = GetLaraInfo(item);
	if ((IsCleared(In::Left) || IsCleared(In::Right)) &&
		player.Control.Remastered.IsArmedTurning && !(input.Left || input.Right))
	{
		player.Control.TurnRate = 0;
		ResetRemasteredArmedTurn(item);
	}
}

// Sprint toggle: a press while running latches the sprint, the next press or leaving the run releases it.
static void UpdatePlayerRemasteredSprintToggle(ItemInfo& item, PlayerInput& input)
{
	auto& remastered = GetLaraInfo(item).Control.Remastered;
	bool isRunning = (item.Animation.ActiveState == LS_RUN_FORWARD || item.Animation.ActiveState == LS_SPRINT);

	if (!g_Configuration.EnableSprintToggle)
	{
		remastered.IsSprintToggled = false;
	}
	else if (IsClicked(In::Sprint) && !IsPushed(In::Sprint))
	{
		if (remastered.IsSprintToggled)
		{
			remastered.IsSprintToggled = false;
		}
		else if (isRunning)
		{
			remastered.IsSprintToggled = true;
		}
	}

	if (!isRunning)
		remastered.IsSprintToggled = false;

	if (g_Configuration.EnableSprintToggle && isRunning)
		input.Sprint = (remastered.IsSprintToggled || IsPushed(In::Sprint));
}

// Look toggle: a press latches Look and the next press releases it. The latched Look is written held, never clicked.
// Off while locked on a target, where each press switches target, and in states that cannot look.
static void UpdateRemasteredLookToggle(ItemInfo& item, PlayerInput& input, PlayerInput& heldWithoutClickInput)
{
	auto& player = GetLaraInfo(item);
	auto& remastered = player.Control.Remastered;

	if (!g_Configuration.EnableLookToggle || IsPlayerLockedOnTarget(item) ||
		player.Control.Look.Mode == LookMode::None)
	{
		remastered.IsLookToggled = false;
		return;
	}

	if (IsClicked(In::Look) && !IsPushed(In::Look))
		remastered.IsLookToggled = !remastered.IsLookToggled;

	input.Look = (remastered.IsLookToggled || IsPushed(In::Look));
	heldWithoutClickInput.Look = true;
}

// Crouch stages: standing, crouched, on knees. A press on knees rises one stage to the crouch.
static void UpdatePlayerRemasteredCrouchStages(ItemInfo& item, PlayerInput& input)
{
	auto& player = GetLaraInfo(item);
	auto& remastered = player.Control.Remastered;

	if (!g_Configuration.EnableCrouchToggle)
	{
		remastered.CrouchStage = input.Crouch ? 1 : 0;
		remastered.IsCrouchStageUpPending = false;
	}
	else if (IsClicked(In::Crouch) && !IsPushed(In::Crouch))
	{
		if (remastered.CrouchStage == 0)
		{
			remastered.CrouchStage = 1;
		}
		else if (IsPlayerRemasteredOnKnees(item))
		{
			remastered.IsCrouchStageUpPending = true;
		}
		else
		{
			remastered.CrouchStage = 0;
		}
	}

	if (remastered.IsCrouchStageUpPending)
	{
		if (IsPlayerRemasteredOnKnees(item))
		{
			remastered.IsCrouchStageUp = true;
			input.Forward = false;
			input.Back = false;
		}
		else
		{
			remastered.IsCrouchStageUpPending = false;
		}
	}

	if (player.Control.WaterStatus == WaterStatus::Underwater ||
		player.Control.WaterStatus == WaterStatus::TreadWater ||
		DoesRemasteredInputOwnGrip(item))
	{
		remastered.CrouchStage = 0;
		remastered.IsCrouchStageUpPending = false;
	}

	input.Crouch = ((remastered.CrouchStage != 0) || IsPushed(In::Crouch));
}

// True if the stick points within the roll arc: the player's back in a forward jump, her front in a back jump.
static bool IsRemasteredPushInJumpRollArc(const ItemInfo& item)
{
	constexpr auto ROLL_ARC_ANGLE = ANGLE(45.0f);

	bool isForwardJump = (item.Animation.ActiveState == LS_JUMP_FORWARD);
	short rollAngle = isForwardJump ? (short)(item.Pose.Orientation.y + ANGLE(180.0f)) : item.Pose.Orientation.y;
	short deltaAngle = Geometry::GetShortestAngle(rollAngle, GetPlayerHeadingAngleY(item));
	return (abs((int)deltaAngle) <= ROLL_ARC_ANGLE);
}

// True if the stick is pushed firmly along one axis toward the player's back (forward jump)
// or front (back jump). A light push, Look or an already spent push does not roll the player.
// TODO: Roll the player in a left or right running jump when the stick is pushed the opposite way. -- divezzi, 11.10.26
static bool IsPlayerRemasteredJumpRollPushed(const ItemInfo& item, const PlayerInput& input)
{
	constexpr auto PUSH_MIN = 0.7f; // About half the stick's travel. Keys and the D-pad give 1.0.

	const auto& player = GetLaraInfo(item);

	bool isForwardJump = (item.Animation.ActiveState == LS_JUMP_FORWARD);
	if (!isForwardJump && item.Animation.ActiveState != LS_JUMP_BACK)
		return false;

	if (input.Look || player.Control.Remastered.IsJumpRollSpent)
		return false;

	bool isVertical = (input.Forward || input.Back);
	bool isHorizontal = (input.Left || input.Right);
	if (isVertical == isHorizontal)
		return false;

	const auto& moveAxis = player.Control.RefMoveAxis;
	if (abs(isVertical ? moveAxis.y : moveAxis.x) < PUSH_MIN)
		return false;

	return IsRemasteredPushInJumpRollArc(item);
}

// A direction key or stick direction was pressed or released this frame.
static bool HasRemasteredDirectionChanged()
{
	return (IsClicked(In::Forward) || IsClicked(In::Back) || IsClicked(In::Left) || IsClicked(In::Right) ||
			IsReleased(In::Forward) || IsReleased(In::Back) || IsReleased(In::Left) || IsReleased(In::Right));
}

// The roll is an input event. A push held when the jump starts, a push that made a roll and a push held outside the
// roll arc are spent. Only a stick release or a direction press or release clears it; camera movement never does.
static void UpdatePlayerRemasteredJumpRollLatch(ItemInfo& item)
{
	auto& player = GetLaraInfo(item);
	bool isPushing = (player.Control.RefMoveAxis != Vector2::Zero);

	switch (item.Animation.ActiveState)
	{
	case LS_JUMP_ROLL_180:
		player.Control.Remastered.IsJumpRollSpent = true;
		break;

	case LS_JUMP_FORWARD:
	case LS_JUMP_BACK:
		if (!isPushing || HasRemasteredDirectionChanged())
			player.Control.Remastered.IsJumpRollSpent = false;

		if (isPushing && !IsRemasteredPushInJumpRollArc(item))
			player.Control.Remastered.IsJumpRollSpent = true;

		break;

	default:
		player.Control.Remastered.IsJumpRollSpent = isPushing;
		break;
	}
}

// Presses the turn key toward a target heading while the error is at least minAngle, so the stock turn states turn
// the player. With the stick centered while turning, Walk and the sidesteps are released.
static void SynthesizeRemasteredTurnActions(ItemInfo& item, PlayerInput& input, short toAngle, short minAngle)
{
	auto& player = GetLaraInfo(item);
	auto& remastered = player.Control.Remastered;

	short deltaAngle = Geometry::GetShortestAngle(item.Pose.Orientation.y, toAngle);

	input.Left = false;
	input.Right = false;

	if (abs((int)deltaAngle) >= (int)minAngle)
	{
		if (deltaAngle > 0)
		{
			input.Right = true;
		}
		else
		{
			input.Left = true;
		}
	}

	if (!(input.Left || input.Right) && remastered.IsArmedTurning)
		player.Control.TurnRate = 0;

	remastered.IsArmedTurning = (input.Left || input.Right);
	remastered.ArmedTurnErrorY = remastered.IsArmedTurning ? deltaAngle : (short)0;

	if (remastered.IsArmedTurning && player.Control.RefMoveAxis == Vector2::Zero)
	{
		input.Walk = false;
		input.StepLeft = false;
		input.StepRight = false;
	}
}

// Idle with weapons ready and the camera direction exactly behind the player: Walk and Roll are written so the stock
// 180 turn plays instead of a long armed turn. True while it does.
static bool HandleRemasteredArmedTurn180(ItemInfo& item, PlayerInput& input)
{
	if (item.Animation.ActiveState != LS_IDLE)
		return false;

	auto& player = GetLaraInfo(item);
	short deltaAngle = Geometry::GetShortestAngle(item.Pose.Orientation.y, player.Control.RefCameraOrient.y);
	if (deltaAngle != SHRT_MIN || !TestStateDispatch(item, LS_TURN_180))
		return false;

	ResetRemasteredArmedTurn(item);
	player.Control.TurnRate = 0;

	input.Left = false;
	input.Right = false;
	input.StepLeft = false;
	input.StepRight = false;
	input.Walk = true;
	input.Roll = true;
	return true;
}

// Rewrites the movement actions so the stick moves the player relative to the camera.
// On the ground the heading solver turns her, so a push becomes Forward, and a push toward the camera a fast 180.
static void TransformPlayerRemasteredInput(ItemInfo& item, PlayerInput& input, PlayerInput& heldWithoutClickInput)
{
	constexpr auto BACK_ARC_MIN_ANGLE	= ANGLE(135.0f);
	constexpr auto CRAWL_BACK_ARC_ANGLE = ANGLE(45.0f);
	constexpr auto ARMED_TURN_MIN_ANGLE = ANGLE(1.0f);

	auto& player = GetLaraInfo(item);

	ResetPlayerRemasteredClearedLatches(item);
	UpdatePlayerRemasteredSprintToggle(item, input);
	UpdateRemasteredLookToggle(item, input, heldWithoutClickInput);
	UpdatePlayerRemasteredCrouchStages(item, input);
	player.Control.RefMoveAxis = GetMoveAxis();
	UpdatePlayerRemasteredJumpRollLatch(item);

	// Water: the solver turns the player, so the turn keys, side strokes and back stroke are dropped.
	// The stick is held as Forward and, underwater, also as Jump, the stroke; there the Jump key alone no longer swims,
	// but a script push of Jump still does. At the surface Jump is left alone, where a Jump press is the dive.
	float waterRate = 0.0f;
	short waterStepMax = 0;
	bool isUnderwater = false;
	if (GetPlayerRemasteredWaterTurnParams(item, waterRate, waterStepMax, isUnderwater))
	{
		bool isPushingStick = (player.Control.RefMoveAxis != Vector2::Zero);

		input.Left = false;
		input.Right = false;
		input.StepLeft = false;
		input.StepRight = false;
		input.Back = false;
		input.Forward = isPushingStick;

		// A script clear of Forward stops the stroke. Jump is written as held, never clicked, so a resurface in the same tick does not dive.
		if (isUnderwater)
		{
			input.Jump = ((isPushingStick && !IsCleared(In::Forward)) || IsPushed(In::Jump));
			heldWithoutClickInput.Jump = true;
		}

		return;
	}

	// Hanging states: the grip holds until Roll, or until the grab button is released when the grab toggle is off.
	// Roll is consumed so it does not also ask for a turn, except on the rope, where it is the drop.
	if (DoesRemasteredInputOwnGrip(item))
	{
		bool isFireCleared = (IsCleared(In::Fire) && !IsPushed(In::Action));
		input.Action = (!isFireCleared && GetRemasteredGrip(IsRemasteredGrabHeld(), input.Roll));

		if (item.Animation.ActiveState != LS_ROPE_UP)
			input.Roll = false;

		if (input.Forward && input.Back)
			input.Back = false;
	}

	if (!IsPlayerRemasteredDirectionPushed() && !IsPlayerInRemasteredCombatBackStance(item) &&
		(item.Animation.ActiveState == LS_WALK_BACK || item.Animation.ActiveState == LS_RUN_BACK))
	{
		input.Back = false;
	}

	if (IsPlayerRemasteredDirectionPushed())
	{
		ResetRemasteredArmedTurn(item);
		return;
	}

	// Stick centered: with weapons ready and no target the player turns to face the camera direction, otherwise she stops turning.
	if (player.Control.RefMoveAxis == Vector2::Zero)
	{
		float armedRate = 0.0f;
		short armedStepMax = 0;
		if (!input.Look && !input.Jump && player.TargetEntity == nullptr &&
			(Camera.type == CameraType::Chase || Camera.type == CameraType::Combat) &&
			GetPlayerRemasteredTurnParams(item, armedRate, armedStepMax) == RemasteredInputScope::ArmedSynthesis)
		{
			if (!HandleRemasteredArmedTurn180(item, input))
				SynthesizeRemasteredTurnActions(item, input, player.Control.RefCameraOrient.y, ARMED_TURN_MIN_ANGLE);
		}
		else
		{
			ResetRemasteredArmedTurn(item);
			player.Control.TurnRate = 0;
		}

		return;
	}

	float turnRate = 0.0f;
	short turnStepMax = 0;
	auto scope = GetPlayerRemasteredTurnParams(item, turnRate, turnStepMax);
	if (scope == RemasteredInputScope::None)
		return;

	ResetRemasteredArmedTurn(item);

	// Mid-air: no tank turn. Back and Forward roll the player only on the roll push.
	// Otherwise Forward is held so the landing chains into a run, except in the back jump.
	if (scope == RemasteredInputScope::Airborne || scope == RemasteredInputScope::AirborneNoSolve)
	{
		bool isRollPushed = IsPlayerRemasteredJumpRollPushed(item, input);
		bool isBackJump = (item.Animation.ActiveState == LS_JUMP_BACK);

		if (isRollPushed)
			player.Control.Remastered.BackResolution = RemasteredBackResolution::Roll;

		input.Left = false;
		input.Right = false;
		input.Back = (isRollPushed && !isBackJump);
		input.Forward = (isRollPushed || !isBackJump);
		return;
	}

	auto dir = player.Control.RefMoveAxis;
	dir.Normalize();
	short moveAxisAngle = FROM_RAD(std::atan2(dir.x, dir.y));
	bool isPushingBack = (abs((int)moveAxisAngle) >= BACK_ARC_MIN_ANGLE);

	// Walk with a push toward the camera on knees, facing away from it: the stock crawl back plays.
	if (input.Walk && isPushingBack &&
		(item.Animation.ActiveState == LS_CRAWL_IDLE ||
		 item.Animation.ActiveState == LS_CRAWL_FORWARD ||
		 item.Animation.ActiveState == LS_CRAWL_BACK))
	{
		short frameDeltaAngle = Geometry::GetShortestAngle(item.Pose.Orientation.y, player.Control.RefCameraOrient.y);
		if (abs((int)frameDeltaAngle) <= CRAWL_BACK_ARC_ANGLE)
		{
			player.Control.Remastered.BackResolution = RemasteredBackResolution::TankBack;
			input.Left = false;
			input.Right = false;
			return;
		}
	}

	// Weapons drawn, standing: a pure push toward the camera steps the player back without turning her.
	bool isBackAlone = (input.Back && !input.Left && !input.Right);
	if (isBackAlone && IsPlayerInRemasteredCombatBackStance(item) && IsRemasteredStandingState(item))
	{
		player.Control.Remastered.BackResolution = RemasteredBackResolution::TankBack;
		input.Forward = false;
		input.Left = false;
		input.Right = false;
		return;
	}

	// Weapons ready, standing: the turn keys turn the player toward the stick, and Forward is pressed so she moves.
	if (scope == RemasteredInputScope::ArmedSynthesis)
	{
		input.Left = false;
		input.Right = false;

		if (input.Look)
			return;

		if (input.Jump)
			return;

		SynthesizeRemasteredTurnActions(item, input, GetPlayerHeadingAngleY(item), ARMED_TURN_MIN_ANGLE);
		input.Forward = true;
		input.Back = false;
		return;
	}

	input.Left = false;
	input.Right = false;
	input.StepLeft = false;
	input.StepRight = false;

	if (isPushingBack)
		player.Control.Remastered.BackResolution = RemasteredBackResolution::Turn180;

	input.Back = false;
	input.Forward = true;
}

void ApplyRemasteredInput(ItemInfo& item)
{
	auto& player = GetLaraInfo(item);

	// Under Classic controls all Remastered state is cleared, so the toggles are off on a switch back.
	if (!g_Configuration.IsUsingRemasteredControls())
	{
		player.Control.Remastered = {};
		return;
	}

	player.Control.Remastered.BackResolution = RemasteredBackResolution::None;
	player.Control.Remastered.IsCrouchStageUp = false;

	if (player.Context.Vehicle != NO_VALUE)
		return;

	auto input = ReadPlayerInput();
	auto heldWithoutClickInput = PlayerInput{};
	TransformPlayerRemasteredInput(item, input, heldWithoutClickInput);
	MaskRemasteredClearedInput(item, input);
	WritePlayerInput(input, heldWithoutClickInput);
}

// Turns the player toward the lip during the safety grab reach. The turn steps from its own last yaw,
// so a side deflection added by the stock reach code in between is discarded. True while it turns.
static bool HandleRemasteredSafetyGrabTurn(ItemInfo& item)
{
	constexpr auto TURN_RATE	 = 1.0f;
	constexpr auto TURN_STEP_MAX = ANGLE(60.0f);

	auto& player = GetLaraInfo(item);
	auto& remastered = player.Control.Remastered;

	if (!remastered.IsSafetyGrabTurning)
		return false;

	if (!item.Animation.IsAirborne ||
		(item.Animation.ActiveState != LS_REACH && item.Animation.ActiveState != LS_FREEFALL))
	{
		remastered.IsSafetyGrabTurning = false;
		return false;
	}

	item.Pose.Orientation.y = ApproachHeadingAngle(remastered.SafetyGrabTurnYaw, remastered.SafetyGrabTurnAngle, TURN_RATE, TURN_STEP_MAX, remastered.HeadingTurnSign);
	remastered.SafetyGrabTurnYaw = item.Pose.Orientation.y;

	player.Control.MoveAngle = item.Pose.Orientation.y;
	player.Control.TurnRate = 0;

	if (item.Pose.Orientation.y == remastered.SafetyGrabTurnAngle)
		remastered.IsSafetyGrabTurning = false;

	return true;
}

// Turns the player toward the heading the stick points to, at the turn rate of her current state.
void HandlePlayerRemasteredHeading(ItemInfo& item)
{
	constexpr auto TURN_RATE_180	 = 1.0f;
	constexpr auto TURN_STEP_MAX_180 = ANGLE(90.0f);

	if (!g_Configuration.IsUsingRemasteredControls())
		return;

	auto& player = GetLaraInfo(item);

	if (HandleRemasteredSafetyGrabTurn(item))
		return;

	if (!DoesPlayerRemasteredSolverOwnHeading(item))
		return;

	player.Control.TurnRate = 0;

	// No turn on a tank back step or on a mid-air roll push.
	if (player.Control.Remastered.BackResolution == RemasteredBackResolution::TankBack ||
		player.Control.Remastered.BackResolution == RemasteredBackResolution::Roll)
	{
		return;
	}

	if (player.Control.IsMoving)
		return;

	if (player.Control.RefMoveAxis == Vector2::Zero)
		return;

	float rate = 0.0f;
	short stepMax = 0;
	GetPlayerRemasteredTurnParams(item, rate, stepMax);

	if (player.Control.Remastered.BackResolution == RemasteredBackResolution::Turn180)
	{
		rate = TURN_RATE_180;
		stepMax = TURN_STEP_MAX_180;
	}

	if (IsHeld(In::Look))
		return;

	short headingAngle = GetPlayerHeadingAngleY(item);
	item.Pose.Orientation.y = ApproachHeadingAngle(item.Pose.Orientation.y, headingAngle, rate, stepMax, player.Control.Remastered.HeadingTurnSign);
}

// Water: turns the player toward the heading the stick points to and, underwater, pitches her toward the camera elevation.
// Zeroes the classic turn rate in the water states it steers. Stands down while she aligns, looks, or the stick is centered.
// Underwater it also holds the roll at zero.
void HandleRemasteredWaterHeading(ItemInfo& item)
{
	constexpr auto PITCH_RATE		= 1.0f;
	constexpr auto PITCH_STEP_MAX	= ANGLE(11.25f);

	if (!g_Configuration.IsUsingRemasteredControls())
		return;

	auto& player = GetLaraInfo(item);

	float rate = 0.0f;
	short stepMax = 0;
	bool isUnderwater = false;
	if (!GetPlayerRemasteredWaterTurnParams(item, rate, stepMax, isUnderwater))
		return;

	player.Control.TurnRate = 0;

	if (player.Control.IsMoving)
		return;

	if (IsHeld(In::Look))
		return;

	if (player.Control.RefMoveAxis == Vector2::Zero)
		return;

	auto& remastered = player.Control.Remastered;
	short headingAngle = GetPlayerHeadingAngleY(item);
	item.Pose.Orientation.y = ApproachHeadingAngle(item.Pose.Orientation.y, headingAngle, rate, stepMax, remastered.HeadingTurnSign);

	// Surface swimming stays level.
	if (!isUnderwater)
		return;

	// A positive camera elevation puts the camera below the player, looking up, so she pitches up.
	int pitchTurnSign = 0;
	item.Pose.Orientation.x = ApproachHeadingAngle(item.Pose.Orientation.x, player.Control.RefCameraOrient.x, PITCH_RATE, PITCH_STEP_MAX, pitchTurnSign);
	item.Pose.Orientation.z = 0;
}

// Remastered controls: turn states whose turn keys turn the player at the full rate, since the stick's x axis is camera-relative. -- divezzi, 12.10.26
static bool IsRemasteredUnscaledTurnState(const ItemInfo& item)
{
	if (!g_Configuration.IsUsingRemasteredControls())
		return false;

	switch (item.Animation.ActiveState)
	{
	case LS_TURN_LEFT_SLOW:
	case LS_TURN_RIGHT_SLOW:
	case LS_TURN_LEFT_FAST:
	case LS_TURN_RIGHT_FAST:
		return true;
	}

	return false;
}

bool GetRemasteredUnscaledTurnAxisCoeff(const ItemInfo& item, float& axisCoeff)
{
	const auto& player = GetLaraInfo(item);

	if (!IsRemasteredUnscaledTurnState(item) ||
		!(player.Control.Remastered.IsArmedTurning || IsHeld(In::Left) || IsHeld(In::Right)))
	{
		return false;
	}

	bool isTurningLeft = (item.Animation.ActiveState == LS_TURN_LEFT_SLOW ||
						  item.Animation.ActiveState == LS_TURN_LEFT_FAST);
	axisCoeff = isTurningLeft ? -1.0f : 1.0f;
	return true;
}

// Remastered controls: the armed turn does not overshoot the heading it turns to. -- divezzi, 12.10.26
void ClampRemasteredArmedTurnRate(ItemInfo& item)
{
	auto& player = GetLaraInfo(item);
	if (!player.Control.Remastered.IsArmedTurning)
		return;

	// NOTE: In int, so an error of exactly 180 degrees does not wrap to -180.
	int errorMax = abs((int)player.Control.Remastered.ArmedTurnErrorY);
	player.Control.TurnRate = (short)std::clamp((int)player.Control.TurnRate, -errorMax, errorMax);
}

// Remastered controls: after the crawl grab's turn, the player hangs from the edge she now faces. -- divezzi, 11.10.26
bool HandleRemasteredCrawlSafetyGrabHang(ItemInfo& item, CollisionInfo& coll)
{
	auto& player = GetLaraInfo(item);
	if (!player.Control.Remastered.IsCrawlSafetyGrabTurning)
		return false;

	player.Control.Remastered.IsCrawlSafetyGrabTurning = false;

	if (!TestLaraCrawlToHang(&item, &coll))
		return false;

	item.Animation.TargetState = LS_CRAWL_TO_HANG;
	DoLaraCrawlToHangSnap(&item, &coll);
	return true;
}

// Remastered controls: a crouch press on knees rises to the crouch without drawing a weapon. -- divezzi, 10.10.26
bool HandleRemasteredCrouchStageUp(ItemInfo& item)
{
	auto& player = GetLaraInfo(item);
	if (!player.Control.Remastered.IsCrouchStageUp || !TestStateDispatch(item, LS_CROUCH_IDLE))
		return false;

	item.Animation.TargetState = LS_CROUCH_IDLE;
	player.Control.HandStatus = HandStatus::Free;
	return true;
}

// Remastered controls: at a crawlspace edge with nothing to vault, the grab button turns the player around to hang from it. -- divezzi, 11.10.26
bool HandleRemasteredCrawlSafetyGrabTurn(ItemInfo& item, CollisionInfo& coll)
{
	if (!IsRemasteredGrabHeld() || !TestLaraCrawlToHang(&item, &coll, ANGLE(0.0f)))
		return false;

	auto& player = GetLaraInfo(item);
	player.Control.Remastered.IsCrawlSafetyGrabTurning = true;
	item.Animation.TargetState = LS_CRAWL_TURN_180;
	return true;
}

// Remastered controls: the weapon draw runs twice per tick, except while the body animation blends. -- divezzi, 12.10.26
bool NeedsSecondWeaponPass(const ItemInfo& laraItem, const LaraInfo& player)
{
	return (g_Configuration.IsUsingRemasteredControls() &&
			player.Control.HandStatus == HandStatus::WeaponDraw &&
			!laraItem.Animation.Blend.IsEnabled());
}

bool IsRemasteredDrawHeld(const LaraInfo& player)
{
	return (IsHeld(In::Draw) || player.Control.Remastered.IsDrawToggled);
}

void UpdateRemasteredDrawToggle(LaraInfo& player)
{
	auto& remastered = player.Control.Remastered;
	if (!remastered.IsDrawToggled)
		return;

	if (IsHeld(In::Draw) ||
		(player.Control.HandStatus == HandStatus::Free &&
		 player.Control.Weapon.RequestGunType == player.Control.Weapon.GunType))
	{
		remastered.IsDrawToggled = false;
	}
}

// Remastered controls: a script draw keeps the weapons out until the player holds Draw. -- divezzi, 12.10.26
void SetRemasteredDrawToggle(ItemInfo& laraItem)
{
	if (!g_Configuration.IsUsingRemasteredControls())
		return;

	auto& player = GetLaraInfo(laraItem);
	player.Control.Remastered.IsDrawToggled = true;
}

void SetRemasteredHolsters(LaraInfo& player, LaraWeaponType weaponType)
{
	auto slot = GetWeaponHolsterSlot(weaponType);
	if (slot == HolsterSlot::Empty || !player.Weapons[(int)weaponType].Present)
		return;

	auto& holsterInfo = player.Control.Weapon.HolsterInfo;
	switch (weaponType)
	{
	case LaraWeaponType::Pistol:
	case LaraWeaponType::Uzi:
		holsterInfo.LeftHolster = slot;
		holsterInfo.RightHolster = slot;
		break;

	case LaraWeaponType::Revolver:
		holsterInfo.RightHolster = slot;
		break;

	default:
		holsterInfo.BackHolster = slot;
		break;
	}
}
