#include "framework.h"
#include "Objects/TR3/Trap/ElectricFence.h"

#include "Game/collision/collide_room.h"
#include "Game/collision/floordata.h"
#include "Game/effects/effects.h"
#include "Game/effects/Electricity.h"
#include "Game/effects/item_fx.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
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

		SpawnDynamicLight(
			item.Pose.Position.x,
			isFloorMode ? item.Pose.Position.y - CLICK(2) : item.Pose.Position.y,
			item.Pose.Position.z,
			lightSize, r, g, b);

		if (Random::TestProbability(isFloorMode ? 0.5f : 0.33f))
		{
			Vector3 origin, target;
			if (isFloorMode)
			{
				origin = Vector3(
					item.Pose.Position.x + Random::GenerateInt(-BLOCK(0.5f), BLOCK(0.5f)),
					item.Pose.Position.y + sin(GlobalCounter / 2.0f) * CLICK(0.5f),
					item.Pose.Position.z + Random::GenerateInt(-BLOCK(0.5f), BLOCK(0.5f)));
				target = Vector3(
					item.Pose.Position.x + Random::GenerateInt(-BLOCK(0.5f), BLOCK(0.5f)),
					item.Pose.Position.y + sin(GlobalCounter / 4.0f) * CLICK(0.5f),
					item.Pose.Position.z + Random::GenerateInt(-BLOCK(0.5f), BLOCK(0.5f)));
			}
			else
			{
				origin = Vector3(
					item.Pose.Position.x + Random::GenerateInt(-BLOCK(0.25f), BLOCK(0.25f)),
					item.Pose.Position.y - Random::GenerateInt(0, CLICK(2)),
					item.Pose.Position.z + Random::GenerateInt(-BLOCK(0.25f), BLOCK(0.25f)));
				target = Vector3(
					origin.x + Random::GenerateInt(-CLICK(0.5f), CLICK(0.5f)),
					origin.y - Random::GenerateInt(CLICK(1), CLICK(3)),
					origin.z + Random::GenerateInt(-CLICK(0.5f), CLICK(0.5f)));
			}

			SpawnElectricity(origin, target,
				Random::GenerateInt(isFloorMode ? 4 : 2, isFloorMode ? 12 : 6),
				32, g, b, isFloorMode ? 8 : 6,
				isFloorMode ? 
					((int)ElectricityFlags::Spline | (int)ElectricityFlags::SparkEnd) :
					((int)ElectricityFlags::ThinIn | (int)ElectricityFlags::ThinOut | (int)ElectricityFlags::SparkEnd),
				Random::GenerateFloat(isFloorMode ? 20.0f : 8.0f, isFloorMode ? 40.0f : 16.0f),
				Random::GenerateInt(isFloorMode ? 4 : 3, isFloorMode ? 8 : 5));

			if (Random::TestProbability(isFloorMode ? 0.25f : 0.33f))
				SoundEffect(SFX_TR5_ELECTRIC_LIGHT_CRACKLES, &item.Pose);
		}

		// Check Lara collision
		auto laraItem = LaraItem;
		if (!laraItem || laraItem->HitPoints <= 0)
			return;

		bool shouldKill = false;

		if (isFloorMode)
		{
			// Floor mode: same sector check
			int fenceSectorX = item.Pose.Position.x / BLOCK(1);
			int fenceSectorZ = item.Pose.Position.z / BLOCK(1);
			int laraSectorX = laraItem->Pose.Position.x / BLOCK(1);
			int laraSectorZ = laraItem->Pose.Position.z / BLOCK(1);

			if (fenceSectorX == laraSectorX && fenceSectorZ == laraSectorZ && !laraItem->Animation.IsAirborne)
			{
				short fenceRoomNum = item.RoomNumber;
				auto* fenceFloor = GetFloor(item.Pose.Position.x, item.Pose.Position.y, item.Pose.Position.z, &fenceRoomNum);
				int fenceFloorHeight = GetFloorHeight(fenceFloor, item.Pose.Position.x, item.Pose.Position.y, item.Pose.Position.z);

				short laraRoomNum = laraItem->RoomNumber;
				auto* laraFloor = GetFloor(laraItem->Pose.Position.x, laraItem->Pose.Position.y, laraItem->Pose.Position.z, &laraRoomNum);
				int laraFloorHeight = GetFloorHeight(laraFloor, laraItem->Pose.Position.x, laraItem->Pose.Position.y, laraItem->Pose.Position.z);

				if (abs(laraFloorHeight - fenceFloorHeight) < CLICK(1))
					shouldKill = true;
			}
		}
		else if (isFenceMode)
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
				if (checkNorth && abs(laraItem->Pose.Position.z - sectorMinZ) <= WALL_THRESHOLD)
				{
					if (laraItem->Pose.Position.x >= sectorMinX && laraItem->Pose.Position.x <= sectorMaxX)
						hitWall = true;
				}
				else if (checkEast && abs(laraItem->Pose.Position.x - sectorMaxX) <= WALL_THRESHOLD)
				{
					if (laraItem->Pose.Position.z >= sectorMinZ && laraItem->Pose.Position.z <= sectorMaxZ)
						hitWall = true;
				}
				else if (checkSouth && abs(laraItem->Pose.Position.z - sectorMaxZ) <= WALL_THRESHOLD)
				{
					if (laraItem->Pose.Position.x >= sectorMinX && laraItem->Pose.Position.x <= sectorMaxX)
						hitWall = true;
				}
				else if (checkWest && abs(laraItem->Pose.Position.x - sectorMinX) <= WALL_THRESHOLD)
				{
					if (laraItem->Pose.Position.z >= sectorMinZ && laraItem->Pose.Position.z <= sectorMaxZ)
						hitWall = true;
				}

				if (hitWall && laraItem->Pose.Position.y >= wallTop && laraItem->Pose.Position.y <= wallBottom)
				{
					int dy = abs(laraItem->Pose.Position.y - item.Pose.Position.y);
					if (dy <= ELECTRIC_FENCE_HEIGHT)
						shouldKill = true;
				}
			}
		}

		if (shouldKill)
		{
			DoDamage(laraItem, ELECTRIC_FENCE_DAMAGE);
			ItemElectricBurn(laraItem, ELECTRIC_FENCE_DAMAGE);
			ItemBlueElectricBurn(laraItem, 2 * FPS);
		}
	}
}
