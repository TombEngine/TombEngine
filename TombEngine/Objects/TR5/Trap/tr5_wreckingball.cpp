#include "framework.h"
#include "Objects/TR5/Trap/tr5_wreckingball.h"

#include "Game/Animation/Animation.h"
#include "Game/camera.h"
#include "Game/collision/collide_item.h"
#include "Game/collision/collide_room.h"
#include "Game/control/control.h"
#include "Game/effects/bubble.h"
#include "Game/effects/debris.h"
#include "Game/effects/effects.h"
#include "Game/effects/Light.h"
#include "Game/effects/Splash.h"
#include "Game/effects/tomb4fx.h"
#include "Game/effects/weather.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/room.h"
#include "Game/Setup.h"
#include "Objects/TR5/Light/tr5_light.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Scripting/Internal/TEN/Properties/PropertyHandler.h"
#include "Scripting/Internal/TEN/Properties/PropertyNames.h"
#include "Sound/sound.h"
#include "Specific/Input/Input.h"
#include "Specific/clock.h"
#include "Specific/level.h"

#include <algorithm>
#include <limits>
#include <unordered_map>

using namespace TEN::Animation;
using namespace TEN::Effects::Bubble;
using namespace TEN::Effects::Environment;
using namespace TEN::Effects::Splash;
using namespace TEN::Math;

namespace TEN::Entities::Traps
{
	constexpr auto WRECKINGBALL_STATE_IDLE = 0;
	constexpr auto WRECKINGBALL_STATE_DROP = 1;
	constexpr auto WRECKINGBALL_STATE_ATTACK = 2;
	constexpr auto WRECKINGBALL_STATE_RAISE = 3;

	constexpr int MOVE_SPEED = 32;

	// Max number of sectors (tiles) between the ball and Lara within which the claw will pursue
	// her along the rail. Any positive value bounds the pursuit; used as a default only, so the
	// builder can override it via the SearchRadius property.
	constexpr int SEARCH_RADIUS_DEFAULT = 16;

	// Maximum number of individual chain links that can be stacked between the anchor and the ball.
	constexpr int MAX_CHAIN_LINKS = 128;
	constexpr short WRECKINGBALL_COMPANION_MARKER = 0x5742;

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

	// Radius within which the wrecking ball crushes shatterable statics when it lands.
	constexpr int SHATTER_RADIUS = CLICK(1.5f);

	// Radius within which the wrecking ball crushes enemies when it lands.
	constexpr int ENEMY_KILL_RADIUS = BLOCK(1.0f);

	// Object-specific property hashes (not meant to be shared engine-wide).
	static const auto PropName_MovementSpeed = GetHash("MovementSpeed");
	static const auto PropName_SearchRadius = GetHash("SearchRadius");
	static const auto PropName_RoamDelay = GetHash("RoamDelay");
	static const auto PropName_RoamPauseMin = GetHash("RoamPauseMin");
	static const auto PropName_RoamPauseMax = GetHash("RoamPauseMax");
	static const auto PropName_ShatterStatics = GetHash("ShatterStatics");
	static const auto PropName_KillEnemies = GetHash("KillEnemies");
	static const auto PropName_DropDelay = GetHash("DropDelay");
	static const auto PropName_DropSpeed = GetHash("DropSpeed");

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
		int UnderwaterBubbleTimer = 0;

		float RotatingSpotlightAngle = 0.0f;
		float PreviousSpotlightAngle = 0.0f;
		int AlarmFlashTimer = 0;
		int AlarmFlashPeriod = 0;
		int AlarmFlashOn = 0;
	};

	static std::unordered_map<short, WreckingBallState> WreckingBallStates;

	static short EncodeWreckingBallSector(int coordinate)
	{
		int sector = (int)std::floor((float)coordinate / BLOCK(1));
		return (short)std::clamp(sector, (int)std::numeric_limits<short>::min(), (int)std::numeric_limits<short>::max());
	}

	static int DecodeWreckingBallSector(short sector)
	{
		return (sector * BLOCK(1)) + BLOCK(0.5f);
	}

	static void StoreWreckingBallState(ItemInfo& item, const WreckingBallState& state)
	{
		item.ItemFlags[0] = state.BaseObject;
		item.ItemFlags[1] = (short)state.PhaseState;
		item.ItemFlags[2] = (short)state.MoveAxis;
		item.ItemFlags[3] = (short)std::clamp(state.DropDelay, 0, (int)std::numeric_limits<short>::max());
		item.ItemFlags[4] = (short)std::clamp(state.RoamTimer, 0, (int)std::numeric_limits<short>::max());
		item.ItemFlags[5] = EncodeWreckingBallSector(state.RoamTargetX);
		item.ItemFlags[6] = EncodeWreckingBallSector(state.RoamTargetZ);
		item.ItemFlags[7] = (short)std::clamp(state.RoamWaitTimer, 0, (int)std::numeric_limits<short>::max());
	}

	static void RestoreWreckingBallState(const ItemInfo& item, WreckingBallState& state)
	{
		state = WreckingBallState();

		short anchorNumber = item.ItemFlags[0];
		if (anchorNumber >= 0 && anchorNumber < g_Level.Items.size() &&
			g_Level.Items[anchorNumber].ObjectNumber == ID_WRECKINGBALL_ANCHOR)
		{
			state.BaseObject = anchorNumber;
		}

		if (item.ItemFlags[1] >= (short)WreckingBallState::Phase::IdleAtTop &&
			item.ItemFlags[1] <= (short)WreckingBallState::Phase::WinchUp)
		{
			state.PhaseState = (WreckingBallState::Phase)item.ItemFlags[1];
		}

		state.MoveAxis = (item.ItemFlags[2] == 1 || item.ItemFlags[2] == 2) ? item.ItemFlags[2] : 0;
		state.DropDelay = std::max(0, (int)item.ItemFlags[3]);
		state.RoamTimer = std::max(0, (int)item.ItemFlags[4]);
		state.RoamTargetX = DecodeWreckingBallSector(item.ItemFlags[5]);
		state.RoamTargetZ = DecodeWreckingBallSector(item.ItemFlags[6]);
		state.RoamWaitTimer = std::max(0, (int)item.ItemFlags[7]);
		state.TargetX = (item.Pose.Position.x & ~0x3FF) | 512;
		state.TargetZ = (item.Pose.Position.z & ~0x3FF) | 512;
	}

	static void RestoreWreckingBallLinks(const ItemInfo& item, WreckingBallState& state)
	{
		for (const auto& candidate : g_Level.Items)
		{
			if (candidate.ObjectNumber != ID_WRECKINGBALL_CHAIN ||
				candidate.ItemFlags[0] != item.Index ||
				candidate.ItemFlags[1] != WRECKINGBALL_COMPANION_MARKER)
			{
				continue;
			}

			int linkIndex = candidate.ItemFlags[2];
			if (linkIndex < 0 || linkIndex >= MAX_CHAIN_LINKS)
				continue;

			if (state.Links.size() <= linkIndex)
				state.Links.resize(linkIndex + 1, -1);

			state.Links[linkIndex] = candidate.Index;
		}
	}

	static short FindOrCreateWreckingBallAnchor(ItemInfo& item)
	{
		for (auto& candidate : g_Level.Items)
		{
			if (candidate.ObjectNumber == ID_WRECKINGBALL_ANCHOR &&
				candidate.ItemFlags[0] == item.Index &&
				candidate.ItemFlags[1] == WRECKINGBALL_COMPANION_MARKER)
			{
				return candidate.Index;
			}
		}

		int closestAnchor = -1;
		float closestDistance = FLT_MAX;
		for (int i = 0; i < g_Level.NumItems; i++)
		{
			auto& candidate = g_Level.Items[i];
			if (candidate.ObjectNumber != ID_WRECKINGBALL_ANCHOR ||
				candidate.ItemFlags[1] == WRECKINGBALL_COMPANION_MARKER)
			{
				continue;
			}

			float distance = Vector3i::Distance(item.Pose.Position, candidate.Pose.Position);
			if (distance < closestDistance)
			{
				closestDistance = distance;
				closestAnchor = i;
			}
		}

		short anchorNumber = (closestAnchor >= 0) ? (short)closestAnchor : SpawnItem(item, ID_WRECKINGBALL_ANCHOR);
		if (anchorNumber >= 0)
		{
			auto& anchor = g_Level.Items[anchorNumber];
			anchor.ItemFlags[0] = item.Index;
			anchor.ItemFlags[1] = WRECKINGBALL_COMPANION_MARKER;
		}

		return anchorNumber;
	}

	static void RestoreOrAssignWreckingBallLinks(ItemInfo& item, WreckingBallState& state)
	{
		RestoreWreckingBallLinks(item, state);
		if (!state.Links.empty())
			return;

		int closestLink = -1;
		float closestDistance = FLT_MAX;
		for (int i = 0; i < g_Level.NumItems; i++)
		{
			auto& candidate = g_Level.Items[i];
			if (candidate.ObjectNumber != ID_WRECKINGBALL_CHAIN ||
				candidate.ItemFlags[1] == WRECKINGBALL_COMPANION_MARKER)
			{
				continue;
			}

			float distance = Vector3i::Distance(item.Pose.Position, candidate.Pose.Position);
			if (distance < closestDistance)
			{
				closestDistance = distance;
				closestLink = i;
			}
		}

		if (closestLink >= 0)
		{
			auto& link = g_Level.Items[closestLink];
			link.ItemFlags[0] = item.Index;
			link.ItemFlags[1] = WRECKINGBALL_COMPANION_MARKER;
			link.ItemFlags[2] = 0;
			link.DisableInterpolation = false;
			state.Links.push_back((short)closestLink);
		}
	}

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

	// Returns whether Lara is close enough to the ball for the claw to pursue her. A radius of 0
	// (or negative) means the search is unlimited, so the claw chases however far the rail extends.
	static bool IsLaraWithinSearchRadius(const ItemInfo& item)
	{
		if (!LaraItem)
			return false;

		int searchRadius = PropertyHandler::Get(item, PropName_SearchRadius, SEARCH_RADIUS_DEFAULT);
		if (searchRadius <= 0)
			return true;

		// Search radius is a horizontal ground reach in sectors; omit the ceiling-to-floor Y delta so
		// Lara standing directly beneath the hanging ball counts as being in range.
		float distance2D = Vector2i::Distance(
			Vector2i(item.Pose.Position.x, item.Pose.Position.z),
			Vector2i(LaraItem->Pose.Position.x, LaraItem->Pose.Position.z));

		return (distance2D <= CLICK(searchRadius));
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

	// Shatters any statics bearing the shatter flag within the ball landing zone. Only runs when
	// the ball actually drops onto the floor, and is gated by the ShatterStatics property.
	static void ShatterStaticsOnBallImpact(const ItemInfo& item)
	{
		if (!PropertyHandler::Get(item, PropName_ShatterStatics, false))
			return;

		auto& room = g_Level.Rooms[item.RoomNumber];
		for (auto& staticObj : room.mesh)
		{
			if (!(staticObj.Flags & StaticMeshFlags::SM_VISIBLE) ||
				Statics[staticObj.Slot].shatterType == ShatterType::None)
			{
				continue;
			}

			if (Vector3i::Distance(item.Pose.Position, staticObj.Pose.Position) > SHATTER_RADIUS)
				continue;

			// Force the hit points to 0 so debris.cpp actually hides the static after shattering.
			staticObj.HitPoints = 0;

			SoundEffect(GetShatterSound(staticObj.Slot), &staticObj.Pose);
			ShatterObject(nullptr, &staticObj, -128, item.RoomNumber, 0);
			TestTriggers(staticObj.Pose.Position.x, staticObj.Pose.Position.y, staticObj.Pose.Position.z, item.RoomNumber, true);
		}
	}

	// Kills any enemies caught beneath the ball as it slams into the floor. Gated by the
	// KillEnemies property, so builders can decide whether Lara can bait the claw onto her foes.
	static void KillEnemiesOnBallImpact(const ItemInfo& item)
	{
		if (!PropertyHandler::Get(item, PropName_KillEnemies, false))
			return;

		// Collect victims first, then kill them after the loop, so g_Level.Items cannot be
		// reorganized mid-scan and the winning item reference stays valid throughout.
		std::vector<int> victims;

		for (const auto& candidate : g_Level.Items)
		{
			// Only consider intelligent creatures at all; never Lara or inert moveables.
			if (!candidate.IsCreature())
				continue;

			// Discard out-of-range IDs, the ball itself, and items with no room.
			if (!Objects.CheckID(candidate.ObjectNumber, true) ||
				candidate.Index == item.Index ||
				candidate.RoomNumber == NO_VALUE)
			{
				continue;
			}

			// Only crush live creatures still in the ball's landing zone.
			if (candidate.HitPoints <= 0 ||
				Vector3i::Distance(item.Pose.Position, candidate.Pose.Position) > ENEMY_KILL_RADIUS)
			{
				continue;
			}

			victims.push_back(candidate.Index);
		}

		for (int victimIndex : victims)
		{
			auto& victim = g_Level.Items[victimIndex];
			if (victim.HitPoints > 0)
				DoDamage(&victim, INT_MAX);
		}
	}

	// Spawns a splash and a burst of bubbles at the water surface the first frame the ball
	// submerges. Fire-and-forget: the RoomNumber update elsewhere in the control loop prevents
	// repeated triggering.
	static void SpawnWaterSplashOnSubmerge(const ItemInfo& item)
	{
		auto pointColl = GetPointCollision(item);
		int  newRoom = pointColl.GetRoomNumber();

		if (newRoom == item.RoomNumber ||
			!TestEnvironment(RoomEnvFlags::ENV_FLAG_WATER, newRoom) ||
			TestEnvironment(RoomEnvFlags::ENV_FLAG_WATER, item.RoomNumber))
		{
			return;
		}

		int waterHeight = pointColl.GetWaterTopHeight();
		if (waterHeight == NO_HEIGHT)
			return;

		SplashSetup.Position = Vector3(item.Pose.Position.x, waterHeight - 1, item.Pose.Position.z);
		SplashSetup.SplashPower = item.Animation.Velocity.y * 4;
		SplashSetup.InnerRadius = 160;
		SetupSplash(&SplashSetup, newRoom);

		// Emit a large burst of bubbles below the water surface across the ball's footprint.
		// Bubbles rise (negative Y), so they are spawned at the submerged ball's depth and stream
		// up to the surface. The count and spread scale to the ball's mass for a dramatic dive
		// entry, matching the bubble density Lara produces when falling into water from height.
		int bubbleCount = Random::GenerateInt(90, 140);
		for (int i = 0; i < bubbleCount; i++)
		{
			auto bubblePos = Vector3(
				item.Pose.Position.x + Random::GenerateFloat(-512.0f, 512.0f),
				item.Pose.Position.y + Random::GenerateFloat(-512.0f, 64.0f),
				item.Pose.Position.z + Random::GenerateFloat(-512.0f, 512.0f));

			SpawnBubble(bubblePos, newRoom, Random::GenerateFloat(16.0f, 128.0f), Random::GenerateFloat(96.0f, 256.0f));
		}
	}

	// Periodically emits bubbles during a submerged drop without saturating the shared effects pool.
	static void SpawnUnderwaterDiveBubbles(const ItemInfo& item, WreckingBallState& state)
	{
		if (!TestEnvironment(RoomEnvFlags::ENV_FLAG_WATER, item.RoomNumber))
		{
			state.UnderwaterBubbleTimer = 0;
			return;
		}

		int waterHeight = GetPointCollision(item).GetWaterTopHeight();
		if (waterHeight == NO_HEIGHT)
			return;

		if (++state.UnderwaterBubbleTimer < std::max(1, FPS / 2))
			return;

		state.UnderwaterBubbleTimer = 0;
		int bubbleCount = Random::GenerateInt(4, 6);
		for (int i = 0; i < bubbleCount; i++)
		{
			auto bubblePos = Vector3(
				item.Pose.Position.x + Random::GenerateFloat(-512.0f, 512.0f),
				item.Pose.Position.y + Random::GenerateFloat(-384.0f, 128.0f),
				item.Pose.Position.z + Random::GenerateFloat(-512.0f, 512.0f));

			SpawnBubble(bubblePos, item.RoomNumber, Random::GenerateFloat(16.0f, 128.0f), Random::GenerateFloat(256.0f, 576.0f));
		}
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

 static void EnsureWreckingBallLinks(ItemInfo& item, WreckingBallState& state, int requiredCount);

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

		int requiredLinks = std::min<int>(MAX_CHAIN_LINKS, (int)(distance / CHAIN_LINK_SPACING) + 1);
		EnsureWreckingBallLinks(item, state, requiredLinks);

		// Stack links at an absolute, fixed spacing instead of rescaling every link when the chain
		// length changes. This keeps the top of the chain anchored and only the bottom link moves, so
		// the whole stack does not shimmer when the total link count changes each frame.
		int totalLinks = (int)state.Links.size();
		if (totalLinks <= 0)
			return;

		int linkCount = std::min(totalLinks, requiredLinks);

		for (int i = 0; i < totalLinks; i++)
		{
			short linkNumber = state.Links[i];
			if (linkNumber < 0)
				continue;

			auto& link = g_Level.Items[linkNumber];
			if (i >= linkCount)
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
				ItemNewRoom(linkNumber, room);
		}
	}

	static void UpdateIdle(ItemInfo& item, WreckingBallState& state)
	{
		if (!LaraItem || state.BaseObject < 0)
			return;

		auto& anchor = g_Level.Items[state.BaseObject];

		int targetX = (LaraItem->Pose.Position.x & ~0x3FF) | 512;
		int targetZ = (LaraItem->Pose.Position.z & ~0x3FF) | 512;

		if (IsLaraWithinSearchRadius(item) &&
			IsTileTravelReachable(anchor, item.Pose.Position.x, item.Pose.Position.z, targetX, targetZ))
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

		// If Lara has left the search radius, stop pursuing and fall back to idle.
		if (!IsLaraWithinSearchRadius(item))
		{
			item.Pose.Position.x = (item.Pose.Position.x & ~0x3FF) | 512;
			item.Pose.Position.z = (item.Pose.Position.z & ~0x3FF) | 512;

			state.PhaseState = WreckingBallState::Phase::IdleAtTop;
			state.MoveAxis = 0;
			state.Timer = 0;
			state.RoamTimer = 0;
			return;
		}

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
			state.DropDelay = std::max(1, (int)(std::abs(PropertyHandler::Get(item, PropName_DropDelay, 1.0f)) * FPS));
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

		// If Lara just came within the search radius and is reachable, abandon roaming and seek her.
		int laraX = (LaraItem->Pose.Position.x & ~0x3FF) | 512;
		int laraZ = (LaraItem->Pose.Position.z & ~0x3FF) | 512;
		if (IsLaraWithinSearchRadius(item) &&
			IsTileTravelReachable(anchor, item.Pose.Position.x, item.Pose.Position.z, laraX, laraZ))
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
			float dropSpeed = std::max(0.25f, std::abs(PropertyHandler::Get(item, PropName_DropSpeed, 1.0f)));
			item.Animation.Velocity.y = 6.0f * dropSpeed;
			item.Pose.Position.y += item.Animation.Velocity.y;
		}
	}

	static void UpdateDropping(ItemInfo& item, WreckingBallState& state)
	{
		float dropSpeed = std::max(0.25f, std::abs(PropertyHandler::Get(item, PropName_DropSpeed, 1.0f)));
		item.Animation.Velocity.y += 24.0f * dropSpeed;
		item.Pose.Position.y += item.Animation.Velocity.y;

		// Continuously settle the ball toward the floor normal so it comes to rest flush on slopes.
		AlignBallToSurface(item);

		// Create a splash if the ball has just submerged into a water room.
		SpawnWaterSplashOnSubmerge(item);

		// Emit a dense bubble column every frame the ball is underwater during the drop.
		SpawnUnderwaterDiveBubbles(item, state);

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
			// Crush any shatterable statics and enemies beneath the ball as it slams into the floor.
			ShatterStaticsOnBallImpact(item);
			KillEnemiesOnBallImpact(item);

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
		state.BaseObject = FindOrCreateWreckingBallAnchor(item);
		RestoreOrAssignWreckingBallLinks(item, state);

		// Enable interpolation on every link so the renderer smoothly eases their motion between
		// frames. Spawned items default to DisableInterpolation = true, which would otherwise make the
		// chain snap to new positions instead of gliding, causing visible jitter at high frame rates.
		for (short link : state.Links)
		{
			if (link >= 0)
			{
				g_Level.Items[link].DisableInterpolation = false;
				SetItemInvisible(g_Level.Items[link]);
			}
		}

		// Hide every chain link until the trap is activated, so an untriggered wrecking ball parked in
		// the level does not draw its chain. UpdateChain() reveals the links once the trap goes active.

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
		StoreWreckingBallState(item, state);
	}

	// ---------------------------------------------------------------------
	// Control
	// ---------------------------------------------------------------------

	void ControlWreckingBall(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];
		auto [stateIterator, isNewState] = WreckingBallStates.try_emplace(itemNumber);
		auto& state = stateIterator->second;

		if (JustLoaded || isNewState)
		{
			RestoreWreckingBallState(item, state);
			RestoreOrAssignWreckingBallLinks(item, state);
		}

		if (state.BaseObject >= 0 && state.BaseObject < g_Level.Items.size())
		{
			auto& anchor = g_Level.Items[state.BaseObject];
			anchor.ItemFlags[0] = itemNumber;
			anchor.ItemFlags[1] = WRECKINGBALL_COMPANION_MARKER;
		}

		// Re-initialize alarm flash timing after savegame load if needed.
		if (state.AlarmFlashPeriod <= 0)
		{
			float blinkSpeed = PropertyHandler::Get(item, PropName_AlarmLightBlinkSpeed, ALARM_LIGHT_BLINK_SPEED_DEFAULT);
			state.AlarmFlashPeriod = std::max(2, (int)(Random::GenerateInt(15, 30) / blinkSpeed));
			state.AlarmFlashOn = std::clamp(Random::GenerateInt(2, 20), 1, state.AlarmFlashPeriod - 1);
		}

		// Bail out if the required companion objects are missing.
		if (state.BaseObject < 0)
		{
			StoreWreckingBallState(item, state);
			return;
		}

		// Hide the chain (and skip running the ball) until the trap is triggered/activated, matching
		// how other traps gate their control on TriggerActive(). This prevents the chain from being
		// drawn while the wrecking ball sits idle and untriggered in the level.
		if (!TriggerActive(&item))
		{
			for (short link : state.Links)
			{
				if (link >= 0)
					SetItemInvisible(g_Level.Items[link]);
			}
			StoreWreckingBallState(item, state);
			return;
		}

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
		StoreWreckingBallState(item, state);
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

	static void EnsureWreckingBallLinks(ItemInfo& item, WreckingBallState& state, int requiredCount)
	{
		requiredCount = std::clamp(requiredCount, 0, MAX_CHAIN_LINKS);

		for (int i = 0; i < requiredCount; i++)
		{
			if (i < (int)state.Links.size())
			{
				short linkNumber = state.Links[i];
				if (linkNumber >= 0 &&
					linkNumber < (int)g_Level.Items.size() &&
					g_Level.Items[linkNumber].ObjectNumber == ID_WRECKINGBALL_CHAIN)
				{
					continue;
				}
			}

			short linkNumber = SpawnItem(item, ID_WRECKINGBALL_CHAIN);
			if (linkNumber < 0)
				break;

			auto& link = g_Level.Items[linkNumber];
			link.ItemFlags[0] = item.Index;
			link.ItemFlags[1] = WRECKINGBALL_COMPANION_MARKER;
			link.ItemFlags[2] = (short)i;
			link.DisableInterpolation = false;
			SetItemInvisible(link);

			if (i >= (int)state.Links.size())
				state.Links.resize(i + 1, -1);

			state.Links[i] = linkNumber;
		}
	}
}
