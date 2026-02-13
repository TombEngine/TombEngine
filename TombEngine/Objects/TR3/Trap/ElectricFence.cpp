#include "framework.h"
#include "Objects/TR3/Trap/ElectricFence.h"

#include "Game/collision/collide_room.h"
#include "Game/collision/floordata.h"
#include "Game/effects/effects.h"
#include "Game/effects/Electricity.h"
#include "Game/effects/item_fx.h"
#include "Game/items.h"
#include "Math/Math.h"
#include "Renderer/Renderer.h"
#include "Sound/sound.h"
#include "Specific/level.h"

using namespace TEN::Collision::Floordata;
using namespace TEN::Effects::Electricity;
using namespace TEN::Effects::Items;
using namespace TEN::Math;
using namespace TEN::Renderer;

namespace TEN::Entities::Traps
{
	constexpr auto ELECTRIC_FENCE_HEIGHT = CLICK(2);
	constexpr auto ELECTRIC_FENCE_DAMAGE = INT_MAX;
	constexpr auto ELECTRIC_FENCE_DEBUG = true;

	void InitializeElectricFence(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];
		item.Status = ITEM_ACTIVE;
	}

	// Helper to check if there's a wall at a sector edge
	static bool IsWallAtEdge(int x, int z, int roomNumber, bool checkNorth, bool checkEast, bool checkSouth, bool checkWest)
	{
		auto& sector = GetFloor(roomNumber, x, z);

		// Check for side portal (non-traversable portal wall)
		if (sector.SidePortalRoomNumber != 0)
			return true;

		// Check specific edge for portal
		int adjacentX = x;
		int adjacentZ = z;

		if (checkNorth) adjacentZ--;
		else if (checkSouth) adjacentZ++;
		else if (checkWest) adjacentX--;
		else if (checkEast) adjacentX++;

		// Get adjacent sector
		auto gridCoord = GetRoomGridCoord(roomNumber, adjacentX * BLOCK(1), adjacentZ * BLOCK(1), false);
		if (gridCoord.x < 0 || gridCoord.y < 0)
			return true; // Out of bounds = wall

		auto& adjacentSector = GetFloor(roomNumber, gridCoord);

		// Check for portal to another room
		auto nextRoom = adjacentSector.GetNextRoomNumber(adjacentX * BLOCK(1), adjacentZ * BLOCK(1), true);
		if (nextRoom.has_value() && nextRoom.value() != roomNumber)
		{
			// Portal exists - check if it's traversable
			if (!TestNeighborRooms(roomNumber, nextRoom.value()))
				return true; // Non-traversable portal = wall
		}

		// Check floor/ceiling height difference
		int fenceFloorHeight = sector.GetSurfaceHeight(x * BLOCK(1), z * BLOCK(1), true);
		int fenceCeilingHeight = sector.GetSurfaceHeight(x * BLOCK(1), z * BLOCK(1), false);
		int adjacentFloorHeight = adjacentSector.GetSurfaceHeight(adjacentX * BLOCK(1), adjacentZ * BLOCK(1), true);
		int adjacentCeilingHeight = adjacentSector.GetSurfaceHeight(adjacentX * BLOCK(1), adjacentZ * BLOCK(1), false);

		// Wall if significant height difference
		if (abs(adjacentFloorHeight - fenceFloorHeight) >= CLICK(2))
			return true;
		if (abs(adjacentCeilingHeight - fenceCeilingHeight) >= CLICK(2))
			return true;

		return false;
	}

	void ControlElectricFence(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		if (item.Status != ITEM_ACTIVE)
			return;

		bool isFloorMode = (item.TriggerFlags == 0);
		bool isFenceMode = (item.TriggerFlags == 1);

		// Spawn electric effects
		int r = Random::GenerateInt(0, 63) + 128;
		int g = Random::GenerateInt(0, 63) + 192;
		int b = 255;
		int lightSize = Random::GenerateInt(5, 7);

		if (isFloorMode)
		{
			// Floor mode: Horizontal electrical field with dynamic light
			SpawnDynamicLight(
				item.Pose.Position.x,
				item.Pose.Position.y - CLICK(2),
				item.Pose.Position.z,
				lightSize, r, g, b);

			if (Random::TestProbability(0.5f))
			{
				Vector3 origin = Vector3(
					item.Pose.Position.x + Random::GenerateInt(-BLOCK(0.5f), BLOCK(0.5f)),
					item.Pose.Position.y + sin(GlobalCounter / 2.0f) * CLICK(0.5f),
					item.Pose.Position.z + Random::GenerateInt(-BLOCK(0.5f), BLOCK(0.5f)));
				Vector3 target = Vector3(
					item.Pose.Position.x + Random::GenerateInt(-BLOCK(0.5f), BLOCK(0.5f)),
					item.Pose.Position.y + sin(GlobalCounter / 4.0f) * CLICK(0.5f),
					item.Pose.Position.z + Random::GenerateInt(-BLOCK(0.5f), BLOCK(0.5f)));

				SpawnElectricity(origin, target,
					Random::GenerateInt(4, 12),
					32, g, b, 8,
					(int)ElectricityFlags::Spline | (int)ElectricityFlags::SparkEnd,
					Random::GenerateFloat(20.0f, 40.0f),
					Random::GenerateInt(4, 8));

				if (Random::TestProbability(0.25f))
					SoundEffect(SFX_TR5_ELECTRIC_LIGHT_CRACKLES, &item.Pose);
			}
		}
		/* else if (isFenceMode)
		{
			// Fence mode: Short electrical bursts scattered across the wall segment
			// Spawns every couple of seconds

			// Only spawn sparks occasionally (every ~4-5 seconds at 30fps)
			if (!Random::TestProbability(0.006f))
			{
				// Just show dynamic light when not sparking
				SpawnDynamicLight(
					item.Pose.Position.x,
					item.Pose.Position.y,
					item.Pose.Position.z,
					lightSize, r, g, b);
				return;
			}

			// Get floor and ceiling heights at fence position for wall segment height
			short roomNumber = item.RoomNumber;
			auto* floor = GetFloor(item.Pose.Position.x, item.Pose.Position.y, item.Pose.Position.z, &roomNumber);
			int floorHeight = GetFloorHeight(floor, item.Pose.Position.x, item.Pose.Position.y, item.Pose.Position.z);
			int ceilingHeight = GetCeiling(floor, item.Pose.Position.x, item.Pose.Position.y, item.Pose.Position.z);

			// Wall segment extends from ceiling to floor at this position
			int wallTop = ceilingHeight + CLICK(1);
			int wallBottom = floorHeight - CLICK(1);

			// Determine fence facing direction
			short angle = item.Pose.Orientation.y;
			Vector3 towardWall;
			Vector3 alongWall;

			if (angle >= ANGLE(315.0f) || angle < ANGLE(45.0f))
			{
				towardWall = Vector3(0, 0, -1); // North
				alongWall = Vector3(1, 0, 0);   // Along X (East-West)
			}
			else if (angle >= ANGLE(45.0f) && angle < ANGLE(135.0f))
			{
				towardWall = Vector3(1, 0, 0);  // East
				alongWall = Vector3(0, 0, 1);   // Along Z (North-South)
			}
			else if (angle >= ANGLE(135.0f) && angle < ANGLE(225.0f))
			{
				towardWall = Vector3(0, 0, 1);  // South
				alongWall = Vector3(1, 0, 0);   // Along X (East-West)
			}
			else
			{
				towardWall = Vector3(-1, 0, 0); // West
				alongWall = Vector3(0, 0, 1);   // Along Z (North-South)
			}

			// Base position offset toward wall
			Vector3 basePos = item.Pose.Position.ToVector3() + (towardWall * CLICK(0.5f));

			// Spawn 3-5 short spark bursts scattered across the wall segment
			int numSparks = Random::GenerateInt(3, 5);
			for (int i = 0; i < numSparks; i++)
			{
				// Random position across wall segment (width and height)
				float alongDist = Random::GenerateFloat(-CLICK(3), CLICK(3));  // Width along wall
				float verticalPos = Random::GenerateFloat(wallTop, wallBottom); // Wall segment height
				float depthOffset = Random::GenerateFloat(-CLICK(0.2f), CLICK(0.2f)); // Slight depth variation

				// Calculate spark center position
				Vector3 sparkCenter = basePos + (alongWall * alongDist) + (towardWall * depthOffset);
				sparkCenter.y = verticalPos;

				// Create SHORT spark arc (not long vertical line)
				// Origin and target are close together for a "burst" effect
				Vector3 origin = sparkCenter + Vector3(
					Random::GenerateFloat(-CLICK(0.5f), CLICK(0.5f)),
					Random::GenerateFloat(-CLICK(0.5f), CLICK(0.5f)),
					Random::GenerateFloat(-CLICK(0.5f), CLICK(0.5f)));

				Vector3 target = sparkCenter + Vector3(
					Random::GenerateFloat(-CLICK(0.5f), CLICK(0.5f)),
					Random::GenerateFloat(-CLICK(0.5f), CLICK(0.5f)),
					Random::GenerateFloat(-CLICK(0.5f), CLICK(0.5f)));

				// Spawn short electrical burst
				SpawnElectricity(origin, target,
					Random::GenerateInt(4, 8),   // Lower amplitude for tighter sparks
					32, g, b,
					Random::GenerateFloat(8.0f, 16.0f),  // Shorter life
					(int)ElectricityFlags::Spline | (int)ElectricityFlags::ThinOut | (int)ElectricityFlags::ThinIn | (int)ElectricityFlags::SparkEnd,
					Random::GenerateFloat(3.0f, 5.0f),   // Thinner
					Random::GenerateInt(3, 5));          // Fewer segments

				// Add glow at spark
				if (Random::TestProbability(0.3f))
					SpawnElectricityGlow(sparkCenter, Random::GenerateFloat(12.0f, 20.0f), 32, g, b);
			}

			// Sound effect when sparking
			if (Random::TestProbability(0.5f))
				SoundEffect(SFX_TR5_ELECTRIC_LIGHT_CRACKLES, &item.Pose);

			// Dynamic light at fence center
			SpawnDynamicLight(
				item.Pose.Position.x,
				item.Pose.Position.y,
				item.Pose.Position.z,
				lightSize, r, g, b);
		}
		*/
		// Check all creatures for collision with electric fence
		for (auto& entity : g_Level.Items)
		{
			// Skip dead entities
			if (entity.HitPoints <= 0)
				continue;

			// Only check Lara and creatures
			if (!entity.IsLara() && !entity.IsCreature())
				continue;

			// Skip airborne entities
			if (entity.Animation.IsAirborne)
				continue;

			bool shouldKill = false;

			if (isFloorMode)
			{
				// Floor mode: same sector check
				int fenceSectorX = item.Pose.Position.x / BLOCK(1);
				int fenceSectorZ = item.Pose.Position.z / BLOCK(1);
				int entitySectorX = entity.Pose.Position.x / BLOCK(1);
				int entitySectorZ = entity.Pose.Position.z / BLOCK(1);

				if (fenceSectorX == entitySectorX && fenceSectorZ == entitySectorZ)
				{
					short fenceRoomNum = item.RoomNumber;
					auto* fenceFloor = GetFloor(item.Pose.Position.x, item.Pose.Position.y, item.Pose.Position.z, &fenceRoomNum);
					int fenceFloorHeight = GetFloorHeight(fenceFloor, item.Pose.Position.x, item.Pose.Position.y, item.Pose.Position.z);

					short entityRoomNum = entity.RoomNumber;
					auto* entityFloor = GetFloor(entity.Pose.Position.x, entity.Pose.Position.y, entity.Pose.Position.z, &entityRoomNum);
					int entityFloorHeight = GetFloorHeight(entityFloor, entity.Pose.Position.x, entity.Pose.Position.y, entity.Pose.Position.z);

					if (abs(entityFloorHeight - fenceFloorHeight) < CLICK(1))
						shouldKill = true;
				}
			}
			/* else if (isFenceMode)
			{
				// Fence mode: wall edge check
				short angle = item.Pose.Orientation.y;
				int fenceSectorX = item.Pose.Position.x / BLOCK(1);
				int fenceSectorZ = item.Pose.Position.z / BLOCK(1);

				// Determine wall direction
				bool checkNorth = (angle >= ANGLE(315.0f) || angle < ANGLE(45.0f));
				bool checkEast = (angle >= ANGLE(45.0f) && angle < ANGLE(135.0f));
				bool checkSouth = (angle >= ANGLE(135.0f) && angle < ANGLE(225.0f));
				bool checkWest = (angle >= ANGLE(225.0f) && angle < ANGLE(315.0f));

				// Check if wall exists
				if (IsWallAtEdge(fenceSectorX, fenceSectorZ, item.RoomNumber, checkNorth, checkEast, checkSouth, checkWest))
				{
					int sectorMinX = fenceSectorX * BLOCK(1);
					int sectorMaxX = sectorMinX + BLOCK(1);
					int sectorMinZ = fenceSectorZ * BLOCK(1);
					int sectorMaxZ = sectorMinZ + BLOCK(1);

					constexpr auto WALL_THRESHOLD = CLICK(2.5f);

					auto& sector = GetFloor(item.RoomNumber, fenceSectorX, fenceSectorZ);
					int wallBottom = sector.GetSurfaceHeight(item.Pose.Position.x, item.Pose.Position.z, true);
					int wallTop = sector.GetSurfaceHeight(item.Pose.Position.x, item.Pose.Position.z, false);

					bool hitWall = false;
					if (checkNorth && abs(entity.Pose.Position.z - sectorMinZ) <= WALL_THRESHOLD)
					{
						if (entity.Pose.Position.x >= sectorMinX && entity.Pose.Position.x <= sectorMaxX)
							hitWall = true;
					}
					else if (checkEast && abs(entity.Pose.Position.x - sectorMaxX) <= WALL_THRESHOLD)
					{
						if (entity.Pose.Position.z >= sectorMinZ && entity.Pose.Position.z <= sectorMaxZ)
							hitWall = true;
					}
					else if (checkSouth && abs(entity.Pose.Position.z - sectorMaxZ) <= WALL_THRESHOLD)
					{
						if (entity.Pose.Position.x >= sectorMinX && entity.Pose.Position.x <= sectorMaxX)
							hitWall = true;
					}
					else if (checkWest && abs(entity.Pose.Position.x - sectorMinX) <= WALL_THRESHOLD)
					{
						if (entity.Pose.Position.z >= sectorMinZ && entity.Pose.Position.z <= sectorMaxZ)
							hitWall = true;
					}

					if (hitWall && entity.Pose.Position.y >= wallTop && entity.Pose.Position.y <= wallBottom)
					{
						int dy = abs(entity.Pose.Position.y - item.Pose.Position.y);
						if (dy <= ELECTRIC_FENCE_HEIGHT)
							shouldKill = true;
					}
				}
			} */

			if (shouldKill)
			{
				DoDamage(&entity, ELECTRIC_FENCE_DAMAGE);
				ItemElectricBurn(&entity, ELECTRIC_FENCE_DAMAGE);
				ItemBlueElectricBurn(&entity, 2 * FPS);
			}
		}
	}
}