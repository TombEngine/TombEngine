#include "framework.h"
#include "Objects/TR5/Trap/tr5_wreckingball.h"

#include "Game/Animation/Animation.h"
#include "Game/camera.h"
#include "Game/collision/collide_item.h"
#include "Game/collision/collide_room.h"
#include "Game/effects/effects.h"
#include "Game/effects/Light.h"
#include "Game/effects/tomb4fx.h"
#include "Game/effects/weather.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/room.h"
#include "Objects/TR5/Light/tr5_light.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Scripting/Internal/TEN/Properties/PropertyHandler.h"
#include "Scripting/Internal/TEN/Properties/PropertyNames.h"
#include "Sound/sound.h"
#include "Specific/Input/Input.h"
#include "Specific/clock.h"
#include "Specific/level.h"

#include <algorithm>
#include <unordered_map>

using namespace TEN::Animation;
using namespace TEN::Effects::Environment;
using namespace TEN::Math;

namespace TEN::Entities::Traps
{
	constexpr auto WRECKINGBALL_STATE_IDLE = 0;
	constexpr auto WRECKINGBALL_STATE_DROP = 1;
	constexpr auto WRECKINGBALL_STATE_ATTACK = 2;
	constexpr auto WRECKINGBALL_STATE_RAISE = 3;

	constexpr int MOVE_SPEED = 32;

	// Maximum number of individual chain links that can be stacked between the anchor and the ball.
	constexpr int MAX_CHAIN_LINKS = 64;

	// Distance from the anchor (ceiling) to the ball's center when the ball is fully raised.
	constexpr int BALL_HANG_OFFSET_Y = 1644;

	// Vertical extent of a single chain-link mesh in world units. The links are stacked using this
	// spacing, so it must match the actual link mesh height for the chain to be continuous.
	constexpr int CHAIN_LINK_SPACING = CLICK(0.3);

	constexpr auto SPOTLIGHT_ANCHOR_OFFSET_Y = 512.0f;

	// Downward light auto-scale ratios (used when radius/distance properties are left at 0).
	constexpr auto DOWN_LIGHT_RADIUS_RATIO = 0.2f;
	constexpr auto DOWN_LIGHT_FALLOFF_RATIO = 0.45f;
	constexpr auto DOWN_LIGHT_HASH_OFFSET = 1000;

	// Alarm light auto-scale ratios and defaults.
	constexpr auto ALARM_LIGHT_RADIUS_RATIO = 0.6f;
	constexpr auto ALARM_LIGHT_FALLOFF_RATIO = 0.5f;
	constexpr auto ALARM_LIGHT_BLINK_SPEED_DEFAULT = 1.0f;
	constexpr auto ALARM_LIGHT_HASH_OFFSET = 500;

	// Object-specific property hashes (not meant to be shared engine-wide).
	static const auto PropName_MovementSpeed = GetHash("MovementSpeed");
	static const auto PropName_RoamDelay = GetHash("RoamDelay");
	static const auto PropName_RoamPauseMin = GetHash("RoamPauseMin");
	static const auto PropName_RoamPauseMax = GetHash("RoamPauseMax");

	static const auto PropName_DownwardLightEnabled = GetHash("DownwardLightEnabled");
	static const auto PropName_DownwardLightColor = GetHash("DownwardLightColor");
	static const auto PropName_DownwardLightIntensity = GetHash("DownwardLightIntensity");
	static const auto PropName_DownwardLightRadius = GetHash("DownwardLightRadius");
	static const auto PropName_DownwardLightDistance = GetHash("DownwardLightDistance");

	static const auto PropName_AlarmLightEnabled = GetHash("AlarmLightEnabled");
	static const auto PropName_AlarmLightColor = GetHash("AlarmLightColor");
	static const auto PropName_AlarmLightIntensity = GetHash("AlarmLightIntensity");
	static const auto PropName_AlarmLightRadius = GetHash("AlarmLightRadius");
	static const auto PropName_AlarmLightDistance = GetHash("AlarmLightDistance");
	static const auto PropName_AlarmLightRotationSpeed = GetHash("AlarmLightRotationSpeed");
	static const auto PropName_AlarmLightBlinkSpeed = GetHash("AlarmLightBlinkSpeed");

	struct WreckingBallState
	{
		enum class Phase
		{
			IdleAtTop,
			Moving,
			Roaming,
			PreparingDrop,
			Dropping,
			WinchUp
		};

		Phase PhaseState = Phase::IdleAtTop;
		int   MoveAxis = 0;
		int   Timer = 0;
		int   DropDelay = 0;

		short BaseObject = -1;

		std::vector<short> Links = {};

		int TargetX = 0;
		int TargetZ = 0;

		int RoamTimer = 0;
		int RoamTargetX = 0;
		int RoamTargetZ = 0;
		int RoamWaitTimer = 0;

		float RotatingSpotlightAngle = 0.0f;
		float PreviousSpotlightAngle = 0.0f;
		int AlarmFlashTimer = 0;
		int AlarmFlashPeriod = 0;
		int AlarmFlashOn = 0;
	};

	static std::unordered_map<short, WreckingBallState> WreckingBallStates;

	// ---------------------------------------------------------------------
	// Helpers
	// ---------------------------------------------------------------------

	static void SetItemInvisible(ItemInfo& item)
	{
		item.Flags |= IFLAG_INVISIBLE;
		item.Status = ITEM_INVISIBLE;
	}

	static bool IsCeilingSafeForAnchor(const ItemInfo& anchor, int newX, int newZ)
	{
		int   anchorPosY = anchor.Pose.Position.y;
		short room = anchor.RoomNumber;

		int currentCeiling = GetCeiling(
			GetFloor(anchor.Pose.Position.x, anchorPosY, anchor.Pose.Position.z, &room),
			anchor.Pose.Position.x, anchorPosY, anchor.Pose.Position.z);

		room = anchor.RoomNumber;
		int newCeiling = GetCeiling(
			GetFloor(newX, anchorPosY, newZ, &room),
			newX, anchorPosY, newZ);

		if (newCeiling == NO_HEIGHT || currentCeiling == NO_HEIGHT)
			return false;

		// The ball rides a ceiling rail and is locked to its height, so it can only travel across
		// tiles whose ceiling is flush with the anchor's rail. A tile with a higher or lower ceiling
		// would break the rail, so only an exact match lets the ball move onto it.
		return (newCeiling == currentCeiling);
	}

	// Returns whether the ball can physically travel from its current tile to the given tile along the
	// ceiling rail, following the same dominant-axis stepping as MoveAlongRail. A target tile with a
	// flush ceiling but no continuous safe path (a rail gap) is treated as unreachable, so the ball
	// lets its roam countdown take over instead of grinding at the rail edge.
	static bool IsTileTravelReachable(const ItemInfo& anchor, int startX, int startZ, int targetX, int targetZ)
	{
		int curX = (startX & ~0x3FF) | 512;
		int curZ = (startZ & ~0x3FF) | 512;
		int dstX = (targetX & ~0x3FF) | 512;
		int dstZ = (targetZ & ~0x3FF) | 512;

		int guard = 0;
		while (curX != dstX || curZ != dstZ)
		{
			if (++guard > 256)
				return false;

			int dx = dstX - curX;
			int dz = dstZ - curZ;

			if (std::abs(dx) >= std::abs(dz) && dx != 0)
			{
				int nextX = curX + (dx > 0 ? CLICK(1) : -CLICK(1));
				if (!IsCeilingSafeForAnchor(anchor, nextX, curZ))
					return false;
				curX = nextX;
			}
			else if (dz != 0)
			{
				int nextZ = curZ + (dz > 0 ? CLICK(1) : -CLICK(1));
				if (!IsCeilingSafeForAnchor(anchor, curX, nextZ))
					return false;
				curZ = nextZ;
			}
		}
		return true;
	}

	// Picks a random tile physically reachable from the ball along the rail. Used for roaming when
	// Lara is out of reach; only truly reachable tiles are offered so the ball never creeps at a gap.
	static bool FindRandomReachableTile(const ItemInfo& anchor, int& outX, int& outZ)
	{
		std::vector<std::pair<int, int>> candidates;

		// Start point for path checks: the anchor tracks the ball, so it marks the current tile.
		int startX = anchor.Pose.Position.x;
		int startZ = anchor.Pose.Position.z;

		for (int dx = -4; dx <= 4; dx++)
		{
			for (int dz = -4; dz <= 4; dz++)
			{
				int testX = ((anchor.Pose.Position.x + dx * CLICK(1)) & ~0x3FF) | 512;
				int testZ = ((anchor.Pose.Position.z + dz * CLICK(1)) & ~0x3FF) | 512;

				if (IsTileTravelReachable(anchor, startX, startZ, testX, testZ))
					candidates.push_back({ testX, testZ });
			}
		}

		if (candidates.empty())
			return false;

		auto& pick = candidates[Random::GenerateInt(0, (int)candidates.size() - 1)];
		outX = pick.first;
		outZ = pick.second;
		return true;
	}

	// Moves the ball along the ceiling rail toward the given tile center at a constant speed. The
	// travel axis is locked at the start of each pursuit, so the ball completes the dominant axis
	// before taking the other: it always traces a strict L shape and never moves diagonally.
	// Returns true if the ball moved this frame, false if it is blocked or already settled.
	static bool MoveAlongRail(ItemInfo& item, WreckingBallState& state, int targetX, int targetZ)
	{
		auto& anchor = g_Level.Items[state.BaseObject];
		int   moveSpeed = PropertyHandler::Get(item, PropName_MovementSpeed, MOVE_SPEED);

		int dx = targetX - item.Pose.Position.x;
		int dz = targetZ - item.Pose.Position.z;

		// Lock the travel axis once per pursuit. Completing it first enforces an L-shaped path.
		if (state.MoveAxis == 0)
			state.MoveAxis = (std::abs(dx) > std::abs(dz)) ? 1 : 2;

		auto tryMoveAxis = [&](int axis)
		{
			if (axis == 1)
			{
				if (dx == 0)
					return false;

				int step = std::clamp(dx, -moveSpeed, moveSpeed);
				int newX = item.Pose.Position.x + step;
				if (!IsCeilingSafeForAnchor(anchor, newX, item.Pose.Position.z))
					return false;

				item.Pose.Position.x = newX;
				SoundEffect(SFX_TR5_BASE_CLAW_MOTOR_B_LOOP, &item.Pose);
				return true;
			}

			if (dz == 0)
				return false;

			int step = std::clamp(dz, -moveSpeed, moveSpeed);
			int newZ = item.Pose.Position.z + step;
			if (!IsCeilingSafeForAnchor(anchor, item.Pose.Position.x, newZ))
				return false;

			item.Pose.Position.z = newZ;
			SoundEffect(SFX_TR5_BASE_CLAW_MOTOR_B_LOOP, &item.Pose);
			return true;
		};

		// Traverse the locked axis first; only when it is exhausted move along the other axis. A
		// single frame therefore moves on only one axis, keeping the path strictly L-shaped.
		if (tryMoveAxis(state.MoveAxis))
			return true;
		if (tryMoveAxis(state.MoveAxis == 1 ? 2 : 1))
			return true;

		// Fully blocked or settled. Stop centered on the current tile rather than on a rail edge.
		item.Pose.Position.x = (item.Pose.Position.x & ~0x3FF) | 512;
		item.Pose.Position.z = (item.Pose.Position.z & ~0x3FF) | 512;
		return false;
	}
	static void SpawnAnchorSpotlights(const ItemInfo& anchor, const ItemInfo& wreckingBall, float& angle, int& flashTimer, int alarmFlashPeriod, int alarmFlashOn)
	{
		auto origin = Vector3(
			(float)anchor.Pose.Position.x,
			(float)anchor.Pose.Position.y + SPOTLIGHT_ANCHOR_OFFSET_Y,
			(float)anchor.Pose.Position.z);

		// a) Static downward spotlight - distance scales to floor below ball.
		if (PropertyHandler::Get(wreckingBall, PropName_DownwardLightEnabled, true))
		{
			int floorY = GetPointCollision(wreckingBall).GetFloorHeight();
			float dynDist = (float)(floorY - anchor.Pose.Position.y);
			if (dynDist < 256.0f)
				dynDist = 256.0f; // FAILSAFE: Prevent zero or negative distance.

			// Allow distance/radius override via properties; otherwise scale to floor below ball.
			float propDist = PropertyHandler::Get(wreckingBall, PropName_DownwardLightDistance, 0.0f);
			if (propDist > 0.0f)
				dynDist = BLOCK(propDist);

			float dynRadius = PropertyHandler::Get(wreckingBall, PropName_DownwardLightRadius, 0.0f);
			if (dynRadius <= 0.0f)
				dynRadius = dynDist * DOWN_LIGHT_RADIUS_RATIO;
			else
				dynRadius = BLOCK(dynRadius);

			float dynFalloff = dynDist * DOWN_LIGHT_FALLOFF_RATIO;

			auto downColor = PropertyHandler::Get(wreckingBall, PropName_DownwardLightColor, ScriptColor(128, 128, 128));
			float intensity = PropertyHandler::Get(wreckingBall, PropName_DownwardLightIntensity, 5.0f);

			Vector3 dir(0.0f, 1.0f, 0.0f);
			Color color(
				(downColor.GetR() / (float)UCHAR_MAX) * intensity,
				(downColor.GetG() / (float)UCHAR_MAX) * intensity,
				(downColor.GetB() / (float)UCHAR_MAX) * intensity);

			int hash = anchor.Index + DOWN_LIGHT_HASH_OFFSET;

			SpawnDynamicSpotLight(origin, dir, color, dynRadius, dynFalloff, dynDist, true, hash);
		}

		// b) Rotating alarm spotlight - fixed large distance to sweep room walls.
		if (PropertyHandler::Get(wreckingBall, PropName_AlarmLightEnabled, true))
		{
			float rotateSpeed = PropertyHandler::Get(wreckingBall, PropName_AlarmLightRotationSpeed, 0.7f);
			angle += rotateSpeed;
			if (angle > PI_MUL_2)
				angle -= PI_MUL_2;

			const auto& room = g_Level.Rooms[anchor.RoomNumber];

			float roomSizeX = (float)room.XSize * BLOCK(1);
			float roomSizeZ = (float)room.ZSize * BLOCK(1);
			float alarmDist = std::max(roomSizeX, roomSizeZ);
			float propDist = PropertyHandler::Get(wreckingBall, PropName_AlarmLightDistance, 0.0f);
			float alarmRadius = PropertyHandler::Get(wreckingBall, PropName_AlarmLightRadius, 0.0f);

			if (propDist > 0.0f)
				alarmDist = BLOCK(propDist);

			if (alarmRadius <= 0.0f)
				alarmRadius = alarmDist * ALARM_LIGHT_RADIUS_RATIO;
			else
				alarmRadius = BLOCK(alarmRadius);

			float alarmFalloff = alarmDist * ALARM_LIGHT_FALLOFF_RATIO;

			auto alarmColor = PropertyHandler::Get(wreckingBall, PropName_AlarmLightColor, ScriptColor(255, 0, 0));
			float intensity = PropertyHandler::Get(wreckingBall, PropName_AlarmLightIntensity, 2.0f);

			Vector3 dir(sin(angle), 0.0f, cos(angle));
			dir.Normalize();

			Color color(
				(alarmColor.GetR() / (float)UCHAR_MAX) * intensity,
				(alarmColor.GetG() / (float)UCHAR_MAX) * intensity,
				(alarmColor.GetB() / (float)UCHAR_MAX) * intensity);

			int hash = anchor.Index + ALARM_LIGHT_HASH_OFFSET;

			flashTimer = (flashTimer + 1) % alarmFlashPeriod;

			if (flashTimer < alarmFlashOn)
			{
				SpawnDynamicSpotLight(origin, dir, color, alarmRadius, alarmFalloff, alarmDist, false, hash);
			}
		}
	}

	static void UpdateAnchor(ItemInfo& item, WreckingBallState& state)
	{
		if (state.BaseObject < 0)
			return;

		auto& anchor = g_Level.Items[state.BaseObject];

		anchor.Pose.Position.x = item.Pose.Position.x;
		anchor.Pose.Position.z = item.Pose.Position.z;

		short room = anchor.RoomNumber;
		anchor.Pose.Position.y = GetCeiling(
			GetFloor(anchor.Pose.Position.x, anchor.Pose.Position.y, anchor.Pose.Position.z, &room),
			anchor.Pose.Position.x,
			anchor.Pose.Position.y,
			anchor.Pose.Position.z);

		if (room != anchor.RoomNumber)
			ItemNewRoom(state.BaseObject, room);

		SpawnAnchorSpotlights(anchor, item, state.RotatingSpotlightAngle, state.AlarmFlashTimer, state.AlarmFlashPeriod, state.AlarmFlashOn);
	}

	// Tilt the ball to the floor normal so it rests flush on slopes. Only the X and Z axes rotate;
	// the Y heading is preserved.
	static void AlignBallToSurface(ItemInfo& item, float alpha = 0.15f)
	{
		auto floorNormal = GetPointCollision(item).GetFloorNormal();
		auto orient = Geometry::GetRelOrientToNormal(item.Pose.Orientation.y, floorNormal);

		auto extraRot = orient - item.Pose.Orientation;
		item.Pose.Orientation += extraRot * alpha;
	}

	// Ease the ball back to a straight hang so it is vertical on the ceiling rail. Only the X and Z
	// tilt is reset; the Y heading is preserved.
	static void ResetBallToVertical(ItemInfo& item, float alpha = 0.1f)
	{
		auto vertical = EulerAngles(0, item.Pose.Orientation.y, 0);

		auto extraRot = vertical - item.Pose.Orientation;
		item.Pose.Orientation += extraRot * alpha;
	}

	static void UpdateChain(ItemInfo& item, WreckingBallState& state)
	{
		if (state.BaseObject < 0)
			return;

		auto& anchor = g_Level.Items[state.BaseObject];

		// The ball's mesh 0 marks the attachment point for the lowest chain link. Stretch the stacked
		// links from the anchor (ceiling) down to that point so the chain always lines up with the ball.
		auto attach = GetJointPosition(&item, 0);

		float distance = (float)attach.y - (float)anchor.Pose.Position.y;
		if (distance < 0.0f)
			distance = 0.0f;

		// Stack links at an absolute, fixed spacing instead of rescaling every link when the chain
		// length changes. This keeps the top of the chain anchored and only the bottom link moves, so
		// the whole stack does not shimmer when the total link count changes each frame.
		int linkCount = std::min<int>(MAX_CHAIN_LINKS, (int)(distance / CHAIN_LINK_SPACING) + 1);

		for (int i = 0; i < MAX_CHAIN_LINKS; i++)
		{
			auto& link = g_Level.Items[state.Links[i]];
			if (state.Links[i] < 0 || i >= linkCount)
			{
				SetItemInvisible(link);
				continue;
			}

			// Each link sits centered in its own fixed slot below the anchor, so only the lowest link
			// changes when the chain length varies. The renderer interpolates each link's position for
			// smooth, jitter-free movement at high frame rates.
			float slotTop = (float)anchor.Pose.Position.y + (i * CHAIN_LINK_SPACING);
			float centerY = slotTop + (CHAIN_LINK_SPACING / 2.0f);
			float t = (float)i / (float)linkCount;

			link.Pose.Position.x = (int)(anchor.Pose.Position.x + ((attach.x - anchor.Pose.Position.x) * t));
			link.Pose.Position.z = (int)(anchor.Pose.Position.z + ((attach.z - anchor.Pose.Position.z) * t));
			link.Pose.Position.y = (int)centerY;
			link.Pose.Orientation.y = ((i % 2) == 0) ? ANGLE(0.0f) : ANGLE(90.0f);
			link.Pose.Scale = Vector3::One;

			link.Flags &= ~IFLAG_INVISIBLE;
			link.Status = ITEM_ACTIVE;

			short room = link.RoomNumber;
			GetFloor(link.Pose.Position.x, link.Pose.Position.y, link.Pose.Position.z, &room);
			if (room != link.RoomNumber)
				ItemNewRoom(state.Links[i], room);
		}
	}

	static void UpdateIdle(ItemInfo& item, WreckingBallState& state)
	{
		if (!LaraItem || state.BaseObject < 0)
			return;

		auto& anchor = g_Level.Items[state.BaseObject];

		int targetX = (LaraItem->Pose.Position.x & ~0x3FF) | 512;
		int targetZ = (LaraItem->Pose.Position.z & ~0x3FF) | 512;

		if (IsTileTravelReachable(anchor, item.Pose.Position.x, item.Pose.Position.z, targetX, targetZ))
		{
			// Lara is reachable along the rail; chase her.
			state.TargetX = targetX;
			state.TargetZ = targetZ;
			state.PhaseState = WreckingBallState::Phase::Moving;
			state.MoveAxis = 0;
			state.Timer = 0;
			state.RoamTimer = 0;
			return;
		}

		// Lara is out of reach. Wait the roam delay, then start roaming the rail area.
		int roamDelay = (int)(std::abs(PropertyHandler::Get(item, PropName_RoamDelay, 1.0f)) * FPS);
		if (roamDelay <= 0)
			roamDelay = 1;

		state.RoamTimer++;
		if (state.RoamTimer < roamDelay)
			return;

		int roamX, roamZ;
			if (FindRandomReachableTile(anchor, roamX, roamZ))
			{
				int pauseMin = (int)(std::abs(PropertyHandler::Get(item, PropName_RoamPauseMin, 0.5f)) * FPS);
				int pauseMax = (int)(std::abs(PropertyHandler::Get(item, PropName_RoamPauseMax, 1.5f)) * FPS);
				if (pauseMin > pauseMax)
					std::swap(pauseMin, pauseMax);
				if (pauseMin == 0)
					pauseMin = 1;

				state.RoamTargetX = roamX;
				state.RoamTargetZ = roamZ;
				state.PhaseState = WreckingBallState::Phase::Roaming;
				state.MoveAxis = 0;
				state.Timer = 0;
				state.RoamTimer = 0;
				state.RoamWaitTimer = Random::GenerateInt(pauseMin, pauseMax);
			}
	}

	static void UpdateHorizontalMovement(ItemInfo& item, WreckingBallState& state)
	{
		if (!LaraItem || state.BaseObject < 0)
		{
			state.PhaseState = WreckingBallState::Phase::IdleAtTop;
			state.MoveAxis = 0;
			state.Timer = 0;
			state.RoamTimer = 0;
			return;
		}

		auto& anchor = g_Level.Items[state.BaseObject];

		int laraX = (LaraItem->Pose.Position.x & ~0x3FF) | 512;
		int laraZ = (LaraItem->Pose.Position.z & ~0x3FF) | 512;

		// If the player's tile is not physically reachable along the rail (a gap separates them), don't
		// chase. Settle centered on the current tile and start the roam countdown immediately.
		if (!IsTileTravelReachable(anchor, item.Pose.Position.x, item.Pose.Position.z, laraX, laraZ))
		{
			item.Pose.Position.x = (item.Pose.Position.x & ~0x3FF) | 512;
			item.Pose.Position.z = (item.Pose.Position.z & ~0x3FF) | 512;

			state.PhaseState = WreckingBallState::Phase::IdleAtTop;
			state.MoveAxis = 0;
			state.Timer = 0;
			state.RoamTimer = 0;
			return;
		}

		int dx = laraX - item.Pose.Position.x;
		int dz = laraZ - item.Pose.Position.z;
		int moveSpeed = PropertyHandler::Get(item, PropName_MovementSpeed, MOVE_SPEED);

		// Arrived alongside Lara's tile center; stop flush over it, centered on the tile.
		if (std::abs(dx) <= moveSpeed && std::abs(dz) <= moveSpeed)
		{
			item.Pose.Position.x = laraX;
			item.Pose.Position.z = laraZ;

			StopSoundEffect(SFX_TR5_BASE_CLAW_MOTOR_B_LOOP);
			SoundEffect(SFX_TR5_BASE_CLAW_MOTOR_C, &item.Pose);

			state.TargetX = laraX;
			state.TargetZ = laraZ;
			state.PhaseState = WreckingBallState::Phase::PreparingDrop;
			state.DropDelay = 30;
			state.MoveAxis = 0;
			state.Timer = 0;
			state.RoamTimer = 0;
			return;
		}

		// The player is reachable; move toward her tile center. This never gets stuck on a rail gap
		// because the reachability gate above already routed inaccessible targets to the roam timer.
		MoveAlongRail(item, state, laraX, laraZ);
	}

	// Roams the ceiling rail area when Lara is out of reach. Continually checks whether Lara
	// has entered a reachable tile; if so, returns to chase mode.
	static void UpdateRoaming(ItemInfo& item, WreckingBallState& state)
	{
		if (!LaraItem || state.BaseObject < 0)
		{
			state.PhaseState = WreckingBallState::Phase::IdleAtTop;
			state.MoveAxis = 0;
			state.Timer = 0;
			return;
		}

		auto& anchor = g_Level.Items[state.BaseObject];

		// If Lara just became reachable, abandon roaming and seek her.
		int laraX = (LaraItem->Pose.Position.x & ~0x3FF) | 512;
		int laraZ = (LaraItem->Pose.Position.z & ~0x3FF) | 512;
		if (IsTileTravelReachable(anchor, item.Pose.Position.x, item.Pose.Position.z, laraX, laraZ))
		{
			state.TargetX = laraX;
			state.TargetZ = laraZ;
			state.PhaseState = WreckingBallState::Phase::Moving;
			state.MoveAxis = 0;
			state.Timer = 0;
			state.RoamTimer = 0;
			return;
		}

		// Pause briefly at each waypoint before continuing.
		if (state.RoamWaitTimer > 0)
		{
			state.RoamWaitTimer--;
			return;
		}

		// Arrived at the current roam waypoint? Pick the next one after a pause.
		int dx = state.RoamTargetX - item.Pose.Position.x;
		int dz = state.RoamTargetZ - item.Pose.Position.z;
		int moveSpeed = PropertyHandler::Get(item, PropName_MovementSpeed, MOVE_SPEED);

		if (std::abs(dx) <= moveSpeed && std::abs(dz) <= moveSpeed)
		{
			int pauseMin = (int)(std::abs(PropertyHandler::Get(item, PropName_RoamPauseMin, 0.5f)) * FPS);
			int pauseMax = (int)(std::abs(PropertyHandler::Get(item, PropName_RoamPauseMax, 1.5f)) * FPS);
			if (pauseMin > pauseMax)
				std::swap(pauseMin, pauseMax);
			if (pauseMin == 0)
				pauseMin = 1;

			int roamX, roamZ;
			if (FindRandomReachableTile(anchor, roamX, roamZ))
			{
				state.RoamTargetX = roamX;
				state.RoamTargetZ = roamZ;
				state.MoveAxis = 0;
				state.Timer = 0;
				state.RoamWaitTimer = Random::GenerateInt(pauseMin, pauseMax);
			}
			else
			{
				// No reachable tiles left; head home.
				state.PhaseState = WreckingBallState::Phase::IdleAtTop;
				state.MoveAxis = 0;
			}
			return;
		}

		bool movedThisFrame = MoveAlongRail(item, state, state.RoamTargetX, state.RoamTargetZ);

		if (!movedThisFrame)
		{
			state.Timer++;

			// We are stuck on the rail; pick a new roam destination.
			if (state.Timer > 10)
			{
				int roamX, roamZ;
				if (FindRandomReachableTile(anchor, roamX, roamZ))
				{
					state.RoamTargetX = roamX;
					state.RoamTargetZ = roamZ;
					state.MoveAxis = 0;
					state.Timer = 0;
				}
				else
				{
					state.PhaseState = WreckingBallState::Phase::IdleAtTop;
					state.MoveAxis = 0;
				}
			}
		}
		else
		{
			state.Timer = 0;
		}
	}

	static void UpdatePreparingDrop(ItemInfo& item, WreckingBallState& state)
	{
		if (state.DropDelay > 0)
		{
			state.DropDelay--;
			return;
		}

		if (item.Animation.ActiveState == WRECKINGBALL_STATE_IDLE)
		{
			item.Animation.TargetState = WRECKINGBALL_STATE_DROP;
			return;
		}

		if (TestLastFrame(item))
		{
			SoundEffect(SFX_TR5_BASE_CLAW_DROP, &item.Pose);

			state.PhaseState = WreckingBallState::Phase::Dropping;
			item.Animation.Velocity.y = 6.0f;
			item.Pose.Position.y += item.Animation.Velocity.y;
		}
	}

	static void UpdateDropping(ItemInfo& item, WreckingBallState& state)
	{
		item.Animation.Velocity.y += 24.0f;
		item.Pose.Position.y += item.Animation.Velocity.y;

		// Continuously settle the ball toward the floor normal so it comes to rest flush on slopes.
		AlignBallToSurface(item);

		short room = item.RoomNumber;
		int   height = GetFloorHeight(
			GetFloor(item.Pose.Position.x, item.Pose.Position.y, item.Pose.Position.z, &room),
			item.Pose.Position.x,
			item.Pose.Position.y,
			item.Pose.Position.z);

		if (height - item.Pose.Position.y < 1536 && item.Animation.ActiveState != WRECKINGBALL_STATE_IDLE)
			item.Animation.TargetState = WRECKINGBALL_STATE_IDLE;

		if (height < item.Pose.Position.y)
		{
			item.Pose.Position.y = height;

			if (item.Animation.Velocity.y > 48.0f)
			{
				BounceCamera(&item, 64, 8192);
				item.Animation.Velocity.y = -item.Animation.Velocity.y / 8.0f;
			}
			else
			{
				item.Animation.Velocity.y = 0.0f;
				state.MoveAxis = 0;
				state.Timer = 0;
				state.PhaseState = WreckingBallState::Phase::WinchUp;
			}
		}
	}

	static void UpdateWinchUp(ItemInfo& item, WreckingBallState& state)
	{
		if (state.BaseObject < 0)
		{
			state.PhaseState = WreckingBallState::Phase::IdleAtTop;
			state.MoveAxis = 0;
			state.Timer = 0;
			item.Animation.Velocity.y = 0;
			return;
		}

		auto& anchor = g_Level.Items[state.BaseObject];
		int   targetY = anchor.Pose.Position.y + BALL_HANG_OFFSET_Y;

		item.Animation.Velocity.y -= 3;
		item.Pose.Position.y += item.Animation.Velocity.y;

		// Straighten the ball as it is winched back up off the slope.
		ResetBallToVertical(item);

		if (item.Pose.Position.y < targetY)
		{
			StopSoundEffect(SFX_TR5_BASE_CLAW_WINCH_UP_LOOP);
			item.Pose.Position.y = targetY;

			if (item.Animation.Velocity.y < -32.0f)
			{
				SoundEffect(SFX_TR5_BASE_CLAW_TOP_IMPACT, &item.Pose, SoundEnvironment::Land, 1.0f, 0.5f);
				item.Animation.Velocity.y = -item.Animation.Velocity.y / 8.0f;
				BounceCamera(&item, 16, 8192);
			}
			else
			{
				item.Animation.Velocity.y = 0;
				state.MoveAxis = 0;
				state.Timer = 0;
				state.PhaseState = WreckingBallState::Phase::IdleAtTop;
			}
		}
		else
		{
			SoundEffect(SFX_TR5_BASE_CLAW_WINCH_UP_LOOP, &item.Pose);
		}
	}

	// ---------------------------------------------------------------------
	// Init
	// ---------------------------------------------------------------------

	void InitializeWreckingBall(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		// Fully reset the state on (re)initialization. This prevents stale phase data from a previous
		// level load or fast reload from carrying over into the new frame.
		auto& state = WreckingBallStates[itemNumber];
		state = WreckingBallState();

		// Initialize alarm flash timing; blink speed scales the base period.
		float blinkSpeed = PropertyHandler::Get(item, PropName_AlarmLightBlinkSpeed, ALARM_LIGHT_BLINK_SPEED_DEFAULT);
		state.AlarmFlashPeriod = std::max(2, (int)(Random::GenerateInt(15, 30) / blinkSpeed));
		state.AlarmFlashOn = std::clamp(Random::GenerateInt(2, 20), 1, state.AlarmFlashPeriod - 1);

		auto pointColl = GetPointCollision(item);

		// The anchor and chain links are created automatically so the builder only needs to place the
		// ball itself. Builder-placed companions (legacy layouts) are still honored when present.
		auto anchors = FindAllItems(ID_WRECKINGBALL_ANCHOR);
		if (anchors.empty())
			anchors.push_back(SpawnItem(item, ID_WRECKINGBALL_ANCHOR));

		state.BaseObject = anchors[0];

			// Spawn the pool of chain links. A builder-placed chain object, if any, is reused as the
		// first link so legacy layouts keep working.
		bool builderChainUsed = false;
		for (int i = 0; i < MAX_CHAIN_LINKS; i++)
		{
			if (!builderChainUsed)
			{
				auto existing = FindAllItems(ID_WRECKINGBALL_CHAIN);
				if (!existing.empty())
				{
					state.Links.push_back(existing[0]);
					builderChainUsed = true;
					continue;
				}
			}

			state.Links.push_back(SpawnItem(item, ID_WRECKINGBALL_CHAIN));
		}

		// Enable interpolation on every link so the renderer smoothly eases their motion between
		// frames. Spawned items default to DisableInterpolation = true, which would otherwise make the
		// chain snap to new positions instead of gliding, causing visible jitter at high frame rates.
		for (short link : state.Links)
			g_Level.Items[link].DisableInterpolation = false;

		// Persist the anchor index in the ball's flags so it can be recovered after a savegame is
		// loaded (where only the flags are serialized, not this module's static state).
		item.ItemFlags[0] = state.BaseObject;

		if (state.BaseObject < 0)
		{
			TENLog(
				"WreckingBall ERROR: Failed to create the required anchor object. "
				"Anchor ID=" + std::to_string(ID_WRECKINGBALL_ANCHOR),
				LogLevel::Error);

			SetItemInvisible(item);
			return;
		}

		// Park the anchor at the ceiling directly above the ball so the rail and chain start there.
		auto& anchor = g_Level.Items[state.BaseObject];
		anchor.Pose.Position.x = item.Pose.Position.x;
		anchor.Pose.Position.z = item.Pose.Position.z;
		anchor.Pose.Position.y = pointColl.GetCeilingHeight();

		item.Pose.Position.y = pointColl.GetCeilingHeight() + BALL_HANG_OFFSET_Y;

		if (pointColl.GetRoomNumber() != item.RoomNumber)
			ItemNewRoom(itemNumber, pointColl.GetRoomNumber());

		state.TargetX = item.Pose.Position.x;
		state.TargetZ = item.Pose.Position.z;
	}

	// ---------------------------------------------------------------------
	// Control
	// ---------------------------------------------------------------------

	void ControlWreckingBall(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];
		auto& state = WreckingBallStates[itemNumber];

		// Recover the anchor index after a savegame is loaded (only the ball's serialized flag
		// persists, the static state map is rebuilt empty).
		if (state.BaseObject < 0)
			state.BaseObject = item.ItemFlags[0];

		// Re-initialize alarm flash timing after savegame load if needed.
		if (state.AlarmFlashPeriod <= 0)
		{
			float blinkSpeed = PropertyHandler::Get(item, PropName_AlarmLightBlinkSpeed, ALARM_LIGHT_BLINK_SPEED_DEFAULT);
			state.AlarmFlashPeriod = std::max(2, (int)(Random::GenerateInt(15, 30) / blinkSpeed));
			state.AlarmFlashOn = std::clamp(Random::GenerateInt(2, 20), 1, state.AlarmFlashPeriod - 1);
		}

		// Bail out if the required companion objects are missing.
		if (state.BaseObject < 0)
			return;

		short room = item.RoomNumber;
		GetFloor(item.Pose.Position.x, item.Pose.Position.y, item.Pose.Position.z, &room);
		if (room != item.RoomNumber)
			ItemNewRoom(itemNumber, room);

		switch (state.PhaseState)
		{
		case WreckingBallState::Phase::IdleAtTop:
			UpdateIdle(item, state);
			break;

				case WreckingBallState::Phase::Moving:
			UpdateHorizontalMovement(item, state);
			break;

		case WreckingBallState::Phase::Roaming:
			UpdateRoaming(item, state);
			break;

		case WreckingBallState::Phase::PreparingDrop:
			UpdatePreparingDrop(item, state);
			break;

		case WreckingBallState::Phase::Dropping:
			UpdateDropping(item, state);
			break;

		case WreckingBallState::Phase::WinchUp:
			UpdateWinchUp(item, state);
			break;
		}

		UpdateAnchor(item, state);
		UpdateChain(item, state);
		AnimateItem(item);
	}

	// ---------------------------------------------------------------------
	// Collision
	// ---------------------------------------------------------------------

	void CollideWreckingBall(short itemNumber, ItemInfo* playerItem, CollisionInfo* coll)
	{
		auto& item = g_Level.Items[itemNumber];

		if (!TestBoundsCollide(&item, playerItem, coll->Setup.Radius))
			return;

		auto prevPos = playerItem->Pose.Position;

		bool killZone = false;
		if ((prevPos.x & WALL_MASK) > CLICK(1) &&
			(prevPos.x & WALL_MASK) < CLICK(3) &&
			(prevPos.z & WALL_MASK) > CLICK(1) &&
			(prevPos.z & WALL_MASK) < CLICK(3))
		{
			killZone = true;
		}

		int damage = (item.Animation.Velocity.y > 0.0f) ? PropertyHandler::Get(item, PropName_Damage, 96) : 0;

		if (ItemPushItem(&item, playerItem, coll, coll->Setup.EnableSpasm, 1))
		{
			if (killZone)
				DoDamage(playerItem, INT_MAX);
			else
				DoDamage(playerItem, damage);

			prevPos -= playerItem->Pose.Position;

			if (damage != 0)
			{
				for (int i = 14 + (GetRandomControl() & 3); i > 0; --i)
				{
					TriggerBlood(
						playerItem->Pose.Position.x + (GetRandomControl() & 63) - 32,
						playerItem->Pose.Position.y - (GetRandomControl() & 511) - 256,
						playerItem->Pose.Position.z + (GetRandomControl() & 63) - 32,
						-1,
						1);
				}
			}

			if (!coll->Setup.EnableObjectPush || killZone)
				playerItem->Pose.Position += prevPos;
		}
	}
}

