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
#include "Sound/sound.h"
#include "Specific/Input/Input.h"
#include "Specific/level.h"

#include <algorithm>
#include <unordered_map>

using namespace TEN::Animation;
using namespace TEN::Effects::Environment;

namespace TEN::Entities::Traps
{
	constexpr auto WRECKINGBALL_STATE_IDLE = 0;
	constexpr auto WRECKINGBALL_STATE_DROP = 1;
	constexpr auto WRECKINGBALL_STATE_ATTACK = 2;
		constexpr auto WRECKINGBALL_STATE_RAISE = 3;

	constexpr int MOVE_SPEED = 64;

	// Distance from the anchor (ceiling) to the ball's center when the ball is fully raised.
	constexpr int BALL_HANG_OFFSET_Y = 1644;

	// Distance from the ball's center to the ball's top, where the chain is attached.
	constexpr int BALL_TOP_OFFSET_Y = CLICK(1);

	constexpr auto SPOTLIGHT_ANCHOR_OFFSET_Y = 512.0f;

	// The chain mesh's base (unscaled) vertical length in world units.
		constexpr float CHAIN_LENGTH = 3500.0f;

	// Downward facing spotlight
	constexpr auto SPOTLIGHT_DOWN_R = 0.7f;
	constexpr auto SPOTLIGHT_DOWN_G = 0.0f;
	constexpr auto SPOTLIGHT_DOWN_B = 0.0f;
	constexpr auto SPOTLIGHT_DOWN_INTENSITY = 0.5f;
	constexpr auto SPOTLIGHT_DOWN_RADIUS_RATIO = 3.0f;
	constexpr auto SPOTLIGHT_DOWN_FALLOFF_RATIO = 4.0f;
	constexpr auto SPOTLIGHT_DOWN_HASH_OFFSET = 1000;

	// Alarm spotlight
	constexpr auto SPOTLIGHT_ALARM_R = 1.0f;
	constexpr auto SPOTLIGHT_ALARM_G = 0.0f;
	constexpr auto SPOTLIGHT_ALARM_B = 0.0f;
	constexpr auto SPOTLIGHT_ALARM_INTENSITY = 2.0f;
	constexpr auto SPOTLIGHT_ALARM_RADIUS_RATIO = 0.6f;
	constexpr auto SPOTLIGHT_ALARM_ROTATE_SPEED = 0.7f;
	constexpr auto SPOTLIGHT_ALARM_DIST = 8192.0f;
	constexpr auto SPOTLIGHT_ALARM_FALLOFF_RATIO = 0.5f;
	constexpr auto SPOTLIGHT_ALARM_HASH_OFFSET = 500;

	auto SPOTLIGHT_ALARM_FLASH_PERIOD = Random::GenerateInt(15, 30);
	auto SPOTLIGHT_ALARM_FLASH_ON = std::clamp(Random::GenerateInt(2, 20), 1, SPOTLIGHT_ALARM_FLASH_PERIOD - 1);

	struct WreckingBallState
	{
		enum class Phase
		{
			IdleAtTop,
			Moving,
			PreparingDrop,
			Dropping,
			WinchUp
		};

		Phase PhaseState = Phase::IdleAtTop;
		int   MoveAxis = 0;
		int   Timer = 0;
		int   DropDelay = 0;

		short BaseObject = -1;
		short ChainObject = -1;

		int TargetX = 0;
		int TargetZ = 0;

		float RotatingSpotlightAngle = 0.0f;
		float PreviousSpotlightAngle = 0.0f;
		int AlarmFlashTimer = 0;

	};

	static std::unordered_map<short, WreckingBallState> WreckingBallStates;

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

		auto pointColl = GetPointCollision(item);

		auto anchors = FindAllItems(ID_WRECKINGBALL_ANCHOR);
		if (!anchors.empty())
			state.BaseObject = anchors[0];

		auto chains = FindAllItems(ID_WRECKINGBALL_CHAIN);
		if (!chains.empty())
			state.ChainObject = chains[0];

		if (state.BaseObject < 0 || state.ChainObject < 0)
		{
			TENLog(
				"WreckingBall ERROR: Missing required objects. Please place add to map"
				"Expected Anchor ID=" + std::to_string(ID_WRECKINGBALL_ANCHOR) +
				" Chain ID=" + std::to_string(ID_WRECKINGBALL_CHAIN) +
				" | Found Anchor ItemIndex=" + std::to_string(state.BaseObject) +
				" Chain ItemIndex=" + std::to_string(state.ChainObject),
				LogLevel::Error);

			item.Flags |= IFLAG_INVISIBLE;
			item.Status = ITEM_NOT_ACTIVE;
			return;
		}

		item.Pose.Position.y = pointColl.GetCeilingHeight() + BALL_HANG_OFFSET_Y;

		if (pointColl.GetRoomNumber() != item.RoomNumber)
			ItemNewRoom(itemNumber, pointColl.GetRoomNumber());

		state.TargetX = item.Pose.Position.x;
		state.TargetZ = item.Pose.Position.z;
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

		int damage = (item.Animation.Velocity.y > 0.0f) ? 96 : 0;

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

	// ---------------------------------------------------------------------
	// Helpers
	// ---------------------------------------------------------------------

	static bool IsCeilingSafeForAnchor(const ItemInfo& anchor, int newX, int newZ)
	{
		int   y = anchor.Pose.Position.y;
		short room = anchor.RoomNumber;

		int currentCeiling = GetCeiling(
			GetFloor(anchor.Pose.Position.x, y, anchor.Pose.Position.z, &room),
			anchor.Pose.Position.x, y, anchor.Pose.Position.z);

				room = anchor.RoomNumber;
		int newCeiling = GetCeiling(
			GetFloor(newX, y, newZ, &room),
			newX, y, newZ);

		if (newCeiling == NO_HEIGHT)
			return false;

		// Allow the ball to track Lara across tiles with ceilings at or above the anchor's ceiling.
		// The ball is suspended from a rail, so it can move under taller (lower) ceilings freely.
		// Only tiles whose ceiling rises above the anchor (blocking the rail) are rejected.
		return (newCeiling <= currentCeiling);
	}

		static bool FindClosestReachableTile(const ItemInfo& anchor, int& outX, int& outZ)
	{
		if (!LaraItem)
			return false;

				// Search outward from the ball's own position so the fallback repositions the ball to a nearby
		// reachable tile, rather than hopping toward Lara across the room. Among reachable tiles, pick
		// the one closest to Lara so the ball keeps pushing toward her instead of drifting to a corner.
		int laraX = (LaraItem->Pose.Position.x & ~0x3FF) | 512;
		int laraZ = (LaraItem->Pose.Position.z & ~0x3FF) | 512;

		int ballX = anchor.Pose.Position.x;
		int ballZ = anchor.Pose.Position.z;

		int bestX = 0;
		int bestZ = 0;
		int bestDist = INT_MAX;

		for (int dx = -4; dx <= 4; dx++)
		{
			for (int dz = -4; dz <= 4; dz++)
			{
				int testX = ((ballX + dx * CLICK(1)) & ~0x3FF) | 512;
				int testZ = ((ballZ + dz * CLICK(1)) & ~0x3FF) | 512;

				if (!IsCeilingSafeForAnchor(anchor, testX, testZ))
					continue;

				int testDist = std::abs(laraX - testX) + std::abs(laraZ - testZ);
				if (testDist < bestDist)
				{
					bestDist = testDist;
					bestX = testX;
					bestZ = testZ;
				}
			}
		}

		if (bestDist == INT_MAX)
			return false;

		outX = bestX;
		outZ = bestZ;
		return true;
	}

	static void SpawnAnchorSpotlights(const ItemInfo& anchor, const ItemInfo& ball, float& angle, int& flashTimer)
	{
		auto origin = Vector3(
			(float)anchor.Pose.Position.x,
			(float)anchor.Pose.Position.y + SPOTLIGHT_ANCHOR_OFFSET_Y,
			(float)anchor.Pose.Position.z);

		// a) Static downward spotlight — distance scales to floor below ball.
		{
			int   floorY = GetPointCollision(ball).GetFloorHeight();
			float dynDist = (float)(floorY - anchor.Pose.Position.y);
			if (dynDist < 256.0f)
				dynDist = 256.0f; // FAILSAFE: Prevent zero or negative distance.

			float dynRadius = dynDist * SPOTLIGHT_DOWN_RADIUS_RATIO;
			float dynFalloff = dynDist * SPOTLIGHT_DOWN_FALLOFF_RATIO;

			Vector3 dir(0.0f, 1.0f, 0.0f);
			Color color(
				SPOTLIGHT_DOWN_R * SPOTLIGHT_DOWN_INTENSITY,
				SPOTLIGHT_DOWN_G * SPOTLIGHT_DOWN_INTENSITY,
				SPOTLIGHT_DOWN_B * SPOTLIGHT_DOWN_INTENSITY);

				int hash = anchor.Index + SPOTLIGHT_DOWN_HASH_OFFSET;

			SpawnDynamicSpotLight(origin, dir, color, dynRadius, dynFalloff, dynDist, true, hash);
		}

		// b) Rotating alarm spotlight — fixed large distance to sweep room walls.
		{
			angle += SPOTLIGHT_ALARM_ROTATE_SPEED;
			if (angle > PI_MUL_2)
				angle -= PI_MUL_2;

			const auto& room = g_Level.Rooms[anchor.RoomNumber];
			float roomSizeX = (float)room.XSize * BLOCK(1);
			float roomSizeZ = (float)room.ZSize * BLOCK(1);
			float alarmDist = std::max(roomSizeX, roomSizeZ);

			float alarmRadius = alarmDist * SPOTLIGHT_ALARM_RADIUS_RATIO;
			float alarmFalloff = alarmDist * SPOTLIGHT_ALARM_FALLOFF_RATIO;

			Vector3 dir(sin(angle), 0.0f, cos(angle));
			dir.Normalize();

			Color color(
				SPOTLIGHT_ALARM_R * SPOTLIGHT_ALARM_INTENSITY,
				SPOTLIGHT_ALARM_G * SPOTLIGHT_ALARM_INTENSITY,
				SPOTLIGHT_ALARM_B * SPOTLIGHT_ALARM_INTENSITY);

				int hash = anchor.Index + SPOTLIGHT_ALARM_HASH_OFFSET;

				flashTimer = (flashTimer + 1) % SPOTLIGHT_ALARM_FLASH_PERIOD;

			if (flashTimer < SPOTLIGHT_ALARM_FLASH_ON)
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

		SpawnAnchorSpotlights(anchor, item, state.RotatingSpotlightAngle, state.AlarmFlashTimer);
	}

	static void UpdateChain(ItemInfo& item, WreckingBallState& state)
	{
		if (state.ChainObject < 0 || state.BaseObject < 0)
			return;

		auto& chain = g_Level.Items[state.ChainObject];
		auto& anchor = g_Level.Items[state.BaseObject];

		chain.Pose.Position.x = anchor.Pose.Position.x;
		chain.Pose.Position.z = anchor.Pose.Position.z;
		chain.Pose.Position.y = anchor.Pose.Position.y;

		// Stretch the chain from the anchor (at the ceiling) down to the top of the ball.
		// The chain's top stays anchored while its bottom tracks the ball as it drops and rises.
		float ballTopY = (float)item.Pose.Position.y - (float)BALL_TOP_OFFSET_Y;
		float distance = ballTopY - (float)anchor.Pose.Position.y;
		if (distance < 0.0f)
			distance = 0.0f;

		float scaleY = distance / CHAIN_LENGTH;
		if (scaleY < 0.1f)
			scaleY = 0.1f;

		chain.Pose.Scale.y = scaleY;

		short room = chain.RoomNumber;
		GetFloor(chain.Pose.Position.x, chain.Pose.Position.y, chain.Pose.Position.z, &room);
		if (room != chain.RoomNumber)
			ItemNewRoom(state.ChainObject, room);
	}

	static void UpdateIdle(ItemInfo& item, WreckingBallState& state)
	{
		if (!LaraItem || state.BaseObject < 0)
			return;

		auto& anchor = g_Level.Items[state.BaseObject];

		int targetX = (LaraItem->Pose.Position.x & ~0x3FF) | 512;
		int targetZ = (LaraItem->Pose.Position.z & ~0x3FF) | 512;

		if (!IsCeilingSafeForAnchor(anchor, targetX, targetZ))
			return;

		state.TargetX = targetX;
		state.TargetZ = targetZ;
		state.PhaseState = WreckingBallState::Phase::Moving;
		state.MoveAxis = 0;
		state.Timer = 0;
	}

		static void UpdateHorizontalMovement(ItemInfo& item, WreckingBallState& state)
	{
		if (!LaraItem || state.BaseObject < 0)
		{
			state.PhaseState = WreckingBallState::Phase::IdleAtTop;
			state.MoveAxis = 0;
			state.Timer = 0;
			return;
		}

		auto& anchor = g_Level.Items[state.BaseObject];

		// Re-home on Lara's current tile every frame so the ball tracks her as she moves.
		int laraX = (LaraItem->Pose.Position.x & ~0x3FF) | 512;
		int laraZ = (LaraItem->Pose.Position.z & ~0x3FF) | 512;

		if (!IsCeilingSafeForAnchor(anchor, laraX, laraZ))
		{
			// Lara left the traversable region; park the ball back at the top until she returns.
			state.PhaseState = WreckingBallState::Phase::IdleAtTop;
			state.MoveAxis = 0;
			state.Timer = 0;
			return;
		}

		int dx = laraX - item.Pose.Position.x;
		int dz = laraZ - item.Pose.Position.z;

		// Drop once we are alongside Lara's tile; we can never reach her exact center because she
		// occupies it, so a proximity test is used instead of pursuing her tile center directly.
		if (std::abs(dx) <= MOVE_SPEED && std::abs(dz) <= MOVE_SPEED)
		{
			StopSoundEffect(SFX_TR5_BASE_CLAW_MOTOR_B_LOOP);
			SoundEffect(SFX_TR5_BASE_CLAW_MOTOR_C, &item.Pose);

			state.TargetX = laraX;
			state.TargetZ = laraZ;
			state.PhaseState = WreckingBallState::Phase::PreparingDrop;
			state.DropDelay = 30;
			state.MoveAxis = 0;
			state.Timer = 0;
			return;
		}

		bool movedThisFrame = false;

		// Move along the dominant axis first, then the other. The ball rides a ceiling rail, so only
		// the ceiling matters; there is no floor/radius occupancy check, matching the original TR5.
		auto tryMoveAxis = [&](int axis)
			{
				if (axis == 1)
				{
					int step = std::clamp(dx, -MOVE_SPEED, MOVE_SPEED);
					if (step == 0)
						return false;

					int newX = item.Pose.Position.x + step;
					int newZ = item.Pose.Position.z;

					if (!IsCeilingSafeForAnchor(anchor, newX, newZ))
						return false;

					item.Pose.Position.x = newX;
					SoundEffect(SFX_TR5_BASE_CLAW_MOTOR_B_LOOP, &item.Pose);
					return true;
				}

				int step = std::clamp(dz, -MOVE_SPEED, MOVE_SPEED);
				if (step == 0)
					return false;

				int newZ = item.Pose.Position.z + step;

				if (!IsCeilingSafeForAnchor(anchor, item.Pose.Position.x, newZ))
					return false;

				item.Pose.Position.z = newZ;
				SoundEffect(SFX_TR5_BASE_CLAW_MOTOR_B_LOOP, &item.Pose);
				return true;
			};

		// Choose the axis with the greater distance to Lara each time we start a new approach so the
		// ball homes toward her instead of jittering on an arbitrary first axis.
		if (state.MoveAxis == 0)
			state.MoveAxis = (std::abs(dx) > std::abs(dz)) ? 1 : 2;

		movedThisFrame = tryMoveAxis(state.MoveAxis);
		if (!movedThisFrame)
			movedThisFrame = tryMoveAxis(state.MoveAxis == 1 ? 2 : 1);

		if (!movedThisFrame)
		{
			state.Timer++;

			// We are stuck (e.g. a blocking ceiling edge). Move to the nearest reachable tile that is
			// closest to Lara. If none is reachable, drop in place rather than drifting to a corner.
			if (state.Timer > 10)
			{
				int bestX, bestZ;
				if (FindClosestReachableTile(anchor, bestX, bestZ))
				{
					state.TargetX = bestX;
					state.TargetZ = bestZ;
					state.MoveAxis = 0;
					state.Timer = 0;
					return;
				}

				state.PhaseState = WreckingBallState::Phase::PreparingDrop;
				state.DropDelay = 30;
				state.MoveAxis = 0;
				state.Timer = 0;
				return;
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
				state.PhaseState = WreckingBallState::Phase::WinchUp;
			}
		}
	}

	static void UpdateWinchUp(ItemInfo& item, WreckingBallState& state)
	{
		if (state.BaseObject < 0)
		{
			state.PhaseState = WreckingBallState::Phase::IdleAtTop;
			item.Animation.Velocity.y = 0;
			return;
		}

		auto& anchor = g_Level.Items[state.BaseObject];
		int   targetY = anchor.Pose.Position.y + BALL_HANG_OFFSET_Y;

		item.Animation.Velocity.y -= 3;
		item.Pose.Position.y += item.Animation.Velocity.y;

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
				state.PhaseState = WreckingBallState::Phase::IdleAtTop;
			}
		}
		else
		{
			SoundEffect(SFX_TR5_BASE_CLAW_WINCH_UP_LOOP, &item.Pose);
		}
	}

	void ControlWreckingBall(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];
		auto& state = WreckingBallStates[itemNumber];

		// Bail out if the required companion objects are missing.
		if (state.BaseObject < 0 || state.ChainObject < 0)
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
}

