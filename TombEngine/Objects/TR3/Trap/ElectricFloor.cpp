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

	static void SpawnSparks(const ItemInfo& item, float halfSpanX, float halfSpanZ, float cosY, float sinY)
	{
		if (!Random::TestProbability(0.6f))
			return;

		float localX = Random::GenerateFloat(-halfSpanX, halfSpanX);
		float localZ = Random::GenerateFloat(-halfSpanZ, halfSpanZ);

		float sparkX = item.Pose.Position.x + (localX * cosY - localZ * sinY);
		float sparkY = item.Pose.Position.y;
		float sparkZ = item.Pose.Position.z + (localX * sinY + localZ * cosY);

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

			float ang = TO_RAD(Random::GenerateAngle());
			float vertAng = -TO_RAD(90.0f);
			Vector3 v = Vector3(sin(ang), vertAng + Random::GenerateFloat(-PI / 16, PI / 16), cos(ang));
			v.Normalize(v);
			s.velocity = v * Random::GenerateFloat(8, 32);
			s.sourceColor = Vector4(0.4f, 0.6f, 1.0f, 1);
			s.destinationColor = Vector4(0.6f, 0.6f, 0.8f, 0.8f);
			s.active = true;
		}

		if (Random::TestProbability(0.25f))
		{
			SoundEffect(SFX_TR5_ELECTRIC_LIGHT_CRACKLES, const_cast<Pose*>(&item.Pose));

			if (Random::TestProbability(0.1f))
			{
				Vector3 lightPos = Vector3(sparkX, sparkY, sparkZ);
				Color	lightColor = Color(0.4f, 0.6f, 1.0f);
				SpawnDynamicPointLight(lightPos, lightColor, BLOCK(3), false, 0);
			}
		}
	}

	static void SpawnLightning(const ItemInfo& item, float halfSpanX, float halfSpanZ, float cosY, float sinY)
	{
		if (!Random::TestProbability(0.15f))
			return;

		float leftX = -halfSpanX / Random::GenerateInt(1, 2);
		float rightX = halfSpanX / Random::GenerateInt(1, 2);
		float randomZ = Random::GenerateFloat(-halfSpanZ, halfSpanZ);
		float yOffset = -CLICK(0.25f);

		Vector3 origin = Vector3(
			item.Pose.Position.x + (leftX * cosY - randomZ * sinY),
			item.Pose.Position.y + yOffset,
			item.Pose.Position.z + (leftX * sinY + randomZ * cosY));

		Vector3 target = Vector3(
			item.Pose.Position.x + (rightX * cosY - randomZ * sinY),
			item.Pose.Position.y + yOffset,
			item.Pose.Position.z + (rightX * sinY + randomZ * cosY));

		float r = Random::GenerateInt(32, 128) / 255.0f;
		float g = Random::GenerateInt(128, 192) / 255.0f;
		float b = Random::GenerateInt(192, 255) / 255.0f;

		SpawnElectricity(
			origin,
			target,
			Random::GenerateInt(8, 16),
			r * 255, g * 255, b * 255,
			Random::GenerateInt(16, 32),
			(int)ElectricityFlags::Spline | (int)ElectricityFlags::ThinIn | (int)ElectricityFlags::SparkEnd,
			Random::GenerateFloat(8.0f, 16.0f),
			Random::GenerateInt(1, 2));

		Vector3 lightPos = Vector3(
			item.Pose.Position.x + (Random::GenerateFloat(-halfSpanX, halfSpanX) * cosY - randomZ * sinY),
			item.Pose.Position.y + yOffset,
			item.Pose.Position.z + (Random::GenerateFloat(-halfSpanX, halfSpanX) * sinY + randomZ * cosY));
		Color lightColor = Color(0.4f, 0.6f, 1.0f);
		SpawnDynamicPointLight(lightPos, lightColor, BLOCK(3), false, 0);
	}

	static void SpawnSweepingLight(ItemInfo& item, float halfSpanX, float halfSpanZ, float cosY, float sinY)
	{
		int	  extraBlocksEachSide = item.TriggerFlags;
		int	  totalSpanX = 1 + (2 * extraBlocksEachSide);
		float travelTime = totalSpanX * FPS * 0.30f;
		float stepSize = 1000.0f / travelTime;

		item.ItemFlags[7] += stepSize;
		if (item.ItemFlags[7] >= 1000)
			item.ItemFlags[7] = 0;

		float normalizedPos = (item.ItemFlags[7] / 1000.0f) * 2.0f - 1.0f;
		float localX = normalizedPos * halfSpanX;
		float localZ = Random::GenerateFloat(-halfSpanZ * 0.3f, halfSpanZ * 0.3f);
		float yOffset = -CLICK(0.25f);

		Vector3 lightPos = Vector3(
			item.Pose.Position.x + (localX * cosY - localZ * sinY),
			item.Pose.Position.y + (yOffset - CLICK(0.75f)),
			item.Pose.Position.z + (localX * sinY + localZ * cosY));

		float flicker = Random::GenerateFloat(0.8f, 1.2f);
		Color lightColor = Color(0.6f * flicker, 0.9f * flicker, 1.50f * flicker);
		SpawnDynamicPointLight(lightPos, lightColor, BLOCK(5.0f), true, item.Index);
	}

	static bool IsEntityInField(const ItemInfo& electricFloorItem, const ItemInfo& entity, float halfSpanX, float halfSpanZ, float cosY, float sinY)
	{
		float worldDeltaX = entity.Pose.Position.x - electricFloorItem.Pose.Position.x;
		float worldDeltaZ = entity.Pose.Position.z - electricFloorItem.Pose.Position.z;

		float localX = worldDeltaX * cosY + worldDeltaZ * sinY;
		float localZ = -worldDeltaX * sinY + worldDeltaZ * cosY;

		if (abs(localX) > halfSpanX || abs(localZ) > halfSpanZ)
			return false;

		short electricFloorRoomNum = electricFloorItem.RoomNumber;
		auto* electricFloorFloor = GetFloor(electricFloorItem.Pose.Position.x, electricFloorItem.Pose.Position.y, electricFloorItem.Pose.Position.z, &electricFloorRoomNum);
		int	  electricFloorHeight = GetFloorHeight(electricFloorFloor, electricFloorItem.Pose.Position.x, electricFloorItem.Pose.Position.y, electricFloorItem.Pose.Position.z);

		return (abs(entity.Pose.Position.y - electricFloorHeight) < CLICK(1));
	}

	static void KillEntity(ItemInfo& entity)
	{
		if (entity.IsLara())
		{
			auto& player = GetLaraInfo(entity);

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

	static void CheckCollisions(const ItemInfo& item, float halfSpanX, float halfSpanZ, float cosY, float sinY)
	{
		for (auto& entity : g_Level.Items)
		{
			if (entity.HitPoints <= 0)
				continue;

			if (!entity.IsLara() && !entity.IsCreature())
				continue;

			if (entity.Animation.IsAirborne)
				continue;

			if (IsEntityInField(item, entity, halfSpanX, halfSpanZ, cosY, sinY))
				KillEntity(entity);
		}
	}

	void ControlElectricFloor(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		if (item.Status != ITEM_ACTIVE)
			return;

		int extraBlocksEachSide = item.TriggerFlags;
		int totalSpanX = 1 + (2 * extraBlocksEachSide);
		int spanZ = 1;

		float halfSpanX = totalSpanX * BLOCK(0.5f);
		float halfSpanZ = spanZ * BLOCK(0.5f);

		float rotY = TO_RAD(item.Pose.Orientation.y);
		float cosY = cos(rotY);
		float sinY = sin(rotY);

		SpawnSparks(item, halfSpanX, halfSpanZ, cosY, sinY);
		SpawnLightning(item, halfSpanX, halfSpanZ, cosY, sinY);
		SpawnSweepingLight(item, halfSpanX, halfSpanZ, cosY, sinY);
		CheckCollisions(item, halfSpanX, halfSpanZ, cosY, sinY);
	}
}