#include "framework.h"
#include "Objects/TR3/Trap/ElectricField.h"
#include "Game/Lara/lara_helpers.h"

#include "Game/effects/Electricity.h"
#include "Game/effects/spark.h"
#include "Game/effects/item_fx.h"
#include "Game/items.h"
#include "Renderer/Renderer.h"
#include "Sound/sound.h"
#include "Game/control/trigger.h"

using namespace TEN::Effects::Spark;
using namespace TEN::Effects::Items;

namespace TEN::Entities::Traps
{
	constexpr auto ELECTRIC_FIELD_DAMAGE = INT_MAX;

	// Performance tuning constants
	constexpr auto EFFECT_UPDATE_INTERVAL = 2;   // Update visual effects every N frames
	constexpr auto SOUND_UPDATE_INTERVAL = 10;  // Update sound every N frames

	void InitializeElectricField(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];
		item.Status = ITEM_ACTIVE;
	}

	static void SpawnSparks(const ItemInfo& item, float halfSpanX, float halfSpanZ, float cosY, float sinY, bool isWallMode)
	{
		// Throttle: Only update every 2nd frame (50% reduction)
		if (GlobalCounter % EFFECT_UPDATE_INTERVAL != 0)
			return;

		// Reduced probability from 0.6 to 0.4
		if (!Random::TestProbability(0.4f))
			return;

		float localX = Random::GenerateFloat(-halfSpanX, halfSpanX);
		float localY = isWallMode ? Random::GenerateFloat(-BLOCK(1), BLOCK(1)) : 0;
		float localZ = isWallMode ? -CLICK(1) : Random::GenerateFloat(-halfSpanZ, halfSpanZ);

		float sparkX = item.Pose.Position.x + (localX * cosY - localZ * sinY);
		float sparkY = item.Pose.Position.y + localY;
		float sparkZ = item.Pose.Position.z + (localX * sinY + localZ * cosY);

		// Always spawn exactly 1 spark (not 1-2) for better performance
		auto& s = GetFreeSparkParticle();
		s = {};
		s.age = 0;
		s.life = Random::GenerateFloat(10, 18);
		s.friction = 1.0f;
		s.gravity = 2.0f;
		s.height = Random::GenerateFloat(128.0f, 384.0f);
		s.width = Random::GenerateFloat(16.0f, 32.0f);
		s.room = item.RoomNumber;
		s.pos = Vector3(
			sparkX + Random::GenerateFloat(-16, 16),
			sparkY + Random::GenerateFloat(-16, 16),
			sparkZ + Random::GenerateFloat(-16, 16));

		if (isWallMode)
		{
			float	direction = Random::TestProbability(0.5f) ? 1.0f : -1.0f;
			float	ang = TO_RAD(item.Pose.Orientation.y);
			Vector3 v = Vector3(-sin(ang) * direction, Random::GenerateFloat(-0.2f, 0.2f), -cos(ang) * direction);
			v.Normalize(v);
			s.velocity = v * Random::GenerateFloat(8, 32);
		}
		else
		{
			float	ang = TO_RAD(Random::GenerateAngle());
			float	vertAng = -TO_RAD(90.0f);
			Vector3 v = Vector3(sin(ang), vertAng + Random::GenerateFloat(-PI / 16, PI / 16), cos(ang));
			v.Normalize(v);
			s.velocity = v * Random::GenerateFloat(8, 32);
		}

		s.sourceColor = Vector4(0.4f, 0.6f, 1.0f, 1);
		s.destinationColor = Vector4(0.6f, 0.6f, 0.8f, 0.8f);
		s.active = true;

		// Throttle sound effects to every 10th frame
		if (GlobalCounter % SOUND_UPDATE_INTERVAL == 0 && Random::TestProbability(0.25f))
		{
			SoundEffect(SFX_TR5_ELECTRIC_LIGHT_CRACKLES, const_cast<Pose*>(&item.Pose));

			// Reduced light probability from 0.1 to 0.05 and radius from BLOCK(3) to BLOCK(2)
			if (Random::TestProbability(0.05f))
			{
				Vector3 lightPos = Vector3(sparkX, sparkY, sparkZ);
				Color	lightColor = Color(0.4f, 0.6f, 1.0f);
				SpawnDynamicPointLight(lightPos, lightColor, BLOCK(2), false, 0);
			}
		}
	}

	static void SpawnFloorLight(ItemInfo& item, float halfSpanX, float halfSpanZ, float cosY, float sinY, bool isWallMode)
	{
		// Throttle more aggressively to reduce epilepsy risk
		// Only spawn every 5 frames minimum
		if (GlobalCounter % 5 != 0)
			return;

		// Lower probability for gentler effect
		if (!Random::TestProbability(0.2f))
			return;

		if (isWallMode)
		{
			// Random position along wall width - FULLY contained within field bounds
			float localX = Random::GenerateFloat(-halfSpanX, halfSpanX);
			float localY = Random::GenerateFloat(-BLOCK(0.5f), BLOCK(0.5f));
			float wallZ = -CLICK(1);

			Vector3 lightPos = Vector3(
				item.Pose.Position.x + (localX * cosY - wallZ * sinY),
				item.Pose.Position.y + localY,
				item.Pose.Position.z + (localX * sinY + wallZ * cosY));

			// Reduced flicker range to be gentler (was 0.8-1.2, now 0.9-1.1)
			float flicker = Random::GenerateFloat(0.9f, 1.1f);
			Color lightColor = Color(0.6f * flicker, 0.9f * flicker, 1.50f * flicker);

			// Smaller radius to keep light more contained within field bounds
			float lightRadius = std::min(BLOCK(3.0f), halfSpanX * 1.5f);
			SpawnDynamicPointLight(lightPos, lightColor, lightRadius, false, item.Index);  // No shadows
		}
		else
		{
			// Random position along floor depth - FULLY contained within field bounds
			float localZ = Random::GenerateFloat(-halfSpanZ, halfSpanZ);
			float localX = Random::GenerateFloat(-halfSpanX, halfSpanX);
			float yOffset = -CLICK(0.25f);

			Vector3 lightPos = Vector3(
				item.Pose.Position.x + (localX * cosY - localZ * sinY),
				item.Pose.Position.y + (yOffset - CLICK(0.75f)),
				item.Pose.Position.z + (localX * sinY + localZ * cosY));

			// Reduced flicker range
			float flicker = Random::GenerateFloat(0.9f, 1.1f);
			Color lightColor = Color(0.6f * flicker, 0.9f * flicker, 1.50f * flicker);

			// Smaller radius to keep light more contained
			float lightRadius = std::min(BLOCK(3.0f), halfSpanZ * 1.5f);
			SpawnDynamicPointLight(lightPos, lightColor, lightRadius, false, item.Index);  // No shadows
		}
	}

	static bool IsEntityInField(const ItemInfo& electricFieldItem, const ItemInfo& entity, float halfSpanX, float halfSpanZ, float cosY, float sinY, bool isWallMode)
	{
		float worldDeltaX = entity.Pose.Position.x - electricFieldItem.Pose.Position.x;
		float worldDeltaZ = entity.Pose.Position.z - electricFieldItem.Pose.Position.z;

		float localX = worldDeltaX * cosY + worldDeltaZ * sinY;
		float localZ = -worldDeltaX * sinY + worldDeltaZ * cosY;

		auto bounds = GameBoundingBox(&entity);
		int	 entityTop = entity.Pose.Position.y + bounds.Y1;
		int	 entityBottom = entity.Pose.Position.y + bounds.Y2;

		if (isWallMode)
		{
			if (abs(localX) > halfSpanX)
				return false;

			int wallHeight = BLOCK(2);
			int fieldTop = electricFieldItem.Pose.Position.y - wallHeight;
			int fieldBottom = electricFieldItem.Pose.Position.y + wallHeight;

			if (entityTop > fieldBottom || entityBottom < fieldTop)
				return false;

			int fieldThickness = CLICK(0.5f);
			if (abs(localZ + CLICK(1)) > fieldThickness)
				return false;

			return true;
		}
		else
		{
			if (abs(localX) > halfSpanX || abs(localZ) > halfSpanZ)
				return false;

			int fieldThickness = CLICK(0.25f);
			int fieldTop = electricFieldItem.Pose.Position.y - fieldThickness;
			int fieldBottom = electricFieldItem.Pose.Position.y + fieldThickness;

			if (entityTop < fieldBottom && entityBottom > fieldTop)
				return true;

			return false;
		}
	}

	static void KillEntity(ItemInfo& entity)
	{
		if (entity.IsLara())
		{
			auto& player = GetLaraInfo(entity);

			if (player.Context.Vehicle == NO_VALUE && !player.Control.IsLow && !entity.Animation.IsAirborne)
			{
				SetAnimation(entity, ID_LARA_EXTRA_ANIMS, LEA_ELECTROCUTION_DEATH);
				entity.Animation.FrameNumber = 0;
				player.Control.IsMoving = false;
				player.Control.HandStatus = HandStatus::Busy;
				AnimateItem(entity);
			}

			entity.HitPoints = 0;
			ItemElectricBurn(&entity, ELECTRIC_FIELD_DAMAGE);
			ItemBlueElectricBurn(&entity, 2 * FPS);
		}
		else
		{
			DoDamage(&entity, ELECTRIC_FIELD_DAMAGE);
			ItemElectricBurn(&entity, ELECTRIC_FIELD_DAMAGE);
			ItemBlueElectricBurn(&entity, 2 * FPS);
		}
	}

	static void CheckCollisions(const ItemInfo& item, float halfSpanX, float halfSpanZ, float cosY, float sinY, bool isWallMode)
	{
		// OPTIMIZATION: Only check items in current room and neighbor rooms (not all items in level!)
		auto nearbyItems = std::vector<ItemInfo*>{};

		// Check current room
		const auto& room = g_Level.Rooms[item.RoomNumber];
		int itemNumber = room.itemNumber;
		while (itemNumber != NO_VALUE)
		{
			auto& entity = g_Level.Items[itemNumber];

			if (entity.HitPoints > 0 && (entity.IsLara() || entity.IsCreature()))
				nearbyItems.push_back(&entity);

			itemNumber = entity.NextItem;
		}

		// Check neighbor rooms
		for (int neighborRoomNumber : room.NeighborRoomNumbers)
		{
			const auto& neighborRoom = g_Level.Rooms[neighborRoomNumber];
			itemNumber = neighborRoom.itemNumber;
			while (itemNumber != NO_VALUE)
			{
				auto& entity = g_Level.Items[itemNumber];

				if (entity.HitPoints > 0 && (entity.IsLara() || entity.IsCreature()))
					nearbyItems.push_back(&entity);

				itemNumber = entity.NextItem;
			}
		}

		// Now check only nearby items (typically 5-20 instead of 500+)
		for (auto* entity : nearbyItems)
		{
			if (IsEntityInField(item, *entity, halfSpanX, halfSpanZ, cosY, sinY, isWallMode))
				KillEntity(*entity);
		}
	}

	void ControlElectricField(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		if (!TriggerActive(&item))
			return;

		bool isWallMode = (item.TriggerFlags < 0);

		// OCB directly represents total width in blocks
		// Positive OCB = floor mode depth, Negative OCB = wall mode width
		int totalBlocks = abs(item.TriggerFlags);
		if (totalBlocks == 0)
			totalBlocks = 1;  // Default to 1 block if OCB is 0

		float halfSpanX, halfSpanZ;

		if (isWallMode)
		{
			// Wall mode: width is variable (OCB), depth is always 1 block
			halfSpanX = totalBlocks * BLOCK(0.5f);  // Center-aligned width
			halfSpanZ = BLOCK(0.5f);
		}
		else
		{
			// Floor mode: width is always 1 block, depth is variable (OCB)
			halfSpanX = BLOCK(0.5f);
			halfSpanZ = totalBlocks * BLOCK(0.5f);  // Center-aligned depth
		}

		float rotY = TO_RAD(item.Pose.Orientation.y);
		float cosY = cos(rotY);
		float sinY = sin(rotY);

		SpawnSparks(item, halfSpanX, halfSpanZ, cosY, sinY, isWallMode);
		SpawnFloorLight(item, halfSpanX, halfSpanZ, cosY, sinY, isWallMode);
		CheckCollisions(item, halfSpanX, halfSpanZ, cosY, sinY, isWallMode);
	}
}