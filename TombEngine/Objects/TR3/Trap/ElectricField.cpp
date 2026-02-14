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

	void InitializeElectricField(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];
		item.Status = ITEM_ACTIVE;
	}

	static void SpawnSparks(const ItemInfo& item, float halfSpanX, float halfSpanZ, float cosY, float sinY, bool isWallMode)
	{
		if (!Random::TestProbability(0.6f))
			return;

		float localX = Random::GenerateFloat(-halfSpanX, halfSpanX);
		float localY = isWallMode ? Random::GenerateFloat(-BLOCK(1), BLOCK(1)) : 0;
		float localZ = isWallMode ? -CLICK(1) : Random::GenerateFloat(-halfSpanZ, halfSpanZ);

		float sparkX = item.Pose.Position.x + (localX * cosY - localZ * sinY);
		float sparkY = item.Pose.Position.y + localY;
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

	static void SpawnLightning(const ItemInfo& item, float halfSpanX, float halfSpanZ, float cosY, float sinY, bool isWallMode)
	{
		if (!Random::TestProbability(0.15f))
			return;

		float yOffset = isWallMode ? 0 : -CLICK(0.25f);

		if (isWallMode)
		{
			float leftX = -halfSpanX / Random::GenerateInt(1, 2);
			float rightX = halfSpanX / Random::GenerateInt(1, 2);
			float randomY = Random::GenerateFloat(-BLOCK(0.5f), BLOCK(0.5f));
			float wallZ = -CLICK(1);

			Vector3 origin = Vector3(
				item.Pose.Position.x + (leftX * cosY - wallZ * sinY),
				item.Pose.Position.y + randomY,
				item.Pose.Position.z + (leftX * sinY + wallZ * cosY));

			Vector3 target = Vector3(
				item.Pose.Position.x + (rightX * cosY - wallZ * sinY),
				item.Pose.Position.y + randomY,
				item.Pose.Position.z + (rightX * sinY + wallZ * cosY));

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
				item.Pose.Position.x + (Random::GenerateFloat(-halfSpanX, halfSpanX) * cosY - wallZ * sinY),
				item.Pose.Position.y + randomY,
				item.Pose.Position.z + (Random::GenerateFloat(-halfSpanX, halfSpanX) * sinY + wallZ * cosY));
			Color lightColor = Color(0.4f, 0.6f, 1.0f);
			SpawnDynamicPointLight(lightPos, lightColor, BLOCK(3), false, 0);
		}
		else
		{
			float leftZ = -halfSpanZ / Random::GenerateInt(1, 2);
			float rightZ = halfSpanZ / Random::GenerateInt(1, 2);
			float randomX = Random::GenerateFloat(-halfSpanX, halfSpanX);

			Vector3 origin = Vector3(
				item.Pose.Position.x + (randomX * cosY - leftZ * sinY),
				item.Pose.Position.y + yOffset,
				item.Pose.Position.z + (randomX * sinY + leftZ * cosY));

			Vector3 target = Vector3(
				item.Pose.Position.x + (randomX * cosY - rightZ * sinY),
				item.Pose.Position.y + yOffset,
				item.Pose.Position.z + (randomX * sinY + rightZ * cosY));

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
				item.Pose.Position.x + (randomX * cosY - Random::GenerateFloat(-halfSpanZ, halfSpanZ) * sinY),
				item.Pose.Position.y + yOffset,
				item.Pose.Position.z + (randomX * sinY + Random::GenerateFloat(-halfSpanZ, halfSpanZ) * cosY));
			Color lightColor = Color(0.4f, 0.6f, 1.0f);
			SpawnDynamicPointLight(lightPos, lightColor, BLOCK(3), false, 0);
		}
	}

	static void SpawnSweepingLight(ItemInfo& item, float halfSpanX, float halfSpanZ, float cosY, float sinY, bool isWallMode)
	{
		int	  extraBlocksEachSide = abs(item.TriggerFlags);
		int	  totalSpanZ = 1 + (2 * extraBlocksEachSide);
		float travelTime = totalSpanZ * FPS * 0.30f;
		float stepSize = 1000.0f / travelTime;

		item.ItemFlags[7] += stepSize;
		if (item.ItemFlags[7] >= 1000)
			item.ItemFlags[7] = 0;

		float normalizedPos = (item.ItemFlags[7] / 1000.0f) * 2.0f - 1.0f;

		if (isWallMode)
		{
			float wallZ = -CLICK(1);
			float localY = Random::GenerateFloat(-BLOCK(0.3f), BLOCK(0.3f));
			float localX = normalizedPos * halfSpanX;

			Vector3 lightPos = Vector3(
				item.Pose.Position.x + (localX * cosY - wallZ * sinY),
				item.Pose.Position.y + localY,
				item.Pose.Position.z + (localX * sinY + wallZ * cosY));

			float flicker = Random::GenerateFloat(0.8f, 1.2f);
			Color lightColor = Color(0.6f * flicker, 0.9f * flicker, 1.50f * flicker);
			SpawnDynamicPointLight(lightPos, lightColor, BLOCK(5.0f), true, item.Index);
		}
		else
		{
			float localZ = normalizedPos * halfSpanZ;
			float localX = Random::GenerateFloat(-halfSpanX * 0.3f, halfSpanX * 0.3f);
			float yOffset = -CLICK(0.25f);

			Vector3 lightPos = Vector3(
				item.Pose.Position.x + (localX * cosY - localZ * sinY),
				item.Pose.Position.y + (yOffset - CLICK(0.75f)),
				item.Pose.Position.z + (localX * sinY + localZ * cosY));

			float flicker = Random::GenerateFloat(0.8f, 1.2f);
			Color lightColor = Color(0.6f * flicker, 0.9f * flicker, 1.50f * flicker);
			SpawnDynamicPointLight(lightPos, lightColor, BLOCK(5.0f), true, item.Index);
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
		for (auto& entity : g_Level.Items)
		{
			if (entity.HitPoints <= 0)
				continue;

			if (!entity.IsLara() && !entity.IsCreature())
				continue;

			if (IsEntityInField(item, entity, halfSpanX, halfSpanZ, cosY, sinY, isWallMode))
				KillEntity(entity);
		}
	}

	void ControlElectricField(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		if (!TriggerActive(&item))
			return;

		bool isWallMode = (item.TriggerFlags < 0);
		int	 extraBlocksEachSide = isWallMode ? (abs(item.TriggerFlags) - 1) : item.TriggerFlags;

		float halfSpanX, halfSpanZ;

		if (isWallMode)
		{
			int totalSpanX = 1 + (2 * extraBlocksEachSide);
			int spanZ = 1;
			halfSpanX = totalSpanX * BLOCK(0.5f);
			halfSpanZ = spanZ * BLOCK(0.5f);
		}
		else
		{
			int totalSpanZ = 1 + (2 * extraBlocksEachSide);
			int spanX = 1;
			halfSpanX = spanX * BLOCK(0.5f);
			halfSpanZ = totalSpanZ * BLOCK(0.5f);
		}

		float rotY = TO_RAD(item.Pose.Orientation.y);
		float cosY = cos(rotY);
		float sinY = sin(rotY);

		SpawnSparks(item, halfSpanX, halfSpanZ, cosY, sinY, isWallMode);
		SpawnLightning(item, halfSpanX, halfSpanZ, cosY, sinY, isWallMode);
		SpawnSweepingLight(item, halfSpanX, halfSpanZ, cosY, sinY, isWallMode);
		CheckCollisions(item, halfSpanX, halfSpanZ, cosY, sinY, isWallMode);
	}
}