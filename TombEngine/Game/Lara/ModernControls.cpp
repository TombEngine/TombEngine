#include "framework.h"
#include "Game/Lara/ModernControls.h"

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
#include "Game/spotcam.h"
#include "Math/Math.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Scripting/Include/ScriptInterfaceLevel.h"
#include "Specific/configuration.h"
#include "Specific/Input/Input.h"

using namespace TEN::Collision::Floordata;
using namespace TEN::Collision::Point;
using namespace TEN::Input;
using namespace TEN::Math;
using namespace TEN::SpotCam;

namespace TEN::Controls::Modern
{
	// Remastered controls: input rewrite and heading solver for camera-relative movement. -- divezzi, 10.10.26

	enum class InputScope
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

	// Settings of the camera steering in underwater vehicles.
	struct VehicleHeadingSetup
	{
		float YawRate		  = 0.0f; // Share of the remaining yaw error turned per tick.
		float PitchRate		  = 0.0f; // Share of the remaining pitch error turned per tick.
		short DiveCameraPitch = 0;	  // Camera pitch below which a vehicle at the surface dives.
	};

	constexpr auto VEHICLE_HEADING_SETUP = VehicleHeadingSetup{ 1.0f, 1.0f, ANGLE(-20.0f) };

	// Control mode set by a running script. Kept across levels and in savegames, and cleared with the game variables.
	static auto RunningScriptControlMode = ScriptControlMode::Default;

	// Grab and weapon button: Fire under Remastered controls, Action under Classic controls.
	// A script clear of Action holds it off, and a script push of Action presses it.
	bool IsGrabHeld()
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
	bool IsVehicleFireHeld()
	{
		if (!g_Configuration.IsUsingRemasteredControls())
			return IsHeld(In::Fire);

		if (IsCleared(In::Fire))
			return false;

		if (IsPushed(In::Fire))
			return true;

		return IsHeld(In::Action);
	}

	// Surface dive in a vehicle: Forward under Classic controls; under Remastered controls, the camera pointed down past
	// the dive pitch. A script clear of Forward holds it off, and a script push of Forward presses it.
	bool IsVehicleDiveHeld(const LaraInfo& player)
	{
		if (!g_Configuration.IsUsingRemasteredControls())
			return IsHeld(In::Forward);

		if (IsCleared(In::Forward))
			return false;

		if (IsPushed(In::Forward))
			return true;

		// Under fixed and flyby cameras, the right stick or mouse turned down.
		auto viewTurn = Vector2i::Zero;
		if (GetViewFrameTurn(viewTurn))
			return (viewTurn.y < 0);

		return (player.Control.RefCameraOrient.x < VEHICLE_HEADING_SETUP.DiveCameraPitch);
	}

	// Jeep reverse gear shift: a Sprint press, or under Remastered controls also Reverse held without Accelerate.
	bool IsVehicleReverseGearShift()
	{
		if (!g_Configuration.IsUsingRemasteredControls())
			return IsClicked(In::Sprint);

		if (IsCleared(In::Sprint))
			return false;

		return (IsClicked(In::Sprint) || (IsHeld(In::Reverse) && !IsHeld(In::Accelerate)));
	}

	// Jeep forward gear shift: a Walk press, or under Remastered controls also Accelerate held without Reverse.
	bool IsVehicleForwardGearShift()
	{
		if (!g_Configuration.IsUsingRemasteredControls())
			return IsClicked(In::Walk);

		if (IsCleared(In::Walk))
			return false;

		return (IsClicked(In::Walk) || (IsHeld(In::Accelerate) && !IsHeld(In::Reverse)));
	}

	// Jeep throttle: Accelerate, or under Remastered controls Reverse in the reverse gear.
	bool IsVehicleThrottleHeld(bool isReverseGear)
	{
		if (!g_Configuration.IsUsingRemasteredControls() || !isReverseGear)
			return IsHeld(In::Accelerate);

		if (IsCleared(In::Accelerate))
			return false;

		return IsHeld(In::Reverse);
	}

	// Jeep brake: Brake, or under Remastered controls also the other gear's pedal held alone.
	bool IsVehicleBrakeHeld(bool isReverseGear)
	{
		if (!g_Configuration.IsUsingRemasteredControls())
			return IsHeld(In::Brake);

		if (IsCleared(In::Brake))
			return false;

		auto gearActionID = isReverseGear ? In::Reverse : In::Accelerate;
		auto otherActionID = isReverseGear ? In::Accelerate : In::Reverse;
		return (IsHeld(In::Brake) || (IsHeld(otherActionID) && !IsHeld(gearActionID)));
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
	static bool IsWeaponReady(const ItemInfo& item)
	{
		const auto& player = GetLaraInfo(item);
		return (player.Control.HandStatus == HandStatus::WeaponReady &&
				player.Control.Weapon.GunType != LaraWeaponType::Torch);
	}

	// A lasersight weapon drawn and ready.
	static bool IsLasersightReady(const LaraInfo& player)
	{
		return (player.Control.HandStatus == HandStatus::WeaponReady &&
				player.Weapons[(int)player.Control.Weapon.GunType].HasLasersight);
	}

	// Weapons drawn or being drawn. A torch counts as holstered.
	static bool IsInCombatBackStance(const ItemInfo& item)
	{
		const auto& player = GetLaraInfo(item);
		return ((player.Control.HandStatus == HandStatus::WeaponReady ||
				 player.Control.HandStatus == HandStatus::WeaponDraw) &&
				player.Control.Weapon.GunType != LaraWeaponType::Torch);
	}

	static bool IsStandingState(const ItemInfo& item)
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
	static bool DoesInputOwnGrip(const ItemInfo& item)
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
	static bool GetGrip(bool isGrabHeld, bool isRollHeld)
	{
		if (isRollHeld)
			return false;

		return (g_Configuration.EnableGrabToggle || isGrabHeld);
	}

	static bool IsOnKnees(const ItemInfo& item)
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

	static bool IsCrouchOrCrawlState(const ItemInfo& item)
	{
		return (TestState(item.Animation.ActiveState, CROUCH_STATES) || TestState(item.Animation.ActiveState, CRAWL_STATES));
	}

	// Standing states whose stock routines take the player low while Crouch is held.
	static bool IsCrouchEntryState(const ItemInfo& item)
	{
		switch (item.Animation.ActiveState)
		{
		case LS_IDLE:
		case LS_RUN_FORWARD:
		case LS_TURN_LEFT_SLOW:
		case LS_TURN_RIGHT_SLOW:
		case LS_TURN_LEFT_FAST:
		case LS_TURN_RIGHT_FAST:
		case LS_SPRINT:
		case LS_SPRINT_SLIDE:
			return true;

		default:
			return false;
		}
	}

	// Walk on knees plays the stock tank crawl.
	static bool IsCrawlWalkHeld()
	{
		return IsRawActionHeld(In::Walk);
	}

	// A script pushed a direction this frame. That tick plays classic.
	static bool IsDirectionPushed()
	{
		return (IsPushed(In::Forward) || IsPushed(In::Back) || IsPushed(In::Left) || IsPushed(In::Right));
	}

	// Turn rate and step cap of the heading solver in the player's current state.
	// Scope None leaves the state to play classic.
	static InputScope GetTurnParams(const ItemInfo& item, float& rate, short& stepMax)
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

		if (IsDirectionPushed())
			return InputScope::None;

		// Optics: the stick moves the view, and the player keeps her heading.
		const auto& player = GetLaraInfo(item);
		if (player.Control.Look.IsUsingBinoculars || player.Control.Look.IsUsingLasersight)
			return InputScope::None;

		if (IsWeaponReady(item))
		{
			switch (item.Animation.ActiveState)
			{
			case LS_IDLE:
			case LS_SOFT_SPLAT:
			case LS_TURN_LEFT_SLOW:
			case LS_TURN_RIGHT_SLOW:
			case LS_TURN_LEFT_FAST:
			case LS_TURN_RIGHT_FAST:
				return InputScope::ArmedSynthesis;
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
			return InputScope::Ground;

		case LS_WALK_FORWARD:
			rate = TURN_RATE_WALK;
			stepMax = TURN_STEP_MAX_WALK;
			return InputScope::Ground;

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

			return InputScope::Ground;

		case LS_SPRINT:
			rate = TURN_RATE_SPRINT;
			stepMax = TURN_STEP_MAX_SPRINT;
			return InputScope::Ground;

		case LS_WADE_FORWARD:
			rate = TURN_RATE_WADE;
			stepMax = TURN_STEP_MAX_WADE;
			return InputScope::Ground;

		case LS_STEP_LEFT:
		case LS_STEP_RIGHT:
			rate = TURN_RATE_IDLE;
			stepMax = TURN_STEP_MAX_IDLE;
			return InputScope::Ground;

		case LS_CRAWL_IDLE:
		case LS_CRAWL_TURN_LEFT:
		case LS_CRAWL_TURN_RIGHT:
			if (IsCrawlWalkHeld())
				return InputScope::None;

			rate = TURN_RATE_CRAWL_IDLE;
			stepMax = TURN_STEP_MAX_CRAWL_IDLE;
			return InputScope::Ground;

		case LS_CRAWL_FORWARD:
		case LS_CRAWL_BACK:
			if (IsCrawlWalkHeld())
				return InputScope::None;

			rate = TURN_RATE_CRAWL;
			stepMax = TURN_STEP_MAX_CRAWL;
			return InputScope::Ground;

		case LS_MONKEY_IDLE:
		case LS_MONKEY_FORWARD:
		case LS_MONKEY_BACK:
		case LS_MONKEY_TURN_LEFT:
		case LS_MONKEY_TURN_RIGHT:
		case LS_MONKEY_SHIMMY_LEFT:
		case LS_MONKEY_SHIMMY_RIGHT:
			rate = TURN_RATE_MONKEY;
			stepMax = TURN_STEP_MAX_MONKEY;
			return InputScope::Ground;

		// Jump wind-up. Holstered, she turns toward the stick before the launch. Armed, it plays classic.
		case LS_JUMP_PREPARE:
			if (IsWeaponReady(item))
				return InputScope::None;

			rate = TURN_RATE_JUMP_PREPARE;
			stepMax = TURN_STEP_MAX_JUMP_PREPARE;
			return InputScope::Ground;

		// The back jump keeps its launch heading, armed or not, so its roll push is read the same way.
		case LS_JUMP_BACK:
			return InputScope::AirborneNoSolve;

		case LS_JUMP_LEFT:
		case LS_JUMP_RIGHT:
			if (IsWeaponReady(item))
				return InputScope::None;

			return InputScope::AirborneNoSolve;

		// The forward jump is the only jump the solver steers mid-air.
		case LS_JUMP_FORWARD:
			rate = TURN_RATE_AIRBORNE;
			stepMax = TURN_STEP_MAX_AIRBORNE;
			return InputScope::Airborne;

		case LS_JUMP_UP:
		case LS_FREEFALL:
		case LS_REACH:
		case LS_SWAN_DIVE:
			return InputScope::AirborneNoSolve;

		default:
			return InputScope::None;
		}
	}

	// True if the heading solver turns the player in the current state.
	bool DoesSolverOwnHeading(const ItemInfo& item)
	{
		float rate = 0.0f;
		short stepMax = 0;
		auto scope = GetTurnParams(item, rate, stepMax);
		return (scope == InputScope::Ground || scope == InputScope::Airborne);
	}

	// True if the camera turns toward the player's movement. Ground states only.
	bool DoesCameraAssistApply(const ItemInfo& item)
	{
		float rate = 0.0f;
		short stepMax = 0;
		return (GetTurnParams(item, rate, stepMax) == InputScope::Ground);
	}

	// Turn rate and step cap of the heading solver in the water states it steers. False in any other state,
	// on a script direction push, and with the divesuit, which keeps its own pitch control.
	// isUnderwater is set for the states that also follow the camera pitch and swim on Jump: underwater and the fly cheat.
	static bool GetWaterTurnParams(const ItemInfo& item, float& rate, short& stepMax, bool& isUnderwater)
	{
		constexpr auto TURN_RATE_UNDERWATER = 1.0f;
		constexpr auto TURN_RATE_SURFACE	= 0.5f;
		constexpr auto TURN_STEP_MAX		= ANGLE(179.0f); // Not reached: a step is at most 57 degrees at rate 1.0.

		isUnderwater = false;

		if (IsDirectionPushed())
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
	bool DoesSolverOwnWaterHeading(const ItemInfo& item)
	{
		if (!g_Configuration.IsUsingRemasteredControls())
			return false;

		float rate = 0.0f;
		short stepMax = 0;
		bool isUnderwater = false;
		return GetWaterTurnParams(item, rate, stepMax, isUnderwater);
	}

	// Heading a stick direction points to in the camera frame.
	static short GetCameraRelativeAngleY(const LaraInfo& player, Vector2 dir)
	{
		dir.Normalize();
		short moveAxisAngle = FROM_RAD(std::atan2(dir.x, dir.y));
		return (short)(player.Control.RefCameraOrient.y + moveAxisAngle);
	}

	// Right stick and mouse pitch step under Remastered controls with a fixed or flyby camera. False otherwise.
	static bool GetViewPitchStep(short& pitchStep)
	{
		constexpr auto PITCH_STEP_MAX = ANGLE(6.0f); // Twice the stock swim pitch rate.

		pitchStep = 0;

		auto turn = Vector2i::Zero;
		if (!GetViewFrameTurn(turn))
			return false;

		// Without inversion, stick up or mouse forward pitches up.
		pitchStep = (short)std::clamp(turn.y, -(int)PITCH_STEP_MAX, (int)PITCH_STEP_MAX);
		return true;
	}

	// Pitch step that lifts the player off the floor she swims into, up to a little above level.
	static short GetFloorLiftPitch(const ItemInfo& item)
	{
		constexpr auto LIFT_STEP	  = ANGLE(3.0f);
		constexpr auto LIFT_PITCH_MAX = ANGLE(5.0f);

		if (LaraCollision.Middle.Floor == NO_HEIGHT || LaraCollision.Middle.Floor >= 0)
			return 0;

		return (short)std::clamp(LIFT_PITCH_MAX - item.Pose.Orientation.x, 0, (int)LIFT_STEP);
	}

	// Heading the stick points to, relative to the camera. The player's own heading when the stick is centered.
	short GetPlayerHeadingAngleY(const ItemInfo& item)
	{
		if (!g_Configuration.IsUsingRemasteredControls())
			return item.Pose.Orientation.y;

		const auto& player = GetLaraInfo(item);
		if (player.Control.RefMoveAxis == Vector2::Zero)
			return item.Pose.Orientation.y;

		return GetCameraRelativeAngleY(player, player.Control.RefMoveAxis);
	}

	// A diagonal step or wall met within 45 degrees of square-on resets the side collision, so the player
	// moves on until the front probe stops her instead of being deflected every tick. Returns true if reset.
	bool HandleDiagonalStep(const ItemInfo& item, CollisionInfo& coll)
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
	static bool GetDiagonalLip(const ItemInfo& item, short& lipAngle, float& lipDistance)
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
	static bool TestSafetyGrabDiagonal(ItemInfo& item, const CollisionInfo& probe, short& turnAngle)
	{
		short lipAngle = 0;
		float lipDistance = 0.0f;
		if (!GetDiagonalLip(item, lipAngle, lipDistance))
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
	static void ApplySafetyGrabStandOff(ItemInfo& item)
	{
		constexpr auto STAND_OFF_DISTANCE = LARA_RADIUS * 1.2f;

		short lipAngle = 0;
		float lipDistance = 0.0f;
		if (!GetDiagonalLip(item, lipAngle, lipDistance))
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
	bool TestSafetyGrab(ItemInfo& item, CollisionInfo& coll)
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

		if (!IsGrabHeld())
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
		if (isDiagonalLip && !TestSafetyGrabDiagonal(item, probe, turnAngle))
			return false;

		int waterHeight = GetPointCollision(item).GetWaterSurfaceHeight();
		if (waterHeight != NO_HEIGHT && (waterHeight - item.Pose.Position.y) < WATER_DEPTH_MIN)
			return false;

		player.Control.RefMoveAxis = Vector2::Zero;
		player.Control.Modern.SafetyGrabTurnAngle = turnAngle;
		player.Control.Modern.SafetyGrabTurnYaw = item.Pose.Orientation.y;
		player.Control.Modern.IsSafetyGrabTurning = true;

		if (isDiagonalLip)
			ApplySafetyGrabStandOff(item);

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
	bool HandleTightropeIdle(ItemInfo& item)
	{
		auto& modern = GetLaraInfo(item).Control.Modern;

		if (IsHeld(In::Forward))
		{
			modern.IsTightropeTurnLatched = false;
			return false;
		}

		if (IsHeld(In::Back) && !IsHeld(In::Roll))
		{
			if (modern.IsTightropeTurnLatched)
			{
				item.Animation.TargetState = LS_TIGHTROPE_WALK;
				return true;
			}

			modern.IsTightropeTurnLatched = true;
			item.Animation.TargetState = LS_TIGHTROPE_TURN_180;
			return true;
		}

		if (!IsHeld(In::Back))
			modern.IsTightropeTurnLatched = false;

		return false;
	}

	// A script clear of Sprint, Look or Crouch resets its toggle.
	static void ResetClearedLatches(ItemInfo& item)
	{
		auto& player = GetLaraInfo(item);

		if (IsCleared(In::Sprint))
			player.Control.Modern.IsSprintToggled = false;

		if (IsCleared(In::Look))
			player.Control.Modern.IsLookToggled = false;

		if (IsCleared(In::Crouch))
		{
			player.Control.Modern.CrouchStage = 0;
			player.Control.Modern.IsCrouchStageUpPending = false;
		}
	}

	static void ResetArmedTurn(ItemInfo& item)
	{
		auto& modern = GetLaraInfo(item).Control.Modern;
		modern.IsArmedTurning = false;
		modern.ArmedTurnErrorY = 0;
	}

	// A script clear holds the action off, even where the rewrite would set it.
	static void MaskClearedInput(ItemInfo& item, PlayerInput& input)
	{
		ApplyClearedActions(input);

		// A script clear of the turn key the armed turn pressed stops the armed turn.
		auto& player = GetLaraInfo(item);
		if ((IsCleared(In::Left) || IsCleared(In::Right)) &&
			player.Control.Modern.IsArmedTurning && !(input.Left || input.Right))
		{
			player.Control.TurnRate = 0;
			ResetArmedTurn(item);
		}
	}

	// Sprint toggle: a press while running or in the fly cheat latches the sprint.
	// The next press or leaving those states releases it.
	static void UpdateSprintToggle(ItemInfo& item, PlayerInput& input)
	{
		auto& modern = GetLaraInfo(item).Control.Modern;
		bool isSprintState = (item.Animation.ActiveState == LS_RUN_FORWARD || item.Animation.ActiveState == LS_SPRINT ||
			item.Animation.ActiveState == LS_FLY_CHEAT);

		if (!g_Configuration.EnableSprintToggle)
		{
			modern.IsSprintToggled = false;
		}
		else if (IsClicked(In::Sprint) && !IsPushed(In::Sprint))
		{
			if (modern.IsSprintToggled)
			{
				modern.IsSprintToggled = false;
			}
			else if (isSprintState)
			{
				modern.IsSprintToggled = true;
			}
		}

		if (!isSprintState)
			modern.IsSprintToggled = false;

		if (g_Configuration.EnableSprintToggle && isSprintState)
			input.Sprint = (modern.IsSprintToggled || IsPushed(In::Sprint));
	}

	// Look toggle: a press latches Look and the next press releases it. The latched Look is written held, never clicked.
	// Off while locked on a target, where each press switches target, unless a lasersight weapon is ready.
	// Off in binoculars without the sight, where a Look press closes them, and in states that cannot look.
	static void UpdateLookToggle(ItemInfo& item, PlayerInput& input, PlayerInput& heldWithoutClickInput)
	{
		auto& player = GetLaraInfo(item);
		auto& modern = player.Control.Modern;

		bool isLasersightReady = IsLasersightReady(player);
		bool isUsingBinocularsOnly = (player.Control.Look.IsUsingBinoculars && !player.Control.Look.IsUsingLasersight);

		if (!g_Configuration.EnableLookToggle || (IsPlayerLockedOnTarget(item) && !isLasersightReady) ||
			player.Control.Look.Mode == LookMode::None || isUsingBinocularsOnly)
		{
			modern.IsLookToggled = false;
			return;
		}

		if (IsClicked(In::Look) && !IsPushed(In::Look))
			modern.IsLookToggled = !modern.IsLookToggled;

		input.Look = (modern.IsLookToggled || IsPushed(In::Look));
		heldWithoutClickInput.Look = true;
	}

	// Look toggle in a vehicle, which skips the input rewrite: a press latches Look and the next press releases it.
	// A script clear of Look releases it, and so does a Brake press, which every vehicle's dismount reads, so she can get off.
	// Look-around reads the latch through IsLookAroundHeld().
	static void UpdateVehicleLookToggle(ItemInfo& item)
	{
		auto& modern = GetLaraInfo(item).Control.Modern;

		if (!g_Configuration.EnableLookToggle || IsCleared(In::Look) || IsClicked(In::Brake))
		{
			modern.IsLookToggled = false;
			return;
		}

		if (IsClicked(In::Look) && !IsPushed(In::Look))
			modern.IsLookToggled = !modern.IsLookToggled;
	}

	// Look starting this tick, by a press or the toggle, turns the orbit camera behind the player, where it stays after Look.
	// In a vehicle a press does it, also in vehicles where she cannot look.
	// Not on a target switch, with a lasersight weapon ready, in optics or in a flyby.
	static void ResetCameraOnLookStart(const ItemInfo& item, bool wasLookToggled)
	{
		const auto& player = GetLaraInfo(item);
		bool isInVehicle = (player.Context.Vehicle != NO_VALUE);

		bool isLookStarting = (g_Configuration.EnableLookToggle && !isInVehicle) ?
			(!wasLookToggled && player.Control.Modern.IsLookToggled) :
			(IsClicked(In::Look) && !IsPushed(In::Look));
		if (!isLookStarting)
			return;

		if ((player.Control.Look.Mode == LookMode::None && !isInVehicle) || IsPlayerLockedOnTarget(item) || IsLasersightReady(player) ||
			player.Control.Look.IsUsingBinoculars || player.Control.Look.IsUsingLasersight || UseSpotCam)
		{
			return;
		}

		ResetCameraOrient(item);
	}

	// Look outside the player's tick: the latched Look under the Look toggle, otherwise the Look key.
	bool IsLookHeld(const LaraInfo& player)
	{
		if (!g_Configuration.IsUsingRemasteredControls() || !g_Configuration.EnableLookToggle)
			return IsHeld(In::Look);

		return (player.Control.Modern.IsLookToggled || IsPushed(In::Look));
	}

	// Look inside the player's tick: on foot the Look action the input rewrite wrote; in a vehicle, which skips it, IsLookHeld().
	bool IsLookAroundHeld(const LaraInfo& player)
	{
		if (player.Context.Vehicle == NO_VALUE)
			return IsHeld(In::Look);

		return IsLookHeld(player);
	}

	void ResetLookToggle(LaraInfo& player)
	{
		if (!g_Configuration.IsUsingRemasteredControls())
			return;

		player.Control.Modern.IsLookToggled = false;
	}

	// Look and optics: the right stick and mouse also turn the view, on the axes the look mode allows and not on an axis
	// with a direction cleared this tick. Up aims up on every input unless the camera Y axis is inverted.
	void ApplyLookAxisCoeff(const ItemInfo& item, Vector2& axisCoeff)
	{
		// At the default sensitivity, 50 pixels of sideways mouse movement in one tick give the full turn rate.
		constexpr auto MOUSE_AXIS_COEFF = 16.0f;

		if (!g_Configuration.IsUsingRemasteredControls())
			return;

		const auto& player = GetLaraInfo(item);
		auto lookMode = player.Control.Look.Mode;

		auto viewAxis = GetCameraAxis() + (GetMouseAxis() * MOUSE_AXIS_COEFF);
		if (IsCleared(In::Forward) || IsCleared(In::Back) ||
			(lookMode != LookMode::Free && lookMode != LookMode::Vertical))
		{
			viewAxis.y = 0.0f;
		}

		if (IsCleared(In::Left) || IsCleared(In::Right) ||
			(lookMode != LookMode::Free && lookMode != LookMode::Horizontal))
		{
			viewAxis.x = 0.0f;
		}

		// NOTE: A positive x coefficient aims down. Forward on the keys and left stick is positive,
		// up on the right stick and mouse is negative.
		float upCoeff = axisCoeff.x - viewAxis.y;
		axisCoeff.x = std::clamp(g_Configuration.InvertCameraYAxis ? upCoeff : -upCoeff, -1.0f, 1.0f);
		axisCoeff.y = std::clamp(axisCoeff.y + viewAxis.x, -1.0f, 1.0f);
	}

	// Optics zoom from the mouse wheel: 1 zooms in, -1 zooms out, 0 while a zoom action is cleared this tick.
	int GetOpticsWheelZoom()
	{
		if (!g_Configuration.IsUsingRemasteredControls() ||
			IsCleared(In::StepLeft) || IsCleared(In::StepRight) || IsCleared(In::Walk) || IsCleared(In::Sprint))
		{
			return 0;
		}

		if (IsHeld(In::MouseScrollUp))
			return 1;

		if (IsHeld(In::MouseScrollDown))
			return -1;

		return 0;
	}

	// Weapon scroll from the mouse wheel: 1 selects the next weapon, -1 the previous one. 0 while the wheel serves something
	// else: the lasersight zoom, or an action the player bound to it.
	int GetWeaponWheelScroll(const LaraInfo& player)
	{
		if (!g_Configuration.IsUsingRemasteredControls() || player.Control.Look.IsUsingLasersight)
			return 0;

		for (const auto& [actionID, keyID] : g_Bindings.GetBindingProfile(BindingProfileID::Custom))
		{
			if (keyID == MK_AXIS_Z_NEG || keyID == MK_AXIS_Z_POS)
				return 0;
		}

		if (IsHeld(In::MouseScrollUp))
			return 1;

		if (IsHeld(In::MouseScrollDown))
			return -1;

		return 0;
	}

	// A toggled crouch that has taken the player low ends when she stands up by any way other than a Crouch press,
	// such as the fly cheat, a knock-down or an object moving her, so she does not crouch again on her own.
	static void UpdateCrouchToggleRelease(ItemInfo& item)
	{
		auto& modern = GetLaraInfo(item).Control.Modern;

		if (!g_Configuration.EnableCrouchToggle || modern.CrouchStage == 0)
		{
			modern.IsCrouchStageReached = false;
		}
		else if (IsCrouchOrCrawlState(item))
		{
			modern.IsCrouchStageReached = true;
		}
		else if (modern.IsCrouchStageReached && IsCrouchEntryState(item))
		{
			modern.CrouchStage = 0;
			modern.IsCrouchStageUpPending = false;
			modern.IsCrouchStageReached = false;
		}
	}

	// Crouch stages: standing, crouched, on knees. A press on knees rises one stage to the crouch.
	static void UpdateCrouchStages(ItemInfo& item, PlayerInput& input)
	{
		auto& player = GetLaraInfo(item);
		auto& modern = player.Control.Modern;

		if (!g_Configuration.EnableCrouchToggle)
		{
			modern.CrouchStage = input.Crouch ? 1 : 0;
			modern.IsCrouchStageUpPending = false;
		}
		else if (IsClicked(In::Crouch) && !IsPushed(In::Crouch))
		{
			if (modern.CrouchStage == 0)
			{
				modern.CrouchStage = 1;
			}
			else if (IsOnKnees(item))
			{
				modern.IsCrouchStageUpPending = true;
			}
			else
			{
				modern.CrouchStage = 0;
			}
		}

		UpdateCrouchToggleRelease(item);

		if (modern.IsCrouchStageUpPending)
		{
			if (IsOnKnees(item))
			{
				modern.IsCrouchStageUp = true;
				input.Forward = false;
				input.Back = false;
			}
			else
			{
				modern.IsCrouchStageUpPending = false;
			}
		}

		if (player.Control.WaterStatus == WaterStatus::Underwater ||
			player.Control.WaterStatus == WaterStatus::TreadWater ||
			DoesInputOwnGrip(item))
		{
			modern.CrouchStage = 0;
			modern.IsCrouchStageUpPending = false;
		}

		input.Crouch = ((modern.CrouchStage != 0) || IsPushed(In::Crouch));
	}

	// True if the stick points within the roll arc: the player's back in a forward jump, her front in a back jump.
	static bool IsPushInJumpRollArc(const ItemInfo& item)
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
	static bool IsJumpRollPushed(const ItemInfo& item, const PlayerInput& input)
	{
		constexpr auto PUSH_MIN = 0.7f; // About half the stick's travel. Keys and the D-pad give 1.0.

		const auto& player = GetLaraInfo(item);

		bool isForwardJump = (item.Animation.ActiveState == LS_JUMP_FORWARD);
		if (!isForwardJump && item.Animation.ActiveState != LS_JUMP_BACK)
			return false;

		if (input.Look || player.Control.Modern.IsJumpRollSpent)
			return false;

		bool isVertical = (input.Forward || input.Back);
		bool isHorizontal = (input.Left || input.Right);
		if (isVertical == isHorizontal)
			return false;

		const auto& moveAxis = player.Control.RefMoveAxis;
		if (abs(isVertical ? moveAxis.y : moveAxis.x) < PUSH_MIN)
			return false;

		return IsPushInJumpRollArc(item);
	}

	// A direction key or stick direction was pressed or released this frame.
	static bool HasDirectionChanged()
	{
		return (IsClicked(In::Forward) || IsClicked(In::Back) || IsClicked(In::Left) || IsClicked(In::Right) ||
				IsReleased(In::Forward) || IsReleased(In::Back) || IsReleased(In::Left) || IsReleased(In::Right));
	}

	// The roll is an input event. A push held when the jump starts, a push that made a roll and a push held outside the
	// roll arc are spent. Only a stick release or a direction press or release clears it; camera movement never does.
	static void UpdateJumpRollLatch(ItemInfo& item)
	{
		auto& player = GetLaraInfo(item);
		bool isPushing = (player.Control.RefMoveAxis != Vector2::Zero);

		switch (item.Animation.ActiveState)
		{
		case LS_JUMP_ROLL_180:
			player.Control.Modern.IsJumpRollSpent = true;
			break;

		case LS_JUMP_FORWARD:
		case LS_JUMP_BACK:
			if (!isPushing || HasDirectionChanged())
				player.Control.Modern.IsJumpRollSpent = false;

			if (isPushing && !IsPushInJumpRollArc(item))
				player.Control.Modern.IsJumpRollSpent = true;

			break;

		default:
			player.Control.Modern.IsJumpRollSpent = isPushing;
			break;
		}
	}

	// Presses the turn key toward a target heading while the error is at least minAngle, so the stock turn states turn
	// the player. With the stick centered while turning, Walk and the sidesteps are released.
	static void SynthesizeTurnActions(ItemInfo& item, PlayerInput& input, short toAngle, short minAngle)
	{
		auto& player = GetLaraInfo(item);
		auto& modern = player.Control.Modern;

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

		if (!(input.Left || input.Right) && modern.IsArmedTurning)
			player.Control.TurnRate = 0;

		modern.IsArmedTurning = (input.Left || input.Right);
		modern.ArmedTurnErrorY = modern.IsArmedTurning ? deltaAngle : (short)0;

		if (modern.IsArmedTurning && player.Control.RefMoveAxis == Vector2::Zero)
		{
			input.Walk = false;
			input.StepLeft = false;
			input.StepRight = false;
		}
	}

	// Idle with weapons ready and the camera direction exactly behind the player: Walk and Roll are written so the stock
	// 180 turn plays instead of a long armed turn. True while it does.
	static bool HandleArmedTurn180(ItemInfo& item, PlayerInput& input)
	{
		if (item.Animation.ActiveState != LS_IDLE)
			return false;

		auto& player = GetLaraInfo(item);
		short deltaAngle = Geometry::GetShortestAngle(item.Pose.Orientation.y, player.Control.RefCameraOrient.y);
		if (deltaAngle != SHRT_MIN || !TestStateDispatch(item, LS_TURN_180))
			return false;

		ResetArmedTurn(item);
		player.Control.TurnRate = 0;

		input.Left = false;
		input.Right = false;
		input.StepLeft = false;
		input.StepRight = false;
		input.Walk = true;
		input.Roll = true;
		return true;
	}

	// Drops movement input while the crawl grab turns the player to hang, so the solver does not turn her back.
	static bool HoldCrawlSafetyGrabInput(ItemInfo& item, PlayerInput& input)
	{
		auto& player = GetLaraInfo(item);
		if (!player.Control.Modern.IsCrawlSafetyGrabTurning)
			return false;

		if (!IsOnKnees(item))
		{
			player.Control.Modern.IsCrawlSafetyGrabTurning = false;
			return false;
		}

		player.Control.RefMoveAxis = Vector2::Zero;
		input.Forward = false;
		input.Back = false;
		input.Left = false;
		input.Right = false;
		input.StepLeft = false;
		input.StepRight = false;
		input.Sprint = false;
		input.Jump = false;
		input.Roll = false;
		return true;
	}

	// Rewrites the movement actions so the stick moves the player relative to the camera.
	// On the ground the heading solver turns her, so a push becomes Forward, and a push toward the camera a fast 180.
	static void TransformInput(ItemInfo& item, PlayerInput& input, PlayerInput& heldWithoutClickInput)
	{
		constexpr auto BACK_ARC_MIN_ANGLE	= ANGLE(135.0f);
		constexpr auto ARMED_TURN_MIN_ANGLE = ANGLE(1.0f);

		auto& player = GetLaraInfo(item);

		ResetClearedLatches(item);
		UpdateSprintToggle(item, input);

		bool wasLookToggled = player.Control.Modern.IsLookToggled;
		UpdateLookToggle(item, input, heldWithoutClickInput);
		ResetCameraOnLookStart(item, wasLookToggled);

		UpdateCrouchStages(item, input);
		player.Control.RefMoveAxis = GetHeldMoveAxis();

		// A pending rise on knees centers the stick, so the rewrite leaves Forward and Back off until the rise plays.
		if (player.Control.Modern.IsCrouchStageUp)
			player.Control.RefMoveAxis = Vector2::Zero;

		if (HoldCrawlSafetyGrabInput(item, input))
		{
			player.Control.TurnRate = 0;
			return;
		}

		UpdateJumpRollLatch(item);

		// Water: the solver turns the player, so the turn keys, side strokes and back stroke are dropped.
		// The stick is held as Forward and, underwater, also as Jump, the stroke; there the Jump key alone no longer swims,
		// but a script push of Jump still does. At the surface Jump is left alone, where a Jump press is the dive.
		float waterRate = 0.0f;
		short waterStepMax = 0;
		bool isUnderwater = false;
		if (GetWaterTurnParams(item, waterRate, waterStepMax, isUnderwater))
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
		if (DoesInputOwnGrip(item))
		{
			bool isFireCleared = (IsCleared(In::Fire) && !IsPushed(In::Action));
			input.Action = (!isFireCleared && GetGrip(IsGrabHeld(), input.Roll));

			if (item.Animation.ActiveState != LS_ROPE_UP)
				input.Roll = false;

			if (input.Forward && input.Back)
				input.Back = false;
		}

		// Forward and Back held together cancel each other, as Left and Right do. A script push of a direction keeps them.
		if (input.Forward && input.Back && !IsDirectionPushed())
		{
			input.Forward = false;
			input.Back = false;
		}

		if (!IsDirectionPushed() && !IsInCombatBackStance(item) &&
			(item.Animation.ActiveState == LS_WALK_BACK || item.Animation.ActiveState == LS_RUN_BACK))
		{
			input.Back = false;
		}

		if (IsDirectionPushed())
		{
			ResetArmedTurn(item);
			return;
		}

		// Stick centered: with weapons ready and no target the player turns to face the camera direction, otherwise she stops turning.
		if (player.Control.RefMoveAxis == Vector2::Zero)
		{
			float armedRate = 0.0f;
			short armedStepMax = 0;
			if (!input.Look && !input.Jump && player.TargetEntity == nullptr &&
				(Camera.type == CameraType::Chase || Camera.type == CameraType::Combat) &&
				GetTurnParams(item, armedRate, armedStepMax) == InputScope::ArmedSynthesis)
			{
				if (!HandleArmedTurn180(item, input))
					SynthesizeTurnActions(item, input, player.Control.RefCameraOrient.y, ARMED_TURN_MIN_ANGLE);
			}
			else
			{
				ResetArmedTurn(item);
				player.Control.TurnRate = 0;
			}

			return;
		}

		float turnRate = 0.0f;
		short turnStepMax = 0;
		auto scope = GetTurnParams(item, turnRate, turnStepMax);
		if (scope == InputScope::None)
			return;

		ResetArmedTurn(item);

		// Mid-air: no tank turn. Back and Forward roll the player only on the roll push.
		// Otherwise Forward is held so the landing chains into a run, except in the back jump.
		if (scope == InputScope::Airborne || scope == InputScope::AirborneNoSolve)
		{
			bool isRollPushed = IsJumpRollPushed(item, input);
			bool isBackJump = (item.Animation.ActiveState == LS_JUMP_BACK);

			if (isRollPushed)
				player.Control.Modern.BackResolution = ModernBackResolution::Roll;

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

		// Weapons drawn, standing: a pure push toward the camera steps the player back without turning her.
		bool isBackAlone = (input.Back && !input.Left && !input.Right);
		if (isBackAlone && IsInCombatBackStance(item) && IsStandingState(item))
		{
			player.Control.Modern.BackResolution = ModernBackResolution::TankBack;
			input.Forward = false;
			input.Left = false;
			input.Right = false;
			return;
		}

		// Weapons ready, standing: the turn keys turn the player toward the stick, and Forward is pressed so she moves.
		if (scope == InputScope::ArmedSynthesis)
		{
			input.Left = false;
			input.Right = false;

			if (input.Look)
				return;

			if (input.Jump)
				return;

			SynthesizeTurnActions(item, input, GetPlayerHeadingAngleY(item), ARMED_TURN_MIN_ANGLE);
			input.Forward = true;
			input.Back = false;
			return;
		}

		input.Left = false;
		input.Right = false;
		input.StepLeft = false;
		input.StepRight = false;

		if (isPushingBack)
			player.Control.Modern.BackResolution = ModernBackResolution::Turn180;

		input.Back = false;
		input.Forward = true;
	}

	void ApplyInput(ItemInfo& item)
	{
		auto& player = GetLaraInfo(item);

		// Under Classic controls all Remastered state is cleared, so the toggles are off on a switch back.
		if (!g_Configuration.IsUsingRemasteredControls())
		{
			player.Control.Modern = {};
			return;
		}

		player.Control.Modern.BackResolution = ModernBackResolution::None;
		player.Control.Modern.IsCrouchStageUp = false;

		// Getting on or off a vehicle releases the Look toggle.
		bool isInVehicle = (player.Context.Vehicle != NO_VALUE);
		if (isInVehicle != player.Control.Modern.WasInVehicle)
		{
			player.Control.Modern.IsLookToggled = false;
			player.Control.Modern.WasInVehicle = isInVehicle;
		}

		// Vehicles skip the input rewrite, but the Look toggle still latches and a Look press still turns the camera behind the player.
		if (isInVehicle)
		{
			bool wasLookToggled = player.Control.Modern.IsLookToggled;
			UpdateVehicleLookToggle(item);
			ResetCameraOnLookStart(item, wasLookToggled);
			return;
		}

		auto input = ReadPlayerInput();
		auto heldWithoutClickInput = PlayerInput{};
		TransformInput(item, input, heldWithoutClickInput);
		MaskClearedInput(item, input);
		WritePlayerInput(input, heldWithoutClickInput);
	}

	// Turns the player toward the lip during the safety grab reach. The turn steps from its own last yaw,
	// so a side deflection added by the stock reach code in between is discarded. True while it turns.
	static bool HandleSafetyGrabTurn(ItemInfo& item)
	{
		constexpr auto TURN_RATE	 = 1.0f;
		constexpr auto TURN_STEP_MAX = ANGLE(60.0f);

		auto& player = GetLaraInfo(item);
		auto& modern = player.Control.Modern;

		if (!modern.IsSafetyGrabTurning)
			return false;

		if (!item.Animation.IsAirborne ||
			(item.Animation.ActiveState != LS_REACH && item.Animation.ActiveState != LS_FREEFALL))
		{
			modern.IsSafetyGrabTurning = false;
			return false;
		}

		item.Pose.Orientation.y = ApproachHeadingAngle(modern.SafetyGrabTurnYaw, modern.SafetyGrabTurnAngle, TURN_RATE, TURN_STEP_MAX, modern.HeadingTurnSign);
		modern.SafetyGrabTurnYaw = item.Pose.Orientation.y;

		player.Control.MoveAngle = item.Pose.Orientation.y;
		player.Control.TurnRate = 0;

		if (item.Pose.Orientation.y == modern.SafetyGrabTurnAngle)
			modern.IsSafetyGrabTurning = false;

		return true;
	}

	// Turns the player toward the heading the stick points to, at the turn rate of her current state.
	void HandleHeading(ItemInfo& item)
	{
		constexpr auto TURN_RATE_180	 = 1.0f;
		constexpr auto TURN_STEP_MAX_180 = ANGLE(90.0f);

		if (!g_Configuration.IsUsingRemasteredControls())
			return;

		auto& player = GetLaraInfo(item);

		if (HandleSafetyGrabTurn(item))
			return;

		if (!DoesSolverOwnHeading(item))
			return;

		player.Control.TurnRate = 0;

		// No turn on a tank back step or on a mid-air roll push.
		if (player.Control.Modern.BackResolution == ModernBackResolution::TankBack ||
			player.Control.Modern.BackResolution == ModernBackResolution::Roll)
		{
			return;
		}

		if (player.Control.IsMoving)
			return;

		if (player.Control.RefMoveAxis == Vector2::Zero)
			return;

		float rate = 0.0f;
		short stepMax = 0;
		GetTurnParams(item, rate, stepMax);

		if (player.Control.Modern.BackResolution == ModernBackResolution::Turn180)
		{
			rate = TURN_RATE_180;
			stepMax = TURN_STEP_MAX_180;
		}

		if (IsHeld(In::Look))
			return;

		short headingAngle = GetPlayerHeadingAngleY(item);
		item.Pose.Orientation.y = ApproachHeadingAngle(item.Pose.Orientation.y, headingAngle, rate, stepMax, player.Control.Modern.HeadingTurnSign);
	}

	// Water: turns the player toward the heading the stick points to and, underwater, pitches her toward the camera elevation.
	// Zeroes the classic turn rate in the water states it steers. Stands down while she aligns, looks, or the stick is centered.
	// Underwater it also holds the roll at zero.
	void HandleWaterHeading(ItemInfo& item)
	{
		constexpr auto PITCH_RATE		= 1.0f;
		constexpr auto PITCH_STEP_MAX	= ANGLE(11.25f);

		if (!g_Configuration.IsUsingRemasteredControls())
			return;

		auto& player = GetLaraInfo(item);

		float rate = 0.0f;
		short stepMax = 0;
		bool isUnderwater = false;
		if (!GetWaterTurnParams(item, rate, stepMax, isUnderwater))
			return;

		player.Control.TurnRate = 0;

		if (player.Control.IsMoving)
			return;

		if (IsHeld(In::Look))
			return;

		// Under fixed and flyby cameras, the right stick and mouse pitch her, also while the stick is centered.
		short viewPitchStep = 0;
		bool isViewPitch = (isUnderwater && GetViewPitchStep(viewPitchStep));
		bool isStill = (player.Control.RefMoveAxis == Vector2::Zero);
		if (isViewPitch)
		{
			if (!isStill)
				viewPitchStep += GetFloorLiftPitch(item);

			item.Pose.Orientation.x += viewPitchStep;
			item.Pose.Orientation.z = 0;
		}

		if (isStill)
			return;

		auto& modern = player.Control.Modern;
		short headingAngle = GetPlayerHeadingAngleY(item);
		item.Pose.Orientation.y = ApproachHeadingAngle(item.Pose.Orientation.y, headingAngle, rate, stepMax, modern.HeadingTurnSign);

		// Surface swimming stays level.
		if (!isUnderwater || isViewPitch)
			return;

		// A positive camera elevation puts the camera below the player, looking up, so she pitches up.
		int pitchTurnSign = 0;
		item.Pose.Orientation.x = ApproachHeadingAngle(item.Pose.Orientation.x, player.Control.RefCameraOrient.x, PITCH_RATE, PITCH_STEP_MAX, pitchTurnSign);
		item.Pose.Orientation.z = 0;
	}

	// Move axis without the components whose direction action is off, and with Forward and Back canceling each other like
	// Left and Right, so a script clear or a pair held together stops it.
	Vector2 GetHeldMoveAxis()
	{
		auto axis = GetMoveAxis();
		if (!IsHeld(In::Left) && !IsHeld(In::Right))
			axis.x = 0.0f;

		if (IsHeld(In::Forward) == IsHeld(In::Back))
			axis.y = 0.0f;

		return axis;
	}

	static bool IsValidScriptControlMode(int controlModeID)
	{
		return (controlModeID >= 0 && controlModeID < (int)ScriptControlMode::Count);
	}

	// Control mode a gameflow entry or a script forces, or none for Default, which leaves the choice to the next one.
	static std::optional<ControlMode> GetForcedControlMode(ScriptControlMode scriptControlMode)
	{
		switch (scriptControlMode)
		{
		case ScriptControlMode::Classic:
			return ControlMode::Classic;

		case ScriptControlMode::Remastered:
			return ControlMode::RemasteredControls;

		case ScriptControlMode::Default:
			return std::nullopt;

		default:
			TENLog("Control mode " + std::to_string((int)scriptControlMode) + " does not exist.", LogLevel::Warning);
			return std::nullopt;
		}
	}

	// Picks the control mode set by a running script, the current level's gameflow entry or the gameflow settings, in that order.
	// Reloads the bindings and puts the camera behind the player when the mode in use changes.
	static void UpdateControlMode()
	{
		auto prevControlMode = g_Configuration.GetControlMode();

		auto forcedControlMode = GetForcedControlMode(RunningScriptControlMode);
		if (!forcedControlMode.has_value())
			forcedControlMode = GetForcedControlMode(g_GameFlow->GetLevel(CurrentLevel)->GetScriptControlMode());

		if (!forcedControlMode.has_value())
			forcedControlMode = GetForcedControlMode(g_GameFlow->GetSettings()->Gameplay.ForcedControlMode.value_or(ScriptControlMode::Default));

		g_Configuration.ForcedControlMode = forcedControlMode;
		if (g_Configuration.GetControlMode() == prevControlMode)
			return;

		RefreshBindingsForControlMode();

		if (LaraItem != nullptr)
			ResetCameraOrient(*LaraItem);
	}

	// Level start: clears a mode set by a running script where the game variables are cleared, then applies the gameflow's.
	void InitializeControlMode(bool clearScriptMode)
	{
		if (clearScriptMode)
			RunningScriptControlMode = ScriptControlMode::Default;

		UpdateControlMode();
	}

	// Default releases the mode set by a running script.
	void SetScriptControlMode(ScriptControlMode controlMode)
	{
		RunningScriptControlMode = controlMode;
		UpdateControlMode();
	}

	// Savegame form of the mode set by a running script.
	int GetScriptControlModeID()
	{
		return (int)RunningScriptControlMode;
	}

	void SetScriptControlModeID(int controlModeID)
	{
		SetScriptControlMode(IsValidScriptControlMode(controlModeID) ? (ScriptControlMode)controlModeID : ScriptControlMode::Default);
	}

	// Underwater vehicle steering, each turn rate capped by the vehicle's own maximum. With Accelerate held, the yaw steps
	// toward the heading the stick points to, or the camera's heading with the stick centered, and below the surface the
	// pitch steps toward the camera pitch. Back held with Accelerate keeps the heading it gave until Back is released.
	// Stopped, the stick's x axis turns it toward that side of the camera and, below the surface, its y axis pitches it.
	// Stands down while the player looks around.
	void HandleUnderwaterVehicleHeading(const ItemInfo& vehicleItem, LaraInfo& player, EulerAngles& turnRate, const EulerAngles& turnRateMax, bool isAtSurface)
	{
		if (!g_Configuration.IsUsingRemasteredControls() || IsDirectionPushed())
			return;

		if (IsLookAroundHeld(player))
			return;

		const auto& orient = vehicleItem.Pose.Orientation;
		auto axis = GetHeldMoveAxis();
		bool isAccelerating = IsHeld(In::Accelerate);

		auto& modern = player.Control.Modern;
		bool isBackHeld = (isAccelerating && axis.y < 0.0f);
		if (!isBackHeld)
			modern.IsVehicleBackLocked = false;

		// Under fixed and flyby cameras, the right stick and mouse pitch it, and a centered stick keeps its heading.
		short viewPitchStep = 0;
		bool isViewFrame = GetViewPitchStep(viewPitchStep);

		// Stopped, only the stick's x axis turns it.
		auto headingAxis = isAccelerating ? axis : Vector2(axis.x, 0.0f);
		if ((isAccelerating && !isViewFrame) || headingAxis != Vector2::Zero)
		{
			short headingAngle = (headingAxis == Vector2::Zero) ? player.Control.RefCameraOrient.y : GetCameraRelativeAngleY(player, headingAxis);
			if (isBackHeld)
			{
				if (!modern.IsVehicleBackLocked)
				{
					modern.VehicleBackAngleY = headingAngle;
					modern.IsVehicleBackLocked = true;
				}

				headingAngle = modern.VehicleBackAngleY;
			}

			short yawAngle = ApproachHeadingAngle(orient.y, headingAngle, VEHICLE_HEADING_SETUP.YawRate, turnRateMax.y, modern.HeadingTurnSign);
			turnRate.y = Geometry::GetShortestAngle(orient.y, yawAngle);
		}

		// At the surface the vehicle keeps its own pitch.
		if (isAtSurface)
			return;

		if (isAccelerating && !isViewFrame)
		{
			// A positive camera elevation puts the camera below the vehicle, looking up, so it pitches up.
			int pitchTurnSign = 0;
			short pitchAngle = ApproachHeadingAngle(orient.x, player.Control.RefCameraOrient.x, VEHICLE_HEADING_SETUP.PitchRate, turnRateMax.x, pitchTurnSign);
			turnRate.x = Geometry::GetShortestAngle(orient.x, pitchAngle);
		}
		else if (!isAccelerating && axis.y != 0.0f)
		{
			// Stick up pitches up, unless the camera's vertical axis is inverted.
			float pitchCoeff = g_Configuration.InvertCameraYAxis ? -axis.y : axis.y;
			turnRate.x = (short)(pitchCoeff * turnRateMax.x);
		}
		else if (isViewFrame)
		{
			turnRate.x = viewPitchStep;
		}
	}

	// Remastered controls: turn states whose turn keys turn the player at the full rate, since the stick's x axis is camera-relative. -- divezzi, 12.10.26
	static bool IsUnscaledTurnState(const ItemInfo& item)
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

	bool GetUnscaledTurnAxisCoeff(const ItemInfo& item, float& axisCoeff)
	{
		const auto& player = GetLaraInfo(item);

		if (!IsUnscaledTurnState(item) ||
			!(player.Control.Modern.IsArmedTurning || IsHeld(In::Left) || IsHeld(In::Right)))
		{
			return false;
		}

		bool isTurningLeft = (item.Animation.ActiveState == LS_TURN_LEFT_SLOW ||
							  item.Animation.ActiveState == LS_TURN_LEFT_FAST);
		axisCoeff = isTurningLeft ? -1.0f : 1.0f;
		return true;
	}

	// Remastered controls: the armed turn does not overshoot the heading it turns to. -- divezzi, 12.10.26
	void ClampArmedTurnRate(ItemInfo& item)
	{
		auto& player = GetLaraInfo(item);
		if (!player.Control.Modern.IsArmedTurning)
			return;

		// NOTE: In int, so an error of exactly 180 degrees does not wrap to -180.
		int errorMax = abs((int)player.Control.Modern.ArmedTurnErrorY);
		player.Control.TurnRate = (short)std::clamp((int)player.Control.TurnRate, -errorMax, errorMax);
	}

	// Remastered controls: after the crawl grab's turn, the player hangs from the edge she now faces. -- divezzi, 11.10.26
	bool HandleCrawlSafetyGrabHang(ItemInfo& item, CollisionInfo& coll)
	{
		auto& player = GetLaraInfo(item);
		if (!player.Control.Modern.IsCrawlSafetyGrabTurning)
			return false;

		player.Control.Modern.IsCrawlSafetyGrabTurning = false;

		if (!TestLaraCrawlToHang(&item, &coll))
			return false;

		item.Animation.TargetState = LS_CRAWL_TO_HANG;
		DoLaraCrawlToHangSnap(&item, &coll);
		return true;
	}

	// Remastered controls: a crouch press on knees rises to the crouch without drawing a weapon. -- divezzi, 10.10.26
	bool HandleCrouchStageUp(ItemInfo& item)
	{
		auto& player = GetLaraInfo(item);
		if (!player.Control.Modern.IsCrouchStageUp || !TestStateDispatch(item, LS_CROUCH_IDLE))
			return false;

		item.Animation.TargetState = LS_CROUCH_IDLE;
		player.Control.HandStatus = HandStatus::Free;
		return true;
	}

	// Remastered controls: at a crawlspace edge with nothing to vault, the grab button turns the player around to hang from it. -- divezzi, 11.10.26
	bool HandleCrawlSafetyGrabTurn(ItemInfo& item, CollisionInfo& coll)
	{
		if (!IsGrabHeld() || !TestLaraCrawlToHang(&item, &coll, ANGLE(0.0f)))
			return false;

		auto& player = GetLaraInfo(item);
		player.Control.Modern.IsCrawlSafetyGrabTurning = true;
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

	bool IsDrawHeld(const LaraInfo& player)
	{
		return (IsHeld(In::Draw) || player.Control.Modern.IsDrawToggled);
	}

	// Remastered controls: weapons stay drawn while Draw is held and are holstered on release. -- divezzi, 12.10.26
	bool IsHolsterRequested(const LaraInfo& player)
	{
		if (!g_Configuration.IsUsingRemasteredControls())
			return IsHeld(In::Draw);

		return !IsDrawHeld(player);
	}

	void UpdateDrawToggle(LaraInfo& player)
	{
		auto& modern = player.Control.Modern;
		if (!modern.IsDrawToggled)
			return;

		if (IsHeld(In::Draw) ||
			(player.Control.HandStatus == HandStatus::Free &&
			 player.Control.Weapon.RequestGunType == player.Control.Weapon.GunType))
		{
			modern.IsDrawToggled = false;
		}
	}

	// Remastered controls: a script draw keeps the weapons out until the player holds Draw. -- divezzi, 12.10.26
	void SetDrawToggle(ItemInfo& laraItem)
	{
		if (!g_Configuration.IsUsingRemasteredControls())
			return;

		auto& player = GetLaraInfo(laraItem);
		player.Control.Modern.IsDrawToggled = true;
	}

	void SetHolsters(LaraInfo& player, LaraWeaponType weaponType)
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
}
