#include "framework.h"
#include "Objects/TR5/Entity/tr5_gunship.h"

#include "Scripting/Internal/TEN/Objects/Creature/Creature.h"
#include "Scripting/Internal/TEN/Properties/PropertyHandler.h"
#include "Scripting/Internal/TEN/Properties/PropertyNames.h"
#include "Game/Animation/Animation.h"
#include "Game/camera.h"
#include "Game/collision/Los.h"
#include "Game/collision/collide_item.h"
#include "Game/collision/collide_room.h"
#include "Game/control/box.h"
#include "Game/control/control.h"
#include "Game/control/lot.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/room.h"
#include "Game/Lara/lara.h"
#include "Game/Lara/lara_helpers.h"
#include "Game/control/los.h"
#include "Math/Geometry.h"
#include "Sound/sound.h"
#include "Game/Setup.h"
#include "Game/effects/debris.h"
#include "Specific/level.h"

#include "Game/misc.h"
#include "Game/effects/tomb4fx.h"

using namespace TEN::Animation;
using namespace TEN::Collision::Los;
using namespace TEN::Math;
using namespace TEN::Scripting::Properties;

namespace TEN::Entities::Creatures::TR5
{
	// Shared runtime state (RomanStatue pattern: a single global instance, like RomanStatueData).
	// Holds only things that are fine to share across multiple helicopters (escape flight, LOS cache,
	// orbit continuity, fire warm-up). Per-heli state lives in the ItemFlags (see table below).
	struct GunshipData
	{
		// Orbit/evade: +1 / -1 = direction of travel while evading.
		int Direction = 1;
		bool Initialized = false;

		// Auto-escape: when active, the escape target acts as the move target.
		bool Active = false;
		Vector3 TargetPos = Vector3::Zero;
		int Frames = 0;

		// Cached LOS result heli->shoot target + last test frame (expensive, cached ~1x/sec).
		int LastTestFrame = -1000000; // Sentinel: erster Aufruf testet immer (kein 1s-Blindefenster).
		bool Clear = true;            // true = freie Sicht (keine Geometrie dazwischen).

		// Fire warm-up + first-frame orientation snap (was ItemFlags[0]).
		int FireFrameCounter = 0;

		// Reset escape state (target gone, fresh timeout window).
		void ResetEscape()
		{
			Active = false;
			TargetPos = Vector3::Zero;
			Frames = 0;
		}
	};

	static GunshipData GunShip;

	enum class GunshipAxis : short
	{
		ZAxis = 0,		// like in TR5 Red alert.
		XAxis = 1,		// Free flying and shooting.		
	};

	// Enum for the helicopter state
	enum class GunShipState : short
	{
		FOLLOW = 0,
		IDLE = 1,
		EVADE_NEAR = 2,
		ESCAPE = 3
	};

	// ItemFlags allocation (per heli); [0] and [3] are FREE - [0] is reserved for external code.
	// [1] = CurrentPitch (Rad x 1000)          [2] = CurrentBankAngle (Rad x 1000)
	// [4] = PrevState                          [5] = InertiaTimer
	// [6] = CurrentYSpeed (Units/Frame x 100)  [7] = EvadeActive (0/1)

	void InitializeGunShip(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];
		InitializeCreature(itemNumber);
		item->ItemFlags[3] = PropertyHandler::Get(*item, PropName_StopMovement, false);
	}

	// Behavior constants
	constexpr int FIRE_RATE = 2;
	constexpr int GUNSHIP_DAMAGE = 20; // Damage dealt by gunship to Lara when shooting

	constexpr float MOVEMENT_LERP_SPEED = 4.0f;
	constexpr int INERTIA_FRAMES = 25;
	constexpr float PITCH_LERP_SPEED = 5.0f;
	constexpr float BANK_LERP_SPEED = 3.0f;

	constexpr int MAX_PITCH_DEG = 20;
	constexpr int MAX_BANK_DEG = 15;
	constexpr float MAX_MOVE_SPEED = 64.0f;
	constexpr float FLY_UP_SPEED = 40.0f;
	constexpr float FLY_DOWN_SPEED = 40.0f;

	constexpr int SECTOR_SIZE = 1024;
	constexpr float EVADE_RAISE_HEIGHT = SECTOR_SIZE * 1.5f; // ~1.5 BLOCK, climb while evading so the tail does not touch the ground
	constexpr float HOVER_HEIGHT_OFFSET = SECTOR_SIZE * 2.0f; // Heli hovers ~1.5 sectors above the target so it can aim downward when firing
	constexpr float EVADE_OVERFLY_HEIGHT = SECTOR_SIZE * 3.0f; // Overfly: heli climbs high enough over Lara that the tail stays clear
	constexpr float MOVE_TARGET_REACH_RADIUS = SECTOR_SIZE * 0.5f; // moveTargetPos: one-shot radius (horizontal), after which the escape/move-target state is cleared
	constexpr float ESCAPE_EXCLUDE_RADIUS = SECTOR_SIZE * 2.0f; // Exclusion radius: the last failed escape target is not re-chosen on re-search
	constexpr int MAX_ESCAPE_FRAMES = 280; // Auto-escape: max frames before the escape flight is given up (combat resumes)
	constexpr int LOS_TEST_INTERVAL = FPS; // ~1 second: throttling of the expensive heli->shoot-target LOS test (applies to all distances)
	constexpr auto DAMAGE_LIMITER = 2.0f;

	// Helper: determines the current state based on distance and collisions
	GunShipState DetermineGunShipState(const ItemInfo& item, float horizontalDistance, bool hasMoveTargetPos)
	{
		int minDistance = PropertyHandler::Get(item, PropName_ShootTargetDistance, 3);
		minDistance = minDistance * SECTOR_SIZE;
		int maxShotsRange = minDistance + SECTOR_SIZE;

		if (hasMoveTargetPos)
		{
			// Escape flight: own state (free movement, Y free within room bounds).
			return GunShipState::ESCAPE;
		}

		if (horizontalDistance < minDistance)
			return GunShipState::EVADE_NEAR;

		// Within firing range: always hover steadily, regardless of evade flag or blockage.
		if (horizontalDistance < maxShotsRange)
			return GunShipState::IDLE;

		// TEST (FOLLOW): wall check temporarily disabled - the heli flies through walls in the FOLLOW branch
		// (intentional, for the test). Rebuild the wall check later.
		return GunShipState::FOLLOW;
	}

	// Helper: returns the room a probe position lies in: the current room or a directly
	// connected neighbor room (via portal, from NeighborRoomNumbers). NO_VALUE = outside the
	// valid, connected room geometry (e.g. horizon / open area without a connected room).
	int ResolveProbeRoom(const Vector3& probePos, int startRoomNumber)
	{
		Vector3i probe(probePos.x, probePos.y, probePos.z);
		if (IsPointInRoom(probe, startRoomNumber))
			return startRoomNumber;

		const auto& room = g_Level.Rooms[startRoomNumber];
		for (int neighborRoomNumber : room.NeighborRoomNumbers)
		{
			if (IsPointInRoom(probe, neighborRoomNumber))
				return neighborRoomNumber;
		}

		return NO_VALUE;
	}

	// Helper: direction-independent collision footprint check.
	// Checks whether the helicopter's ground-plane footprint, offset by `displacement`,
	// hits wall sectors, raised squares / lowered ceilings (clearance).
	// A grid of samples over the entire footprint area (corners, edge and interior points)
	// is checked so that inner walls are detected too - not just the 4 corners.
	// Every point must also lie in a valid, connected room (no horizon, no unconnected exterior).
	bool CheckFootprintCollision(const ItemInfo& item, const Vector3& displacement)
	{
		auto& heliFrame = GetFrame(item);
		float bottomMeshY = item.Pose.Position.y + heliFrame.BoundingBox.Y1 + displacement.y;
		float topMeshY = item.Pose.Position.y + heliFrame.BoundingBox.Y2 + displacement.y;
		int bandHeight = (int)(topMeshY - bottomMeshY);
		float midY = (bottomMeshY + topMeshY) / 2.0f;

		float yawCos = cosf(item.Pose.Orientation.y);
		float yawSin = sinf(item.Pose.Orientation.y);
		float posX = item.Pose.Position.x;
		float posZ = item.Pose.Position.z;

		// Grid over the footprint area (local, relative to the pose origin): 4x4 = 16 samples.
		constexpr int FOOTPRINT_GRID = 4;
		float xSpan = (float)heliFrame.BoundingBox.X2 - (float)heliFrame.BoundingBox.X1;
		float zSpan = (float)heliFrame.BoundingBox.Z2 - (float)heliFrame.BoundingBox.Z1;

		for (int gx = 0; gx < FOOTPRINT_GRID; gx++)
		{
			for (int gz = 0; gz < FOOTPRINT_GRID; gz++)
			{
				float fx = (float)gx / (float)(FOOTPRINT_GRID - 1);
				float fz = (float)gz / (float)(FOOTPRINT_GRID - 1);
				float localX = (float)heliFrame.BoundingBox.X1 + fx * xSpan;
				float localZ = (float)heliFrame.BoundingBox.Z1 + fz * zSpan;

				// Yaw rotation (around Y) + world translation + proposed displacement.
				float worldX = posX + (localX * yawCos + localZ * yawSin) + displacement.x;
				float worldZ = posZ + (-localX * yawSin + localZ * yawCos) + displacement.z;

				Vector3 probePos(worldX, midY, worldZ);

				// Point must lie in a valid, connected room (no horizon, no unconnected exterior).
				int probeRoom = ResolveProbeRoom(probePos, item.RoomNumber);
				if (probeRoom == NO_VALUE)
					return true;

				auto pointColl = GetPointCollision(probePos, probeRoom);

				if (pointColl.IsWall())
					return true;

				// Exact penetration: footprint bottom below the floor or top above the ceiling.
				int floorHeight = pointColl.GetFloorHeight();
				int ceilingHeight = pointColl.GetCeilingHeight();
				if (floorHeight != NO_HEIGHT && floorHeight < bottomMeshY)
					return true;
				if (ceilingHeight != NO_HEIGHT && ceilingHeight > topMeshY)
					return true;

				if (abs(ceilingHeight - floorHeight) <= bandHeight)
					return true;
			}
		}

		return false;
	}

	// Sub-stepped collision check: checks the footprint along the
	// full displacement path (x+y+z) in small sub-steps. true = clear.
	bool SweptFootprintClear(const ItemInfo& item, const Vector3& displacement)
	{
		float stepLength = displacement.Length();
		if (stepLength < 1.0f)
			return !CheckFootprintCollision(item, displacement);

		// Sub-steps, max. SECTOR/4, so no wall is flown through during fast movement.
		constexpr float MAX_SUBSTEP = (float)SECTOR_SIZE * 0.25f;
		int steps = (int)(stepLength / MAX_SUBSTEP + 1.0f);

		Vector3 dir = displacement * (1.0f / stepLength);
		for (int i = 1; i <= steps; i++)
		{
			Vector3 offset = dir * (stepLength * ((float)i / (float)steps));
			if (CheckFootprintCollision(item, offset))
				return false;
		}

		return true;
	}

	// Helper: finds a free flight direction near the preferred direction (XZ plane).
	// Tests five symmetric candidates (0, +/-45, +/-90 degrees) around preferredDir and picks the
	// collision-free direction closest to the preferred direction.
	// A continuity bias prefers the last chosen evade side (prevents rocking back and forth).
	// true = free direction found (in outDir), false = no direction free.
	bool FindBestAvoidanceDirection(const ItemInfo& item, const Vector3& preferredDir, float probeDistance, Vector3& outDir)
	{
		// Five candidates in degrees, symmetric around the preferred direction.
		const float candidateAngles[5] = { 0.0f, 45.0f, -45.0f, 90.0f, -90.0f };
		const float RAD_PER_DEG = 0.0174532925f; // PI / 180

		float bestScore = 1000.0f;
		Vector3 bestDir = preferredDir;
		bool found = false;

		int orbitSide = (GunShip.Direction > 0) ? 1 : -1;

		for (int i = 0; i < 5; i++)
		{
			float theta = candidateAngles[i] * RAD_PER_DEG;
			float cosT = cosf(theta);
			float sinT = sinf(theta);

			// 2D rotation around Y (XZ components only).
			Vector3 cand(preferredDir.x * cosT - preferredDir.z * sinT, 0.0f, preferredDir.x * sinT + preferredDir.z * cosT);
			cand.Normalize();

			if (!SweptFootprintClear(item, cand * probeDistance))
				continue;

			// Scoring: distance to the preferred direction, with a continuity bias toward the last evade side.
			float score = fabsf(candidateAngles[i]);
			int candidateSide = (candidateAngles[i] > 0.0f) ? 1 : ((candidateAngles[i] < 0.0f) ? -1 : 0);
			if (GunShip.Initialized && candidateSide != 0 && candidateSide != orbitSide)
				score += 0.5f;

			if (score < bestScore)
			{
				bestScore = score;
				bestDir = cand;
				found = true;
				if (candidateSide != 0)
					GunShip.Direction = candidateSide;
			}
		}

		if (!found)
			return false;

		GunShip.Initialized = true;
		outDir = bestDir;
		return true;
	}

	// Helper: finds an escape target in valid, connected room geometry that is at least
	// minEscapeDist from the shoot target and offers sufficient flight clearance
	// (including ceiling height). Used on blockage in EVADE_NEAR/ESCAPE to fly out of a dead end.
	// Candidates within excludeRadius around excludePos are skipped so a failed
	// escape target is not re-chosen (prevents the stuck loop).
	// true = escape target found (in outPos), false = no valid point.
	bool FindEscapeTarget(const ItemInfo& item, const Vector3& shootTargetPos, float minEscapeDist, const Vector3& excludePos, float excludeRadius, Vector3& outPos)
	{
		Vector3 heliPos = item.Pose.Position.ToVector3();
		float bestScore = 10000000.0f;
		bool found = false;

		// Ring of candidates around the shoot target (horizontal), at the current heli height.
		constexpr int DIRS = 12;
		constexpr float TWO_PI = 6.28318530718f;

		for (int k = 0; k < DIRS; k++)
		{
			float angle = ((float)k / (float)DIRS) * TWO_PI;
			Vector3 cand(
				shootTargetPos.x + cosf(angle) * minEscapeDist,
				heliPos.y,
				shootTargetPos.z + sinf(angle) * minEscapeDist);

			Vector3 disp = cand - heliPos;
			if (disp.Length() < (float)SECTOR_SIZE * 2.0f)
				continue; // too close to the heli, no useful escape point

			// Destination must offer clearance + connected geometry (ResolveProbeRoom + walls/ceiling).
			if (CheckFootprintCollision(item, disp))
				continue;

			// Path must be clear at several intermediate points so the target is realistically reachable.
			// (Only checking the path middle would miss blockages in the last stretch before the target.)
			if (CheckFootprintCollision(item, disp * 0.33f) ||
				CheckFootprintCollision(item, disp * 0.66f) ||
				CheckFootprintCollision(item, disp * 0.9f))
				continue;

			// LOS: no geometry (wall) may lie between the heli and the escape target - otherwise the
			// escape target is unreachable (flying through walls). FindEscapeTarget then picks the next candidate.
			auto losOrigin = GameVector(heliPos, item.RoomNumber);
			auto losTarget = GameVector(cand, item.RoomNumber);
			if (!LOS(&losOrigin, &losTarget))
				continue;

			// Do not re-choose the last failed escape target (otherwise the same stuck loop).
			// Compare horizontally (XZ) only, since the ring lies at the heli height.
			if (excludePos != Vector3::Zero)
			{
				float exx = cand.x - excludePos.x;
				float exz = cand.z - excludePos.z;
				if (sqrtf(exx * exx + exz * exz) < excludeRadius)
					continue;
			}

			// Nearest valid point (least flight distance) preferred.
			float score = disp.Length();
			if (score < bestScore)
			{
				bestScore = score;
				outPos = cand;
				found = true;
			}
		}

		return found;
	}

	// Cached LOS test heli->shoot target (geometry check: is a wall in between?).
	// LOS is expensive - re-test only ~1x/sec (LOS_TEST_INTERVAL), otherwise serve the cached value.
	// true = clear line of sight (no geometry in between).
	bool GetGunShipLosToShootTarget(const ItemInfo& item, int shootTargetNum)
	{
		if (GlobalCounter - GunShip.LastTestFrame < LOS_TEST_INTERVAL)
			return GunShip.Clear;

		GunShip.LastTestFrame = GlobalCounter;

		auto origin = GameVector(item.Pose.Position.ToVector3(), item.RoomNumber);
		auto target = GameVector(g_Level.Items[shootTargetNum].Pose.Position.ToVector3(), g_Level.Items[shootTargetNum].RoomNumber);

		target += GameVector(0, -512, 0, target.RoomNumber);

		auto clamped = target;
		GunShip.Clear = LOS(&origin, &clamped);
		return GunShip.Clear;
	}

	// Helper: computes the distance and direction to the target
	struct GunShipTargetInfo
	{
		float horizontalDistance = 0.0f;
		float verticalDifference = 0.0f;
		Vector3 targetPos;
		bool hasMoveTarget = false;

		void Calculate(ItemInfo* item, ItemInfo* moveTargetItem, const Vector3& moveTargetPos)
		{
			if (moveTargetPos != Vector3::Zero)
			{
				targetPos = moveTargetPos;
				hasMoveTarget = true;
			}
			else
			{
				targetPos = moveTargetItem->Pose.Position.ToVector3();
				hasMoveTarget = false;
			}

			float dx = targetPos.x - item->Pose.Position.x;
			float dz = targetPos.z - item->Pose.Position.z;
			horizontalDistance = sqrtf(dx * dx + dz * dz);

			verticalDifference = item->Pose.Position.y - targetPos.y;
		}
	};

	// Helper: sets the Y position based on ceiling/floor
	void FixYPosition(ItemInfo* item, float currentYSpeed = 0.0f)
	{
		auto& frameData = GetFrame(*item);
		float bottomMeshY = item->Pose.Position.y + frameData.BoundingBox.Y1;
		float topMeshY = item->Pose.Position.y + frameData.BoundingBox.Y2;

		FloorInfo* floorCheck = GetFloor(item->Pose.Position.x, item->Pose.Position.y, item->Pose.Position.z, &item->RoomNumber);
		if (floorCheck != nullptr)
		{
			const int floorHeight = GetFloorHeight(floorCheck, item->Pose.Position.x, item->Pose.Position.y, item->Pose.Position.z);
			const int ceilingHeight = GetCeiling(floorCheck, item->Pose.Position.x, item->Pose.Position.y, item->Pose.Position.z);

			// Guarantee minimum clearance to the floor (NEVER sink into the ground!)
			if (floorHeight != NO_VALUE)
			{
				const int minGroundClearance = SECTOR_SIZE / 4; // 256 units = 0.25 BLOCK
				// If the heli's bottom lies too low - correct upward
				if (bottomMeshY > floorHeight - minGroundClearance)
					item->Pose.Position.y = floorHeight - frameData.BoundingBox.Y1 - minGroundClearance;
			}

			// Guarantee minimum clearance to the ceiling
			if (ceilingHeight != NO_VALUE)
			{
				const int minCeilingClearance = SECTOR_SIZE / 16; // 64 units = 0.0625 BLOCK, heli may go as close to the ceiling as possible
				if (topMeshY < ceilingHeight + minCeilingClearance)
					item->Pose.Position.y = ceilingHeight + minCeilingClearance - frameData.BoundingBox.Y2;
			}
		}
	}

	// Helper: updates Y movement and orientation in the IDLE state
	void UpdateIdleOrientation(ItemInfo* item, const Vector3& targetPos, float ySpeedTargetGlobal, float& pitchTarget, float& bankTarget)
	{
		if (ySpeedTargetGlobal == 0.0f)
			return;

		auto fwdVec = Vector3(
			cosf(item->Pose.Orientation.x) * sinf(item->Pose.Orientation.y),
			sinf(item->Pose.Orientation.x),
			cosf(item->Pose.Orientation.x) * cosf(item->Pose.Orientation.y));
		fwdVec.Normalize();

		Vector3 toTarget = targetPos - item->Pose.Position.ToVector3();
		toTarget.y = 0.0f;
		toTarget.Normalize();

		float crossY = fwdVec.z * toTarget.x - fwdVec.x * toTarget.z;

		// Pitch based on forward/backward movement
		float dotForward = fwdVec.Dot(toTarget);
		if (dotForward < 0.0f)
			pitchTarget = -(float)DEG_TO_RAD(MAX_PITCH_DEG); // flying backward - pitch back
		else
			pitchTarget = (float)DEG_TO_RAD(MAX_PITCH_DEG); // flying forward - pitch forward

		bankTarget = DEG_TO_RAD(MAX_BANK_DEG) * crossY;
	}

	// Helper: computes pitch/bank for the current state
	void CalculatePitchAndBank(const ItemInfo& item, GunShipState state, const Vector3& targetPos, float yDodgingFactor, float& pitchTarget, float& bankTarget)
	{
		switch (state)
		{
		case GunShipState::FOLLOW:
		case GunShipState::ESCAPE:
			pitchTarget = (float)DEG_TO_RAD(MAX_PITCH_DEG);
			break;
		case GunShipState::EVADE_NEAR:
			pitchTarget = -(float)DEG_TO_RAD(MAX_PITCH_DEG);
			break;
		default:
			pitchTarget = 0.0f;
			return;
		}

		auto fwdVec = Vector3(
			cosf(item.Pose.Orientation.x) * sinf(item.Pose.Orientation.y),
			sinf(item.Pose.Orientation.x),
			cosf(item.Pose.Orientation.x) * cosf(item.Pose.Orientation.y));
		fwdVec.Normalize();

		Vector3 toTarget = targetPos - item.Pose.Position.ToVector3();
		toTarget.y = 0.0f;
		toTarget.Normalize();

		float crossY = fwdVec.z * toTarget.x - fwdVec.x * toTarget.z;

		if (state == GunShipState::EVADE_NEAR && yDodgingFactor != 0.0f)
			bankTarget = DEG_TO_RAD(MAX_BANK_DEG) * crossY;
		else
			bankTarget = DEG_TO_RAD(MAX_BANK_DEG) * crossY;
	}

	// Helper: IDLE pitch that aims the tail gun at the target's lower-leg height.
	// Positive pitch = nose up = tail gun (model axis -Z) points down (offset: pitch + 8 degrees).
	float CalculateIdlePitch(const ItemInfo& item, const Vector3& targetPos)
	{
		float dx = targetPos.x - item.Pose.Position.x;
		float dz = targetPos.z - item.Pose.Position.z;
		float hDist = sqrtf(dx * dx + dz * dz);
		if (hDist < 1.0f)
			hDist = 1.0f;

		// Lower legs sit slightly above the target's pivot (positive Y = downward).
		constexpr float LOWER_LEG_OFFSET = SECTOR_SIZE * 0.25f;
		float dy = (targetPos.y - LOWER_LEG_OFFSET) - item.Pose.Position.y;

		float pitch = atan2f(dy, hDist) - (float)DEG_TO_RAD(8.0f);

		constexpr float maxPitch = (float)DEG_TO_RAD(MAX_PITCH_DEG);
		if (pitch > maxPitch)
			pitch = maxPitch;
		if (pitch < 0.0f)
			pitch = 0.0f;
		return pitch;
	}

	// Fire block: can the shooter fire from the muzzle?
	// true = a target (item/static) lies in the line of fire (own item is skipped).
	bool CanFireShot(ItemInfo* shooter, const Vector3& muzzlePos, const EulerAngles& orientation, float range)
	{
		auto rot = orientation.ToRotationMatrix();
		Vector3 aimed = muzzlePos + Vector3::Transform(Vector3(0.0f, -512.0f, -range * 2), rot);
		auto dir = aimed - muzzlePos;
		float dist = dir.Length();
		dir.Normalize();

		shooter->Collidable = false;
		auto los = GetLosCollision(muzzlePos, shooter->RoomNumber, dir, dist, true, false, true, true);
		shooter->Collidable = true;

		for (auto& itemLos : los.Items)
		{
			if (itemLos.Item == shooter)
				continue;
			return true;
		}

		return !los.Statics.empty();
	}

	// A complete shot (hitscan): fire block + sound + muzzle effects (light/casing/smoke) +
	// tracer + shot ray with hit resolution (static: damage/shatter, item: damage, wall: ricochet).
	void FireShot(ItemInfo* shooter, const Vector3& muzzlePos, const EulerAngles& orientation, float range, int damage, LaraWeaponType weaponType, int sfxID)
	{
		//if (!CanFireShot(shooter, muzzlePos, orientation, range))
//			return;

		// Sound.
		SoundEffect(sfxID, &shooter->Pose, SoundEnvironment::Land, 0.8f);

		// Muzzle effects: light, casing, smoke.
		auto lightColor = Vector3(Random::GenerateFloat(0.75f, 0.85f), Random::GenerateFloat(0.5f, 0.6f), 0.0f) * 255;
		SpawnDynamicLight(muzzlePos.x, muzzlePos.y, muzzlePos.z, 10, lightColor.x, lightColor.y, lightColor.z);

		TriggerGunShellAt(Vector3i(muzzlePos.x, muzzlePos.y, muzzlePos.z), shooter->RoomNumber, ID_GUNSHELL, weaponType);

		TriggerGunSmoke(muzzlePos.x, muzzlePos.y, muzzlePos.z, 0, 0, 0, 0, weaponType, 16);

		// Shot ray: aim spread around the forward direction, distance = double range.
		const float aimSpread = BLOCK(0.2f); // 512 World-Units = 0.5 BLOCK
		auto rot = orientation.ToRotationMatrix();
		auto origin = GameVector(muzzlePos, shooter->RoomNumber);

		float spreadX = Random::GenerateFloat(-aimSpread, aimSpread);
		float spreadY = Random::GenerateFloat(-aimSpread, aimSpread);
		float spreadZ = Random::GenerateFloat(-aimSpread, aimSpread);
		Vector3 aimedPos = muzzlePos + Vector3::Transform(Vector3(spreadX, spreadY, spreadZ) + Vector3(0.0f, 0.0f, -range * 2), rot);

		auto shotDir = aimedPos - muzzlePos;
		float shotDist = shotDir.Length();
		shotDir.Normalize();

		shooter->Collidable = false;
		auto shotLos = GetLosCollision(muzzlePos, shooter->RoomNumber, shotDir, shotDist, true, false, true, true);
		shooter->Collidable = true;

		TriggerBulletTracer(origin, GameVector(aimedPos, shooter->RoomNumber));

		// Determine the next hit (skip own item).
		float bestDist = shotDist;
		bool hitIsItem = false;
		ItemInfo* hitItem = nullptr;
		StaticMesh* hitStatic = nullptr;
		Vector3 hitPos = Vector3::Zero;

		for (auto& itemLos : shotLos.Items)
		{
			if (itemLos.Item == shooter)
				continue;
			if (itemLos.Distance < bestDist)
			{
				bestDist = itemLos.Distance;
				hitIsItem = true;
				hitItem = itemLos.Item;
				hitPos = itemLos.Position;
			}
		}

		for (auto& staticLos : shotLos.Statics)
		{
			if (staticLos.Distance < bestDist)
			{
				bestDist = staticLos.Distance;
				hitIsItem = false;
				hitStatic = staticLos.Static;
				hitPos = staticLos.Position;
			}
		}

		bool hasHit = (hitItem != nullptr || hitStatic != nullptr);

		if (hasHit)
		{
			if (!hitIsItem)
			{
				// Static mesh hit.
				auto* mesh = hitStatic;
				int slot = mesh->Slot;

				if (Statics[slot].shatterType != ShatterType::None)
				{
					mesh->HitPoints -= damage;
					ShatterImpactData.impactDirection = Vector3(0, 0, 0);
					ShatterImpactData.impactLocation = hitPos;
					int shatterRoomNumber = FindRoomNumber(Vector3i(hitPos), mesh->RoomNumber, true);
					ShatterObject(nullptr, mesh, 128, shatterRoomNumber, 0);
					SoundEffect(GetShatterSound(slot), &mesh->Pose);
				}

				GameVector impactPos(hitPos.x, hitPos.y, hitPos.z, origin.RoomNumber);
				TriggerRicochetSpark(impactPos, Random::GenerateAngle());
			}
			else
			{
				// Item hit (creature or object).
				if (hitItem->Index == LaraItem->Index || hitItem->IsCreature())
					DoDamage(hitItem, damage);

				GameVector impactPos(hitPos.x, hitPos.y, hitPos.z, origin.RoomNumber);
				TriggerRicochetSpark(impactPos, Random::GenerateAngle());
			}
		}
		else if (shotLos.Room.IsIntersected)
		{
			// Room geometry hit (wall/floor/ceiling) - ricochet at the hit point.
			hitPos = shotLos.Room.Position;
			GameVector impactPos(hitPos.x, hitPos.y, hitPos.z, shotLos.Room.RoomNumber);
			TriggerRicochetSpark(impactPos, Random::GenerateAngle());
		}
	}

	// =========================================================================
	// Gunship - Original (normal mode, TR5 red-alert behavior)
	// =========================================================================
	// ItemFlags (per heli): [1]=Offset.x, [2]=Offset.y, [4]=Offset.z (glide smoothing).
	// [0]=mode (read by ControlGunShip) is NOT touched.
	// Fire unconditionally via FireShot (original red-alert, spread like in boss mode).
	void ControlOriginalGunShip(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		if (!TriggerActive(item))
			return;

		item->HitPoints = NOT_TARGETABLE;

		SoundEffect(SFX_TR4_HELICOPTER_LOOP, &item->Pose);

		// Target position: Lara position + random offset (heli hovers around Lara).
		auto playerPos = g_Level.Items[LaraItem->Index].Pose.Position.ToVector3();
		auto pos = GameVector(
			playerPos + Vector3(
				Random::GenerateFloat(-255.0f, 255.0f),
				Random::GenerateFloat(-255.0f, 255.0f),
				Random::GenerateFloat(-255.0f, 255.0f)),
			LaraItem->RoomNumber);

		// Initialize glide offset (only on the first frame).
		if (!item->ItemFlags[1] && !item->ItemFlags[2] && !item->ItemFlags[4])
		{
			item->ItemFlags[1] = pos.x / 16;
			item->ItemFlags[2] = pos.y / 16;
			item->ItemFlags[4] = pos.z / 16;
		}

		// Update glide offset (EMA).
		pos.x = (pos.x + 80 * item->ItemFlags[1]) / 6;
		pos.y = (pos.y + 80 * item->ItemFlags[2]) / 6;
		pos.z = (pos.z + 80 * item->ItemFlags[4]) / 6;

		item->ItemFlags[1] = pos.x / 16;
		item->ItemFlags[2] = pos.y / 16;
		item->ItemFlags[4] = pos.z / 16;

		// Heli flies to Lara (lerp factor 1/32).
		short movementAxis = PropertyHandler::Get(*item, PropName_HorizontalVelocity, (short)GunshipAxis::XAxis);

		if (movementAxis == (short)GunshipAxis::ZAxis)
			item->Pose.Position.z += (pos.z - item->Pose.Position.z) / 32;
		else
			item->Pose.Position.x += (pos.x - item->Pose.Position.x) / 32;
		item->Pose.Position.y += (pos.y - item->Pose.Position.y - 256) / 32;

		// Firing: muzzle (joint 8), original red-alert: target = Lara's hips (LM_HIPS) with
		// a random offset, extrapolated 2x beyond Lara (target = 3*pos - 2*origin, as in the TR5 original).
		float maxShotsRange = PropertyHandler::Get(item, PropName_ShootTargetDistance, 3);
		maxShotsRange = maxShotsRange * SECTOR_SIZE;
		auto muzzlePos = GetJointPosition(item, 8, Vector3i::Zero).ToVector3();

		// Lara torso + random offset (corresponds to pos in the original).
		Vector3i randomOffset(
			Random::GenerateInt(-125, 125),
			Random::GenerateInt(-125, 125),
			Random::GenerateInt(-125, 125));
		auto laraPos = GetJointPosition(LaraItem, LM_HIPS, randomOffset).ToVector3();
		laraPos.y += 125;
		// Extrapolation: 2x beyond Lara (3*pos - 2*origin, leading aim like in the original).
		auto aimPoint = 3.0f * laraPos - 2.0f * item->Pose.Position.ToVector3();
		auto fireOrientation = Geometry::GetOrientToPoint(muzzlePos, aimPoint);
		fireOrientation.y += ANGLE(180.0f);

		// Only fire within weapon range (distance heli -> Lara).
		float distanceToLara = Vector3::Distance(item->Pose.Position.ToVector3(), laraPos);
		bool inRange = distanceToLara <= maxShotsRange;

		// Muzzle flash (mesh bit) only in range.
		if (inRange)
			item->MeshBits |= 0x100;
		else
			item->MeshBits &= 0xFEFF;

		// One shot every FIRE_RATE frames, only in range.
		if (inRange && GlobalCounter % FIRE_RATE == 0)
			FireShot(item, muzzlePos, fireOrientation, maxShotsRange, GUNSHIP_DAMAGE, LaraWeaponType::HK, SFX_TR4_HK_FIRE);

		AnimateItem(item);
	}

	void ControlGunShip(short itemNumber)
	{
		auto* item = &g_Level.Items[itemNumber];

		if (!TriggerActive(item))
			return;

		if (!CreatureActive(itemNumber))
			return;

		SoundEffect(SFX_TR4_HELICOPTER_LOOP, &item->Pose);

		item->ItemFlags[0] = PropertyHandler::Get(*item, PropName_AttackType, false);

		if (item->ItemFlags[0] == 1)
		{
			ControlOriginalGunShip(itemNumber);
			return;
		}

		if (item->HitPoints <= 0)
		{
			ExplodingDeath(itemNumber, BODY_DO_EXPLOSION | BODY_NO_BOUNCE);
			DisableEntityAI(itemNumber);
			KillItem(itemNumber);

			item->Flags |= 1;
			item->Status = ITEM_DEACTIVATED;

			TriggerExplosionSparks(item->Pose.Position.x, item->Pose.Position.y - CLICK(3), item->Pose.Position.z, 3, -2, 0, item->RoomNumber);
			for (int i = 0; i < 2; i++)
				TriggerExplosionSparks(item->Pose.Position.x, item->Pose.Position.y - CLICK(3), item->Pose.Position.z, 3, -1, 0, item->RoomNumber);

			SoundEffect(SFX_TR4_EXPLOSION1, &item->Pose, SoundEnvironment::Land, 1.5f);
			SoundEffect(SFX_TR4_EXPLOSION2, &item->Pose);
		}

		int shootTargetNum = PropertyHandler::Get(*item, PropName_EnemyTarget, LaraItem->Index);
		bool hasShootTarget = (shootTargetNum >= 0);

		// Lua move target (GoToTarget): highest priority. One-shot: cleared upon reaching it.
		Vector3 moveTargetPos = (Vector3)PropertyHandler::Get(*item, PropName_GoToTarget, Vec3());
		bool hasLuaTarget = (moveTargetPos != Vector3::Zero);

		// Auto-escape: when active and no Lua target, the escape target acts as the move target.
		// A Lua target (GoToTarget) has priority: it makes the heli fly to its point even during an escape.
		if (!hasLuaTarget && GunShip.Active)
		{
			moveTargetPos = GunShip.TargetPos;

			// Timeout: escape must not run forever (e.g. path blocked / target unreachable) - then resume combat.
			if (++GunShip.Frames > MAX_ESCAPE_FRAMES)
			{
				GunShip.ResetEscape();
				item->ItemFlags[3] = 0;
				moveTargetPos = Vector3::Zero;
			}
		}

		bool hasMoveTargetPos = (moveTargetPos != Vector3::Zero);

		// StopMovement: latching sync - the flag is set by the property (manual stop) OR the heli (auto-stop on a reached Lua target).
		// Once set to 1, it stays 1 until explicitly cleared (e.g. new target).
		item->ItemFlags[3] = PropertyHandler::Get(*item, PropName_StopMovement, item->ItemFlags[3]);

		// One-shot: moveTargetPos (HORIZONTAL) reached - clear the state; normal combat resumes.
		// Horizontal instead of 3D: in FOLLOW the heli hovers ~HOVER_HEIGHT_OFFSET below the target and would never
		// bring the 3D distance below the radius - the escape state would otherwise never clear (heli stuck in IDLE, not firing).
		if (hasMoveTargetPos)
		{
			float dmx = moveTargetPos.x - item->Pose.Position.x;
			float dmz = moveTargetPos.z - item->Pose.Position.z;
			if (sqrtf(dmx * dmx + dmz * dmz) < MOVE_TARGET_REACH_RADIUS)
			{
				GunShip.ResetEscape();
				hasMoveTargetPos = false;
				moveTargetPos = Vector3::Zero;

				if (hasLuaTarget)
				{
					// Lua target reached: clear the property + latch the stop flag (heli parks & hovers/shoots).
					item->Properties.Set(PropName_GoToTarget, Vec3());
					item->ItemFlags[3] = 1;
				}
				// Escape target reached: NO stop - combat resumes automatically (heli keeps flying).
			}
		}

		ItemInfo* moveTargetItem = hasMoveTargetPos ? nullptr : LaraItem.Get();
		if (!hasMoveTargetPos && hasShootTarget)
			moveTargetItem = &g_Level.Items[shootTargetNum];

		int minDistance = PropertyHandler::Get(item, PropName_ShootTargetDistance, 3);
		minDistance = minDistance * SECTOR_SIZE;
		int maxShotsRange = minDistance + SECTOR_SIZE;

		GunShipTargetInfo targetInfo;
		targetInfo.Calculate(item, moveTargetItem, moveTargetPos);

		float yDiff = targetInfo.verticalDifference;
		float horizontalDist = targetInfo.horizontalDistance;

		GunShipState currentState = DetermineGunShipState(*item, horizontalDist, hasMoveTargetPos);

		// StopMovement: force IDLE (hover) when the stop flag is set.
		if (item->ItemFlags[3] == 1)
			currentState = GunShipState::IDLE;

		int prevStates = item->ItemFlags[4];
		int inertiaTimer = item->ItemFlags[5];

		if (prevStates != (int)currentState && !item->ItemFlags[7])
		{
			inertiaTimer = INERTIA_FRAMES;
			item->ItemFlags[4] = (int)currentState;
			item->ItemFlags[5] = INERTIA_FRAMES;
		}

		if (currentState == GunShipState::EVADE_NEAR && !item->ItemFlags[7] && horizontalDist < SECTOR_SIZE * 3 && yDiff >= -SECTOR_SIZE * 6)
			item->ItemFlags[7] = 1;

		// Evade complete (heli back outside firing range) - reset the evade flag.
		// Otherwise the heli never cleanly switches to IDLE in range (FOLLOW/EVADE loop).
		if (item->ItemFlags[7] && horizontalDist > maxShotsRange && currentState != GunShipState::EVADE_NEAR)
		{
			item->ItemFlags[7] = 0;
			inertiaTimer = 0;
			item->ItemFlags[5] = 0;
		}

		if (item->ItemFlags[7] && currentState != GunShipState::ESCAPE)
			currentState = GunShipState::EVADE_NEAR;

		// LOS-based waiting: the heli only approaches/attacks (FOLLOW/IDLE) when it has a clear view of
		// the shoot target. Without LOS (wall in between) - wait in IDLE until the view is clear again.
		// EVADE_NEAR stays untouched (escape path preserved). Applies to all distances (also outside
		// firing range); performance via throttling ~1x/sec (expensive query, cached).
		bool engaging = (currentState == GunShipState::FOLLOW || currentState == GunShipState::IDLE);
			if (engaging && hasShootTarget && shootTargetNum >= 0 && !GetGunShipLosToShootTarget(*item, shootTargetNum))
			currentState = GunShipState::IDLE;

		// Lara is diving (underwater, WaterStatus::Underwater) - the heli holds in IDLE and
		// waits until she surfaces (surface/tread water) or is back on land.
		if (engaging && hasShootTarget && shootTargetNum >= 0 && Lara.Control.WaterStatus == WaterStatus::Underwater)
			currentState = GunShipState::IDLE;

		float currentYSpeed = item->ItemFlags[6] / 100.0f; // x100 (x1000 would overflow the short)
		const float yLerpAlpha = 1.0f / powf(2.0f, MOVEMENT_LERP_SPEED);
		const int minYDiff = SECTOR_SIZE;

		float ySpeedTargetIdle = 0.0f;
		Vector3 targetPosIdle = targetInfo.targetPos;

		// Compute target speed based on state (always, not only when there is no inertia!)
		float targetSpeed = 0.0f;
		float idleTargetY = 0.0f;
		switch (currentState)
		{
			case GunShipState::FOLLOW:
			targetSpeed = hasMoveTargetPos ? MAX_MOVE_SPEED : (horizontalDist > maxShotsRange) ? MAX_MOVE_SPEED : MAX_MOVE_SPEED * 0.25f;
			
			// FOLLOW: heli follows the height of the target to stay above it (as in IDLE/EVADE).
			// Escape/move target: fly to the target height (no hover offset) - horizontal, natural approach
			// (otherwise the heli slowly climbs/sinks - looks like it is being pulled up on a rope).
			if (hasMoveTargetPos)
				idleTargetY = moveTargetPos.y;
			else
				idleTargetY = targetPosIdle.y - HOVER_HEIGHT_OFFSET;
			if (fabsf(item->Pose.Position.y - idleTargetY) > minYDiff)
				ySpeedTargetIdle = (idleTargetY > item->Pose.Position.y) ? FLY_DOWN_SPEED : -FLY_UP_SPEED;
			currentYSpeed += (ySpeedTargetIdle - currentYSpeed) * yLerpAlpha;
			
			break;

			case GunShipState::IDLE:
			targetSpeed = 0.0f;
	
				// Lara is diving (underwater) - the heli holds its height and does not sink (hovers horizontally only).
				if (Lara.Control.WaterStatus == WaterStatus::Underwater)
				{
					ySpeedTargetIdle = 0.0f;
				}
				else
				{
					// Heli hovers slightly above the target so it can aim downward when firing.
					idleTargetY = targetPosIdle.y - HOVER_HEIGHT_OFFSET;
					if (fabsf(item->Pose.Position.y - idleTargetY) > minYDiff)
						ySpeedTargetIdle = (idleTargetY > item->Pose.Position.y) ? FLY_DOWN_SPEED : -FLY_UP_SPEED;
				}
			currentYSpeed += (ySpeedTargetIdle - currentYSpeed) * yLerpAlpha;
			
			break;

			case GunShipState::EVADE_NEAR:
			{
			targetSpeed = MAX_MOVE_SPEED * 2.5f;
			
			// When evading (flying backward) climb slightly so the tail does not hit the ground when pitching back.
				float evadeTargetY = targetPosIdle.y - EVADE_RAISE_HEIGHT;
				if (fabsf(item->Pose.Position.y - evadeTargetY) > minYDiff)
					ySpeedTargetIdle = (evadeTargetY > item->Pose.Position.y) ? FLY_DOWN_SPEED : -FLY_UP_SPEED;
				else
					ySpeedTargetIdle = 0.0f;
			
			currentYSpeed += (ySpeedTargetIdle - currentYSpeed) * yLerpAlpha;
			break;
			}

			case GunShipState::ESCAPE:
			targetSpeed = MAX_MOVE_SPEED;
			
			// Escape: fly Y freely to the target (only FixYPosition clamps to the room ceiling/floor).
			idleTargetY = moveTargetPos.y;
			if (fabsf(item->Pose.Position.y - idleTargetY) > minYDiff)
				ySpeedTargetIdle = (idleTargetY > item->Pose.Position.y) ? FLY_DOWN_SPEED : -FLY_UP_SPEED;
			currentYSpeed += (ySpeedTargetIdle - currentYSpeed) * yLerpAlpha;
			break;
		}

		// StopMovement: no Y movement when the stop flag is set.
		if (item->ItemFlags[3] == 1)
			currentYSpeed = 0.0f;

		// Apply inertia - reduces target speed on state switch
		if (inertiaTimer > 0)
		{
			targetSpeed *= 0.15f;
			inertiaTimer--;
			item->ItemFlags[5] = inertiaTimer;
		}

		// Pitch aus Laufzeitdaten
		float currentPitch = item->ItemFlags[1] / 1000.0f; // Rad x1000

		// Compute pitch ratio for the speed
		float pitchRatio = fabsf(currentPitch) / ((float)MAX_PITCH_DEG * DEG_TO_RAD(1.0f));

		// currentSpeed from targetSpeed and pitchRatio.
		// ESCAPE is a committed emergency flight: speed is decoupled from the (cosmetic)
		// pitch ramp. Otherwise the heli crawls (pitch ramp + inertia + !isMoving damping)
		// and stops in front of the wall instead of flying to the escape target.
		float currentSpeed = (currentState == GunShipState::ESCAPE) ? targetSpeed : targetSpeed * pitchRatio;

		bool isMoving = currentSpeed > 1.0f;

		EulerAngles targetOrient;
		if (hasShootTarget && shootTargetNum >= 0)
			targetOrient = Geometry::GetOrientToPoint(item->Pose.Position.ToVector3(), g_Level.Items[shootTargetNum].Pose.Position.ToVector3());
		else
			targetOrient = Geometry::GetOrientToPoint(item->Pose.Position.ToVector3(), targetInfo.targetPos);

		// Compute pitch and bank target values (ALWAYS, even when not yet moving!)
		float pitchTarget = 0.0f;
		float bankTarget = 0.0f;
		switch (currentState)
		{
			case GunShipState::FOLLOW:
			pitchTarget = (float)DEG_TO_RAD(MAX_PITCH_DEG);
			CalculatePitchAndBank(*item, currentState, targetInfo.targetPos, 0.0f, pitchTarget, bankTarget);
			break;

			case GunShipState::ESCAPE:
			// Escape: pitch like FOLLOW (nose slightly up) so currentSpeed stays > 0 and the heli moves.
			pitchTarget = (float)DEG_TO_RAD(MAX_PITCH_DEG);
			CalculatePitchAndBank(*item, currentState, targetInfo.targetPos, 0.0f, pitchTarget, bankTarget);
			break;

			case GunShipState::EVADE_NEAR:
			pitchTarget = -(float)DEG_TO_RAD(MAX_PITCH_DEG);
			CalculatePitchAndBank(*item, currentState, targetInfo.targetPos, 0.0f, pitchTarget, bankTarget);
			break;

			case GunShipState::IDLE:
			// Heli tilts so that the tail gun points at the target's lower legs.
			pitchTarget = CalculateIdlePitch(*item, targetInfo.targetPos);
			bankTarget = 0.0f;
			break;

			default:
			pitchTarget = 0.0f;
			bankTarget = 0.0f;
			break;
		}

		// Movement direction (FOLLOW: toward the target, EVADE: away from it), normalized.
		Vector3 moveDir(0.0f, 0.0f, 0.0f);
		if (horizontalDist > 1.0f)
		{
			moveDir = Vector3(targetInfo.targetPos.x - item->Pose.Position.x, 0.0f, targetInfo.targetPos.z - item->Pose.Position.z);
			moveDir = moveDir * (1.0f / horizontalDist);
			if (currentState == GunShipState::EVADE_NEAR)
				moveDir = -moveDir;
		}

		bool blocked = false;
		bool overfly = false;
		// Avoidance cascade (SweptFootprintClear + FindBestAvoidanceDirection + Overfly) ONLY for EVADE_NEAR.
		// FOLLOW (forward flight) currently has NO wall check (disabled for testing); it flies through walls.
		// ESCAPE: no wall/avoidance check - a committed, straight emergency flight to the (pre-validated) escape target,
		// through wall/geometry if necessary. Otherwise the cascade blocks the escape in tight gaps (walls left/right/behind)
		// and the heli stands still instead of reaching open space (e.g. flying over Lara).
		if (isMoving && horizontalDist > 1.0f && currentState != GunShipState::FOLLOW && currentState != GunShipState::ESCAPE)
		{
			// Probe distance: movement-speed-based, at least 1 sector (no wall tunneling).
			float probeDistance = (currentSpeed > (float)SECTOR_SIZE) ? currentSpeed : (float)SECTOR_SIZE;

			// Test the preferred direction (combat distance / move-to) for flight clearance.
			if (!SweptFootprintClear(*item, moveDir * probeDistance))
			{
				// Preferred blocked - pick a near avoidance direction (EVADE_NEAR).
				// Collision avoidance has priority over combat distance.
				Vector3 avoidDir = moveDir;
				if (!FindBestAvoidanceDirection(*item, moveDir, probeDistance, avoidDir))
				{
					// No free horizontal direction - overfly vertically (only with ceiling clearance).
					if (SweptFootprintClear(*item, moveDir * probeDistance + Vector3(0.0f, -EVADE_OVERFLY_HEIGHT, 0.0f)))
						overfly = true;
					else
						blocked = true;
				}
				else
				{
					moveDir = avoidDir;
				}
			}
			else if (!SweptFootprintClear(*item, moveDir * probeDistance * 2.0f))
			{
				// Soft blockage further ahead - reduce speed.
				currentSpeed *= 0.5f;
			}

			// Nose orientation: overfly - aim along the movement direction (otherwise the target stays aimed).
			if (overfly)
			{
				Vector3 overPoint = item->Pose.Position.ToVector3() + moveDir * probeDistance;
				overPoint.y = targetInfo.targetPos.y - EVADE_OVERFLY_HEIGHT;
				targetOrient = Geometry::GetOrientToPoint(item->Pose.Position.ToVector3(), overPoint);
			}
		}

		// Final validation of the actual movement (fixes wall pass-through) ONLY for EVADE_NEAR.
		// Overfly: movement is horizontal + vertical, so validate the raised end point.
		// FOLLOW/ESCAPE: skipped (no wall check; on a wall - it flies through, no forward-flight stop).
		if (isMoving && !blocked && currentState != GunShipState::FOLLOW && currentState != GunShipState::ESCAPE)
		{
			Vector3 finalDisplacement = moveDir * currentSpeed;
			if (overfly)
				finalDisplacement += Vector3(0.0f, -EVADE_OVERFLY_HEIGHT, 0.0f);

			if (!SweptFootprintClear(*item, finalDisplacement))
				blocked = true;
		}

		if (blocked)
		{
			const GunShipState blockedState = currentState;

			currentSpeed = 0.0f;

			currentState = GunShipState::IDLE;

			// Auto-escape: heli cannot continue (wall / room-bound) - find an escape target (>= firing range,
			// in reachable rooms, with clearance) and fly there (via move target). Once
			// arrived, the escape state is cleared and combat resumes automatically.
			// Only re-trigger from EVADE_NEAR: during the ESCAPE flight the heli stays committed to the current target
			// (until reaching it/timeout) and no longer swaps it permanently - prevents the
			// EVADE_NEAR<->ESCAPE stuck loop (the heli would otherwise bounce between the two with no progress).
			if (blockedState == GunShipState::EVADE_NEAR && hasShootTarget && shootTargetNum >= 0)
			{
				Vector3 escapeTarget = Vector3::Zero;
				if (FindEscapeTarget(*item, g_Level.Items[shootTargetNum].Pose.Position.ToVector3(), (float)(maxShotsRange + SECTOR_SIZE), GunShip.TargetPos, ESCAPE_EXCLUDE_RADIUS, escapeTarget))
				{
					GunShip.Active = true;
					GunShip.TargetPos = escapeTarget;
					GunShip.Frames = 0; // New target - fresh timeout window (otherwise Frames carries over from the reset).
					item->ItemFlags[7] = 0; // Reset the evade flag so the ESCAPE state is not forced to EVADE_NEAR.
				}
			}

			FixYPosition(item);

			pitchTarget = 0.0f;
			bankTarget = 0.0f;

			// Continue animation despite the blocked status
			AnimateItem(item);
			
			// Collision check after the avoidance movement to verify whether a free path exists
			blocked = CheckFootprintCollision(*item, moveDir * SECTOR_SIZE);
			
			if (!blocked)
			{
				// Collision resolved - wait in IDLE until the target is in range again
				currentState = GunShipState::IDLE;
			}
			else
			{
				// Still blocked - no movement
				currentState = GunShipState::IDLE;
				currentSpeed = 0.0f;
			}
		}

		if (!blocked)
		{
			// Always compute pitch and bank (even when not yet moving!)
			switch (currentState)
			{
			case GunShipState::FOLLOW:
			case GunShipState::ESCAPE:
				CalculatePitchAndBank(*item, currentState, targetInfo.targetPos, 0.0f, pitchTarget, bankTarget);
				break;
			case GunShipState::EVADE_NEAR:
				pitchTarget = -(float)DEG_TO_RAD(MAX_PITCH_DEG);
				CalculatePitchAndBank(*item, currentState, targetInfo.targetPos, 0.0f, pitchTarget, bankTarget);
				break;
			case GunShipState::IDLE:
				// Heli tilts so that the tail gun points at the target's lower legs.
				pitchTarget = CalculateIdlePitch(*item, targetInfo.targetPos);
				bankTarget = 0.0f;
				break;
			default:
				pitchTarget = 0.0f;
				bankTarget = 0.0f;
				break;
			}

			// Only move when isMoving
			if (isMoving)
			{
				float moveDist = currentSpeed;

				switch (currentState)
				{
					case GunShipState::FOLLOW:
					if (horizontalDist > 1.0f)
					{
						// moveDir holds the active flight direction (preferred direction or avoidance direction).
						item->Pose.Position.x += (int)(moveDir.x * moveDist);
						item->Pose.Position.z += (int)(moveDir.z * moveDist);
					}

					if (item->ItemFlags[7])
					{
						float targetY = hasMoveTargetPos ? moveTargetPos.y : moveTargetItem->Pose.Position.y;
						if (fabsf(item->Pose.Position.y - targetY) < SECTOR_SIZE)
							item->ItemFlags[7] = 0;
					}

					break;

					case GunShipState::EVADE_NEAR:
					if (horizontalDist > 1.0f)
					{
						// moveDir holds the active avoidance direction (backward, lateral or overfly).
						item->Pose.Position.x += (int)(moveDir.x * moveDist);
						item->Pose.Position.z += (int)(moveDir.z * moveDist);
					}

					break;

					case GunShipState::ESCAPE:
					if (horizontalDist > 1.0f)
					{
						// Escape: fly directly to the escape target (XZ; Y is handled separately).
						item->Pose.Position.x += (int)(moveDir.x * moveDist);
						item->Pose.Position.z += (int)(moveDir.z * moveDist);
					}

					break;
				}

				if (currentState != GunShipState::IDLE)
				{
					short newRoomNum = GetPointCollision(item->Pose.Position, item->RoomNumber).GetRoomNumber();
					if (newRoomNum != item->RoomNumber)
						ItemNewRoom(itemNumber, newRoomNum);
				}
			}
		}

		if (!blocked)
		{
			// Only apply the Y step if the displaced footprint is clear (side walls/elevations).
			int yDelta = (int)currentYSpeed;
			if (overfly)
			{
				// Overfly: climb to the raised height over Lara.
				float overflyTargetY = targetInfo.targetPos.y - EVADE_OVERFLY_HEIGHT;
				float dy = overflyTargetY - (float)item->Pose.Position.y;
				yDelta = (fabsf(dy) > 1.0f) ? ((dy < 0.0f) ? -FLY_UP_SPEED : FLY_DOWN_SPEED) : 0;
			}
			if (yDelta != 0 && CheckFootprintCollision(*item, Vector3(0.0f, (float)yDelta, 0.0f)))
				yDelta = 0;

			item->Pose.Position.y += yDelta;

			FixYPosition(item);

			// Only for FOLLOW/EVADE: sets movement pitch (+/- MAX_PITCH) and would overwrite the IDLE target pitch.
			if (currentState != GunShipState::IDLE)
				UpdateIdleOrientation(item, targetInfo.targetPos, currentYSpeed, pitchTarget, bankTarget);

			const float pitchLerpAlpha = 1.0f / powf(2.0f, PITCH_LERP_SPEED);
			const float bankLerpAlpha = 1.0f / powf(2.0f, BANK_LERP_SPEED);

			float currentPitch = item->ItemFlags[1] / 1000.0f; // radians x1000
			float currentBankAngle = item->ItemFlags[2] / 1000.0f; // radians x1000

			currentPitch += (pitchTarget - currentPitch) * pitchLerpAlpha;
			currentBankAngle += (bankTarget - currentBankAngle) * bankLerpAlpha;

			item->ItemFlags[1] = (short)(currentPitch * 1000.0f);
			item->ItemFlags[2] = (short)(currentBankAngle * 1000.0f);

			if (!isMoving)
			{
				// Pitch/bank reset to level only outside IDLE: IDLE holds the target pitch on the lower legs.
				if (currentState != GunShipState::IDLE)
				{
					currentPitch *= 0.95f;
					currentBankAngle *= 0.95f;
					item->ItemFlags[1] = (short)(currentPitch * 1000.0f);
					item->ItemFlags[2] = (short)(currentBankAngle * 1000.0f);
				}

				if (fabsf(currentYSpeed) > 0.1f)
					currentYSpeed += (-currentYSpeed) * 0.1f; // target is 0
				else
					currentYSpeed *= 0.95f;
			}

			float lerpAlpha = 1.0f / powf(2.0f, 3);
			if (GunShip.FireFrameCounter == 1)
				lerpAlpha = 1.0f;

			EulerAngles orientResult = targetOrient;
			orientResult.y += ANGLE(180.0f);

			constexpr float RAD_TO_SHORTS = (float)(65536.0 / (2.0 * PI));
			orientResult.x = (short)(currentPitch * RAD_TO_SHORTS);
			orientResult.z = (short)(currentBankAngle * RAD_TO_SHORTS);

			item->Pose.Orientation = EulerAngles::Lerp(item->Pose.Orientation, orientResult, lerpAlpha);

			if (GetFloor(item->Pose.Position.x, item->Pose.Position.y, item->Pose.Position.z, &item->RoomNumber) != nullptr)
				GetCeiling(GetFloor(item->Pose.Position.x, item->Pose.Position.y, item->Pose.Position.z, &item->RoomNumber), item->Pose.Position.x, item->Pose.Position.y, item->Pose.Position.z);

			GunShip.FireFrameCounter++;

			CollisionInfo coll{};
			auto collObjects = GetCollidedObjects(*item, true, true);

			if (!collObjects.Statics.empty())
			{
				for (const StaticMesh* staticMesh : collObjects.Statics)
					ItemPushStatic(item, *staticMesh, &coll);
			}

		int shootHdx = 0.0f, shootHdz = 0.0f, shootHLen = 0.0f;
		if (hasShootTarget && shootTargetNum >= 0)
		{
			shootHdx = g_Level.Items[shootTargetNum].Pose.Position.x - item->Pose.Position.x;
			shootHdz = g_Level.Items[shootTargetNum].Pose.Position.z - item->Pose.Position.z;
			shootHLen = sqrtf(shootHdx * shootHdx + shootHdz * shootHdz);
		}

		// Also fire at the shoot target during a move-target/escape flight, as long as it is in range (heli repositions and fires at the same time).
		const bool hasShootTargetInRange = hasShootTarget && shootHLen <= maxShotsRange;

		if (hasShootTargetInRange)
		{
			// Muzzle (weapon neck joint 8) + fire orientation (slight nod downward).
			auto muzzleJoint = GetJointPosition(item, 8, Vector3i::Zero);
			auto muzzlePos = muzzleJoint.ToVector3();
			auto fireOrientation = EulerAngles(item->Pose.Orientation.x + ANGLE(8.0f), item->Pose.Orientation.y, item->Pose.Orientation.z);

			// Fire block: only fire when a target (Lara/static) is in the line of fire and no wall is in between.
			bool canFire = CanFireShot(item, muzzlePos, fireOrientation, maxShotsRange);

			// Muzzle flash (mesh bit) while firing is possible.
			if (canFire)
			{
				if (GunShip.FireFrameCounter > FIRE_RATE)
					item->MeshBits |= 0x100;
				else
					item->MeshBits &= 0xFEFF;
			}

			// One shot every FIRE_RATE frames (after the initial delay).
			if (canFire && (GlobalCounter % FIRE_RATE) == 0 && GunShip.FireFrameCounter > FIRE_RATE)
				FireShot(item, muzzlePos, fireOrientation, maxShotsRange, GUNSHIP_DAMAGE, LaraWeaponType::HK, SFX_TR4_HK_FIRE);
		}
		// Not in range: ensure flash is cleared.
		else
			item->MeshBits &= 0xFEFF;

			// Persist Y speed per heli (read as the start value on the next frame).
			item->ItemFlags[6] = (short)(currentYSpeed * 100.0f); // x100 (x1000 would overflow the short)

			AnimateItem(item);
		}
	}

	void HitGunship(ItemInfo& target, ItemInfo& source, std::optional<GameVector> pos, int damage, bool isExplosive, int jointIndex)
	{
		const auto& player = *GetLaraInfo(&source);

		if (player.Control.Weapon.GunType == LaraWeaponType::Uzi ||	player.Control.Weapon.GunType == LaraWeaponType::HK || player.Control.Weapon.GunType == LaraWeaponType::Revolver)
			DefaultItemHit(target, source, pos, damage / DAMAGE_LIMITER, isExplosive, jointIndex);
		else
			DefaultItemHit(target, source, pos, damage, isExplosive, jointIndex);
	}
}
