#include "framework.h"
#include "Objects/TR3/Trap/ElectricFloor.h"
#include "Game/Lara/lara_helpers.h"

#include "Game/effects/Electricity.h"
#include "Game/effects/spark.h"
#include "Game/effects/item_fx.h"
#include "Game/items.h"
#include "Renderer/Renderer.h"
#include "Sound/sound.h"

using namespace TEN::Control::Volumes;
using namespace TEN::Effects::Electricity;
using namespace TEN::Effects::Spark;
using namespace TEN::Effects::Items;
using namespace TEN::Math;
using namespace TEN::Renderer;

namespace TEN::Entities::Traps
{
	constexpr auto ELECTRIC_FLOOR_DAMAGE = INT_MAX;

	void InitializeElectricFloor(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];
		item.Status = ITEM_ACTIVE;
	}

	static void DrawElectricFloorDebug(const ItemInfo& item, float halfSpanX, float halfSpanZ, float cosY, float sinY)
	{
		if (g_Renderer.GetCurrentDebugPage() != RendererDebugPage::CollisionStats)
			return;

		Vector3 center = item.Pose.Position.ToVector3();

		// Calculate rotated corners
		Vector3 corners[4];
		corners[0] = Vector3(-halfSpanX, 0, -halfSpanZ); // Min X, Min Z
		corners[1] = Vector3(halfSpanX, 0, -halfSpanZ);  // Max X, Min Z
		corners[2] = Vector3(halfSpanX, 0, halfSpanZ);   // Max X, Max Z
		corners[3] = Vector3(-halfSpanX, 0, halfSpanZ);  // Min X, Max Z

		// Rotate and translate corners
		for (int i = 0; i < 4; i++)
		{
			float x = corners[i].x;
			float z = corners[i].z;
			corners[i].x = center.x + (x * cosY - z * sinY);
			corners[i].y = center.y;
			corners[i].z = center.z + (x * sinY + z * cosY);
		}

		// Draw outline of the electric floor area
		g_Renderer.AddDebugLine(corners[0], corners[1], Color(0, 1, 1, 1)); // Cyan
		g_Renderer.AddDebugLine(corners[1], corners[2], Color(0, 1, 1, 1)); // Cyan
		g_Renderer.AddDebugLine(corners[2], corners[3], Color(0, 1, 1, 1)); // Cyan
		g_Renderer.AddDebugLine(corners[3], corners[0], Color(0, 1, 1, 1)); // Cyan

		// Draw "Electric Death" label at center
		constexpr auto ELECTRIC_DEATH_COLOR = Vector4(0.4f, 0.6f, 1.0f, 1.0f);
		DrawDebugString("Electric Death", center, ELECTRIC_DEATH_COLOR, RendererDebugPage::CollisionStats);
	}


	void ControlElectricFloor(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		if (item.Status != ITEM_ACTIVE)
			return;

		// Get span from OCB - OCB value is how many blocks to add on EACH side
		// OCB 0 = 1 block (just center)
		// OCB 1 = 3 blocks (center + 1 on each side)
		// OCB 2 = 5 blocks (center + 2 on each side)
		int extraBlocksEachSide = item.TriggerFlags;
		int totalSpanX = 1 + (2 * extraBlocksEachSide); // 1 center block + extra on both sides
		int spanZ = 1; // Always 1 block in Z direction

		float halfSpanX = totalSpanX * BLOCK(0.5f);
		float halfSpanZ = spanZ * BLOCK(0.5f);

		// Get rotation angle
		float rotY = TO_RAD(item.Pose.Orientation.y);
		float cosY = cos(rotY);
		float sinY = sin(rotY);

		// Debug visualization
		DrawElectricFloorDebug(item, halfSpanX, halfSpanZ, cosY, sinY);

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

		// Spawn electric sparks at random positions within the floor span
		if (Random::TestProbability(0.5f))
		{
			// Random position within the span (local coordinates)
			float localX = Random::GenerateFloat(-halfSpanX, halfSpanX);
			float localZ = Random::GenerateFloat(-halfSpanZ, halfSpanZ);

			// Rotate to world coordinates
			int sparkX = item.Pose.Position.x + (localX * cosY - localZ * sinY);
			int sparkY = item.Pose.Position.y;
			int sparkZ = item.Pose.Position.z + (localX * sinY + localZ * cosY);

			// Spawn 1-2 custom electric sparks
			int sparkCount = Random::GenerateInt(1, 2);
			for (int i = 0; i < sparkCount; i++)
			{
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

				// Sparks shoot upward with slight random variation
				float ang = TO_RAD(Random::GenerateAngle());
				float vertAng = -TO_RAD(90.0f); // Upward (negative Y velocity)
				Vector3 v = Vector3(sin(ang), vertAng + Random::GenerateFloat(-PI / 16, PI / 16), cos(ang));
				v.Normalize(v);
				s.velocity = v * Random::GenerateFloat(8, 32);
				s.sourceColor = Vector4(0.4f, 0.6f, 1.0f, 1);
				s.destinationColor = Vector4(0.6f, 0.6f, 0.8f, 0.8f);
				s.active = true;
			}

			// Spawn small dynamic light at spark position
			SpawnDynamicLight(sparkX, sparkY, sparkZ, 3, 102, 153, 255);

			// Occasional sound effect
			if (Random::TestProbability(0.25f))
				SoundEffect(SFX_TR5_ELECTRIC_LIGHT_CRACKLES, &item.Pose);
		}

		// Check all creatures for collision with electric floor
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

			// Transform entity position to local space (relative to electric floor)
			float worldDeltaX = entity.Pose.Position.x - item.Pose.Position.x;
			float worldDeltaZ = entity.Pose.Position.z - item.Pose.Position.z;

			// Rotate to local space (inverse rotation)
			float localX = worldDeltaX * cosY + worldDeltaZ * sinY;
			float localZ = -worldDeltaX * sinY + worldDeltaZ * cosY;

			// Check if entity is within the rotated electric floor span
			if (abs(localX) <= halfSpanX && abs(localZ) <= halfSpanZ)
			{
				short electricFloorRoomNum = item.RoomNumber;
				auto* electricFloorFloor = GetFloor(item.Pose.Position.x, item.Pose.Position.y, item.Pose.Position.z, &electricFloorRoomNum);
				int electricFloorFloorHeight = GetFloorHeight(electricFloorFloor, item.Pose.Position.x, item.Pose.Position.y, item.Pose.Position.z);

				// Check if entity's ACTUAL position is at floor level
				if (abs(entity.Pose.Position.y - electricFloorFloorHeight) < CLICK(1))
					shouldKill = true;
			}

			if (shouldKill)
			{
				// Special death animation for Lara
				if (entity.IsLara())
				{
					auto& player = GetLaraInfo(entity);

					SetAnimation(entity, ID_LARA_EXTRA_ANIMS, LEA_ELECTROCUTION_DEATH);
					entity.Animation.FrameNumber = 0;
					player.Control.IsMoving = false;
					player.Control.HandStatus = HandStatus::Busy;
					AnimateItem(entity);

					// Trigger death sequence
					entity.HitPoints = 0;

					// Apply visual effects
					ItemElectricBurn(&entity, ELECTRIC_FLOOR_DAMAGE);
					ItemBlueElectricBurn(&entity, 2 * FPS);
				}
				else
				{
					// Regular damage for creatures
					DoDamage(&entity, ELECTRIC_FLOOR_DAMAGE);
					ItemElectricBurn(&entity, ELECTRIC_FLOOR_DAMAGE);
					ItemBlueElectricBurn(&entity, 2 * FPS);
				}
			}
		}
	}
}





/*
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
		} */