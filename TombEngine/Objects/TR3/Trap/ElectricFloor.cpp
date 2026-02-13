#include "framework.h"
#include "Objects/TR3/Trap/ElectricFloor.h"
#include "Game/Lara/lara_helpers.h"

#include "Game/effects/Electricity.h"
#include "Game/effects/spark.h"
#include "Game/effects/item_fx.h"
#include "Game/items.h"
#include "Renderer/Renderer.h"
#include "Sound/sound.h"

using namespace TEN::Effects::Spark;
using namespace TEN::Effects::Items;

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
		corners[0] = Vector3(-halfSpanX, 0, -halfSpanZ);
		corners[1] = Vector3(halfSpanX, 0, -halfSpanZ);
		corners[2] = Vector3(halfSpanX, 0, halfSpanZ);
		corners[3] = Vector3(-halfSpanX, 0, halfSpanZ);

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
		g_Renderer.AddDebugLine(corners[0], corners[1], Color(0, 1, 1, 1));
		g_Renderer.AddDebugLine(corners[1], corners[2], Color(0, 1, 1, 1));
		g_Renderer.AddDebugLine(corners[2], corners[3], Color(0, 1, 1, 1));
		g_Renderer.AddDebugLine(corners[3], corners[0], Color(0, 1, 1, 1));

		// Draw "Electric Death" label at center
		constexpr auto ELECTRIC_DEATH_COLOR = Vector4(0.4f, 0.6f, 1.0f, 1.0f);
		DrawDebugString("Electric Death", center, ELECTRIC_DEATH_COLOR, RendererDebugPage::CollisionStats);
	}

	static void SpawnElectricFloorEffects(const ItemInfo& item, float halfSpanX, float halfSpanZ, float cosY, float sinY)
	{
		if (!Random::TestProbability(0.5f))
			return;

		// Random position within the span (local coordinates)
		float localX = Random::GenerateFloat(-halfSpanX, halfSpanX);
		float localZ = Random::GenerateFloat(-halfSpanZ, halfSpanZ);

		// Rotate to world coordinates
		int sparkX = item.Pose.Position.x + (localX * cosY - localZ * sinY);
		int sparkY = item.Pose.Position.y;
		int sparkZ = item.Pose.Position.z + (localX * sinY + localZ * cosY);

		// Spawn electric sparks
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
			float vertAng = -TO_RAD(90.0f);
			Vector3 v = Vector3(sin(ang), vertAng + Random::GenerateFloat(-PI / 16, PI / 16), cos(ang));
			v.Normalize(v);
			s.velocity = v * Random::GenerateFloat(8, 32);
			s.sourceColor = Vector4(0.4f, 0.6f, 1.0f, 1);
			s.destinationColor = Vector4(0.6f, 0.6f, 0.8f, 0.8f);
			s.active = true;
		}

		// Occasional sound effect and dynamic light
		if (Random::TestProbability(0.25f))
		{
			SoundEffect(SFX_TR5_ELECTRIC_LIGHT_CRACKLES, const_cast<Pose*>(&item.Pose));

			if (Random::TestProbability(0.1f))
				SpawnDynamicLight(sparkX, sparkY, sparkZ, 10, 102, 153, 255);
		}
	}
	static bool IsEntityInElectricField(const ItemInfo& electricFloorItem, const ItemInfo& entity, float halfSpanX, float halfSpanZ, float cosY, float sinY)
	{
		// Transform entity position to local space (relative to electric floor)
		float worldDeltaX = entity.Pose.Position.x - electricFloorItem.Pose.Position.x;
		float worldDeltaZ = entity.Pose.Position.z - electricFloorItem.Pose.Position.z;

		// Rotate to local space (inverse rotation)
		float localX = worldDeltaX * cosY + worldDeltaZ * sinY;
		float localZ = -worldDeltaX * sinY + worldDeltaZ * cosY;

		// Check if entity is within the rotated electric floor span
		if (abs(localX) > halfSpanX || abs(localZ) > halfSpanZ)
			return false;

		// Check if entity is at floor level
		short electricFloorRoomNum = electricFloorItem.RoomNumber;
		auto* electricFloorFloor = GetFloor(electricFloorItem.Pose.Position.x, electricFloorItem.Pose.Position.y, electricFloorItem.Pose.Position.z, &electricFloorRoomNum);
		int electricFloorFloorHeight = GetFloorHeight(electricFloorFloor, electricFloorItem.Pose.Position.x, electricFloorItem.Pose.Position.y, electricFloorItem.Pose.Position.z);

		return (abs(entity.Pose.Position.y - electricFloorFloorHeight) < CLICK(1));
	}

	static void KillEntity(ItemInfo& entity)
	{
		if (entity.IsLara())
		{
			auto& player = GetLaraInfo(entity);

			// Only trigger electrocution death if Lara is not in a vehicle
			if (player.Context.Vehicle == NO_VALUE)
			{
				SetAnimation(entity, ID_LARA_EXTRA_ANIMS, LEA_ELECTROCUTION_DEATH);
				entity.Animation.FrameNumber = 0;
				player.Control.IsMoving = false;
				player.Control.HandStatus = HandStatus::Busy;
				AnimateItem(entity);
			}

			entity.HitPoints = 0;

			ItemElectricBurn(&entity, ELECTRIC_FLOOR_DAMAGE);
			ItemBlueElectricBurn(&entity, 2 * FPS);
		}
		else
		{
			DoDamage(&entity, ELECTRIC_FLOOR_DAMAGE);
			ItemElectricBurn(&entity, ELECTRIC_FLOOR_DAMAGE);
			ItemBlueElectricBurn(&entity, 2 * FPS);
		}
	}
	static void CheckElectricFloorCollisions(const ItemInfo& item, float halfSpanX, float halfSpanZ, float cosY, float sinY)
	{
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

			// Check if entity is in electric field and kill if true
			if (IsEntityInElectricField(item, entity, halfSpanX, halfSpanZ, cosY, sinY))
				KillEntity(entity);
		}
	}

	void ControlElectricFloor(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		if (item.Status != ITEM_ACTIVE)
			return;

		// Calculate span from OCB
		int extraBlocksEachSide = item.TriggerFlags;
		int totalSpanX = 1 + (2 * extraBlocksEachSide);
		int spanZ = 1;

		float halfSpanX = totalSpanX * BLOCK(0.5f);
		float halfSpanZ = spanZ * BLOCK(0.5f);

		// Calculate rotation
		float rotY = TO_RAD(item.Pose.Orientation.y);
		float cosY = cos(rotY);
		float sinY = sin(rotY);

		// Execute electric floor systems
		DrawElectricFloorDebug(item, halfSpanX, halfSpanZ, cosY, sinY);
		SpawnElectricFloorEffects(item, halfSpanX, halfSpanZ, cosY, sinY);
		CheckElectricFloorCollisions(item, halfSpanX, halfSpanZ, cosY, sinY);
	}
}