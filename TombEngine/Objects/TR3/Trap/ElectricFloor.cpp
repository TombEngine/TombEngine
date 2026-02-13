#include "framework.h"
#include "Objects/TR3/Trap/ElectricFloor.h"

#include "Game/effects/Electricity.h"
#include "Game/effects/item_fx.h"
#include "Game/items.h"
#include "Renderer/Renderer.h"
#include "Sound/sound.h"

using namespace TEN::Control::Volumes;
using namespace TEN::Effects::Electricity;
using namespace TEN::Effects::Items;
using namespace TEN::Math;
using namespace TEN::Renderer;

namespace TEN::Entities::Traps
{
	constexpr auto ELECTRIC_electricFloor_HEIGHT = CLICK(2);
	constexpr auto ELECTRIC_electricFloor_DAMAGE = INT_MAX;

	void InitializeElectricFloor(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];
		item.Status = ITEM_ACTIVE;
	}

	void ControlElectricFloor(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		if (item.Status != ITEM_ACTIVE)
			return;

		// Spawn electric effects
		int r = Random::GenerateInt(0, 63) + 128;
		int g = Random::GenerateInt(0, 63) + 192;
		int b = 255;
		int lightSize = Random::GenerateInt(5, 7);

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
		
		// Check all creatures for collision with electric electricFloor
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
			bool isCheckingCollision = false;

			// Floor mode: same sector check
			int electricFloorSectorX = item.Pose.Position.x / BLOCK(1);
			int electricFloorSectorZ = item.Pose.Position.z / BLOCK(1);
			int entitySectorX = entity.Pose.Position.x / BLOCK(1);
			int entitySectorZ = entity.Pose.Position.z / BLOCK(1);

			if (electricFloorSectorX == entitySectorX && electricFloorSectorZ == entitySectorZ)
			{
				isCheckingCollision = true;

				short electricFloorRoomNum = item.RoomNumber;
				auto* electricFloorFloor = GetFloor(item.Pose.Position.x, item.Pose.Position.y, item.Pose.Position.z, &electricFloorRoomNum);
				int electricFloorFloorHeight = GetFloorHeight(electricFloorFloor, item.Pose.Position.x, item.Pose.Position.y, item.Pose.Position.z);

				short entityRoomNum = entity.RoomNumber;
				auto* entityFloor = GetFloor(entity.Pose.Position.x, entity.Pose.Position.y, entity.Pose.Position.z, &entityRoomNum);
				int entityFloorHeight = GetFloorHeight(entityFloor, entity.Pose.Position.x, entity.Pose.Position.y, entity.Pose.Position.z);

				if (abs(entityFloorHeight - electricFloorFloorHeight) < CLICK(1))
					shouldKill = true;
			}			
			
			if (shouldKill)
			{
				DoDamage(&entity, ELECTRIC_electricFloor_DAMAGE);
				ItemElectricBurn(&entity, ELECTRIC_electricFloor_DAMAGE);
				ItemBlueElectricBurn(&entity, 2 * FPS);
			}
		}		
	}
}