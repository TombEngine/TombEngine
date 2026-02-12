#include "framework.h"
#include "Objects/TR3/Trap/ElectricFence.h"

#include "Game/effects/effects.h"
#include "Game/effects/Electricity.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Math/Math.h"
#include "Specific/level.h"
#include "Game/effects/item_fx.h"


using namespace TEN::Effects::Electricity;
using namespace TEN::Effects::Items;
using namespace TEN::Math;


namespace TEN::Entities::Traps
{
	constexpr auto ELECTRIC_FENCE_RANGE		 = BLOCK(1);		// Horizontal detection range.
	constexpr auto ELECTRIC_FENCE_HEIGHT	 = CLICK(2);		// Vertical detection range.
	constexpr auto ELECTRIC_FENCE_DAMAGE	 = INT_MAX;			// Damage per frame while standing on it.
	
	auto ELECTRIC_FENCE_LIGHT_SIZE			 = Random::GenerateInt(5,7);

	// OCB values:
	// 0 = Starts inactive (must be triggered on)
	// 1 = Starts active

	void InitializeElectricFence(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		// OCB determines initial state: 0 = inactive, 1 = active.
		if (item.TriggerFlags == 0)
			item.Status = ITEM_NOT_ACTIVE;
		else
			item.Status = ITEM_ACTIVE;
	}

	void ControlElectricFence(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		// Only process if active.
		if (item.Status != ITEM_ACTIVE)
			return;

		// Spawn electric light effect.
		int r = Random::GenerateInt(0, 63) + 128;
		int g = Random::GenerateInt(0, 63) + 192;
		int b = 255;
		SpawnDynamicLight(
			item.Pose.Position.x,
			item.Pose.Position.y - CLICK(2),
			item.Pose.Position.z,
			ELECTRIC_FENCE_LIGHT_SIZE, r, g, b);

		// Spawn electricity arcs occasionally for visual effect.
		if (Random::TestProbability(1 / 2.0f))
		{
			auto origin = Vector3(
				item.Pose.Position.x + Random::GenerateInt(-BLOCK(0.5f), BLOCK(0.5f)),
				item.Pose.Position.y + sin(GlobalCounter / 2.0f) * CLICK(0.5f), // Slight vertical oscillation.
				item.Pose.Position.z + Random::GenerateInt(-BLOCK(0.5f), BLOCK(0.5f)));

			auto target = Vector3(
				item.Pose.Position.x + Random::GenerateInt(-BLOCK(0.5f), BLOCK(0.5f)),
				item.Pose.Position.y + sin(GlobalCounter / 4.0f) * CLICK(0.5f), // Slight vertical oscillation.
				item.Pose.Position.z + Random::GenerateInt(-BLOCK(0.5f), BLOCK(0.5f)));

			SpawnElectricity(
				origin, target,
				Random::GenerateInt(4, 12),
				32, g, b,
				8,
				(int)ElectricityFlags::Spline | (int)ElectricityFlags::SparkEnd,
				Random::GenerateFloat(20.0f, 40.0f),
				Random::GenerateInt(4, 8));

			// Play electric sound
			if (Random::TestProbability(1 / 4.0f))
			{
				SoundEffect(SFX_TR5_ELECTRIC_LIGHT_CRACKLES, &item.Pose);
			}
		}

		// Check all items in the level for entities standing on the fence.
		for (auto& entity : g_Level.Items)
		{
			// Skip non-creatures flying and dead entities.
			// TODO: Flying creatures still get affected.
			if (!entity.IsCreature() || item.Animation.IsAirborne || entity.HitPoints <= 0)
				continue;

			// Check horizontal distance.
			int dx = entity.Pose.Position.x - item.Pose.Position.x;
			int dz = entity.Pose.Position.z - item.Pose.Position.z;
			float distance2D = sqrt(SQUARE(dx) + SQUARE(dz));

			if (distance2D > ELECTRIC_FENCE_RANGE)
				continue;

			// Check vertical distance (entity must be roughly at fence height).
			int dy = abs(entity.Pose.Position.y - item.Pose.Position.y);
			
			if (dy > ELECTRIC_FENCE_HEIGHT)
				continue;

			// Check if entity is on the ground (not jumping over).
			if (entity.Animation.IsAirborne)
				continue;

			// Electric fence hit - damage entity.
			DoDamage(&entity, INT_MAX);
			ItemElectricBurn(&entity, ELECTRIC_FENCE_DAMAGE);
			ItemBlueElectricBurn(&entity, 2 * FPS);
		}
	}
}
