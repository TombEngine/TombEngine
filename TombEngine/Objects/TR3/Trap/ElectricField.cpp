#include "framework.h"
#include "Objects/TR3/Trap/ElectricField.h"

#include "Game/collision/floordata.h"
#include "Game/collision/Point.h"
#include "Game/control/trigger.h"
#include "Game/effects/item_fx.h"
#include "Game/effects/spark.h"
#include "Game/items.h"
#include "Game/Lara/lara_helpers.h"
#include "Renderer/Renderer.h"
#include "Sound/sound.h"

using namespace TEN::Collision::Floordata;
using namespace TEN::Collision::Point;
using namespace TEN::Effects::Items;
using namespace TEN::Effects::Spark;
using namespace TEN::Math::Random;

namespace TEN::Entities::Traps
{
	constexpr auto ELECTRIC_FIELD_DAMAGE = INT_MAX;
	constexpr auto EFFECT_UPDATE_INTERVAL = 2;
	constexpr auto SOUND_UPDATE_INTERVAL = 10;
	constexpr auto SPARK_SPAWN_PROBABILITY = 0.4f;
	constexpr auto SOUND_PROBABILITY = 0.25f;
	constexpr auto SPARK_LIGHT_PROBABILITY = 0.05f;
	constexpr auto FLOOR_LIGHT_INTERVAL = 5;
	constexpr auto FLOOR_LIGHT_PROBABILITY = 0.2f;
	constexpr auto WALL_FIELD_THICKNESS = CLICK(0.5f);
	constexpr auto FLOOR_FIELD_THICKNESS = CLICK(0.25f);
	constexpr auto BRIDGE_DETECTION_TOLERANCE = CLICK(0.5f);
	constexpr auto WALL_FORWARD_OFFSET = CLICK(1.75f);

	void InitializeElectricField(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];
		item.Status = ITEM_ACTIVE;

		auto& sector = GetFloor(item.RoomNumber, item.Pose.Position.x, item.Pose.Position.z);
		int ceilingHeight = sector.GetSurfaceHeight(item.Pose.Position.x, item.Pose.Position.z, false);

		item.ItemFlags[0] = item.Pose.Position.y - ceilingHeight;
	}

	static void SpawnSparks(
		const Vector3& center,
		int roomNumber,
		float halfSpanX,
		float halfSpanZ,
		float cosY,
		float sinY,
		bool isWallMode,
		float wallHeight)
	{
		if (GlobalCounter % EFFECT_UPDATE_INTERVAL != 0)
			return;

		if (!Random::TestProbability(SPARK_SPAWN_PROBABILITY))
			return;

		float localX = Random::GenerateFloat(-halfSpanX, halfSpanX);
		float localY = isWallMode ? Random::GenerateFloat(-wallHeight, 0.0f) : 0.0f;
		float localZ = isWallMode ? 0.0f : Random::GenerateFloat(-halfSpanZ, halfSpanZ);

		float sparkX = center.x + (localX * cosY - localZ * sinY);
		float sparkY = center.y + localY;
		float sparkZ = center.z + (localX * sinY + localZ * cosY);

		auto& spark = GetFreeSparkParticle();

		spark = {};
		spark.age = 0;
		spark.life = Random::GenerateFloat(10, 18);
		spark.friction = 1.0f;
		spark.gravity = 2.0f;
		spark.height = Random::GenerateFloat(128.0f, 384.0f);
		spark.width = Random::GenerateFloat(16.0f, 32.0f);
		spark.room = roomNumber;
		spark.pos = Vector3(
			sparkX + Random::GenerateFloat(-16, 16),
			sparkY + Random::GenerateFloat(-16, 16),
			sparkZ + Random::GenerateFloat(-16, 16));

		if (isWallMode)
		{
			float direction = Random::TestProbability(0.5f) ? 1.0f : -1.0f;

			auto velocity = Vector3(-sinY * direction, Random::GenerateFloat(-0.2f, 0.2f), -cosY * direction);
			velocity.Normalize(velocity);
			spark.velocity = velocity * Random::GenerateFloat(8, 32);
		}
		else
		{
			float angle = TO_RAD(Random::GenerateAngle());
			float vertAngle = -TO_RAD(90.0f);

			auto velocity = Vector3(sin(angle), vertAngle + Random::GenerateFloat(-PI / 16, PI / 16), cos(angle));
			velocity.Normalize(velocity);
			spark.velocity = velocity * Random::GenerateFloat(8, 32);
		}

		spark.sourceColor = Vector4(0.4f, 0.6f, 1.0f, 1.0f);
		spark.destinationColor = Vector4(0.6f, 0.6f, 0.8f, 0.8f);
		spark.active = true;

		if (GlobalCounter % SOUND_UPDATE_INTERVAL == 0 && Random::TestProbability(SOUND_PROBABILITY))
		{
			auto soundPose = Pose(center);
			SoundEffect(SFX_TR5_ELECTRIC_LIGHT_CRACKLES, &soundPose);

			if (Random::TestProbability(SPARK_LIGHT_PROBABILITY))
			{
				auto lightColor = Color(0.4f, 0.6f, 1.0f);
				SpawnDynamicPointLight(Vector3(sparkX, sparkY, sparkZ), lightColor, BLOCK(2), false, 0);
			}
		}
	}

	static void SpawnFloorLight(
		const Vector3& center,
		int itemIndex,
		int roomNumber,
		float halfSpanX,
		float halfSpanZ,
		float cosY,
		float sinY,
		bool isWallMode,
		float wallHeight)
	{
		if (GlobalCounter % FLOOR_LIGHT_INTERVAL != 0)
			return;

		if (!Random::TestProbability(FLOOR_LIGHT_PROBABILITY))
			return;

		if (isWallMode)
		{
			float localX = Random::GenerateFloat(-halfSpanX, halfSpanX);
			float localY = Random::GenerateFloat(-wallHeight * 0.8f, -wallHeight * 0.2f);

			auto lightPos = Vector3(
				center.x + (localX * cosY),
				center.y + localY,
				center.z + (localX * sinY));

			float flicker = Random::GenerateFloat(0.9f, 1.1f);
			auto lightColor = Color(0.6f * flicker, 0.9f * flicker, 1.5f * flicker);
			float lightRadius = std::min(BLOCK(3.0f), halfSpanX * 1.5f);

			SpawnDynamicPointLight(lightPos, lightColor, lightRadius, false, itemIndex);
		}
		else
		{
			float localX = Random::GenerateFloat(-halfSpanX, halfSpanX);
			float localZ = Random::GenerateFloat(-halfSpanZ, halfSpanZ);

			auto lightPos = Vector3(
				center.x + (localX * cosY - localZ * sinY),
				center.y - CLICK(1),
				center.z + (localX * sinY + localZ * cosY));

			float flicker = Random::GenerateFloat(0.9f, 1.1f);
			auto lightColor = Color(0.6f * flicker, 0.9f * flicker, 1.5f * flicker);
			float lightRadius = std::min(BLOCK(3.0f), halfSpanZ * 1.5f);

			SpawnDynamicPointLight(lightPos, lightColor, lightRadius, false, itemIndex);
		}
	}

	static bool IsEntityInField(
		const ItemInfo& item,
		const Vector3& center,
		const ItemInfo& entity,
		float halfSpanX,
		float halfSpanZ,
		float cosY,
		float sinY,
		bool isWallMode)
	{
		float worldDeltaX = entity.Pose.Position.x - center.x;
		float worldDeltaZ = entity.Pose.Position.z - center.z;

		float localX = worldDeltaX * cosY + worldDeltaZ * sinY;
		float localZ = -worldDeltaX * sinY + worldDeltaZ * cosY;

		auto bounds = GameBoundingBox(&entity);
		int entityTop = entity.Pose.Position.y + bounds.Y1;
		int entityBottom = entity.Pose.Position.y + bounds.Y2;

		if (isWallMode)
		{
			if (abs(localX) > halfSpanX)
				return false;

			int fieldBottom = (int)center.y;
			int fieldTop = (int)center.y - item.ItemFlags[0];

			if (entityTop > fieldBottom || entityBottom < fieldTop)
				return false;

			if (abs(localZ) > WALL_FIELD_THICKNESS)
				return false;

			return true;
		}
		else
		{
			if (abs(localX) > halfSpanX || abs(localZ) > halfSpanZ)
				return false;

			// Reject if intervening geometry (bridge, raised floor) exists between entity and field.
			auto pointColl = GetPointCollision(entity.Pose.Position, entity.RoomNumber);
			int floorAtEntity = pointColl.GetFloorHeight();

			if (floorAtEntity < (int)center.y - BRIDGE_DETECTION_TOLERANCE)
				return false;

			int fieldTop = (int)center.y - FLOOR_FIELD_THICKNESS;
			int fieldBottom = (int)center.y + FLOOR_FIELD_THICKNESS;

			return (entityTop < fieldBottom && entityBottom > fieldTop);
		}
	}

	static void KillEntity(ItemInfo& entity, bool isWallMode)
	{
		if (entity.IsLara())
		{
			auto& player = GetLaraInfo(entity);

			if (!isWallMode &&
				player.Context.Vehicle == NO_VALUE &&
				!player.Control.IsLow &&
				!entity.Animation.IsAirborne)
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

	static bool CheckCollisions(
		const ItemInfo& item,
		const Vector3& center,
		float halfSpanX,
		float halfSpanZ,
		float cosY,
		float sinY,
		bool isWallMode)
	{
		auto nearbyItems = std::vector<ItemInfo*>{};

		const auto& room = g_Level.Rooms[item.RoomNumber];
		int itemNum = room.itemNumber;

		while (itemNum != NO_VALUE)
		{
			auto& entity = g_Level.Items[itemNum];

			if (entity.HitPoints > 0 && (entity.IsLara() || entity.IsCreature()))
				nearbyItems.push_back(&entity);

			itemNum = entity.NextItem;
		}

		for (int neighborRoomNumber : room.NeighborRoomNumbers)
		{
			const auto& neighborRoom = g_Level.Rooms[neighborRoomNumber];
			itemNum = neighborRoom.itemNumber;

			while (itemNum != NO_VALUE)
			{
				auto& entity = g_Level.Items[itemNum];

				if (entity.HitPoints > 0 && (entity.IsLara() || entity.IsCreature()))
					nearbyItems.push_back(&entity);

				itemNum = entity.NextItem;
			}
		}

		bool isKilling = false;

		for (auto* entity : nearbyItems)
		{
			if (IsEntityInField(item, center, *entity, halfSpanX, halfSpanZ, cosY, sinY, isWallMode))
			{
				KillEntity(*entity, isWallMode);
				isKilling = true;
			}
		}

		return isKilling;
	}

	void ControlElectricField(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		if (!TriggerActive(&item))
			return;

		bool isWallMode = (item.TriggerFlags < 0);
		int totalBlocks = abs(item.TriggerFlags);

		if (totalBlocks == 0)
			totalBlocks = 1;

		float rotY = TO_RAD(-item.Pose.Orientation.y);
		float cosY = cos(rotY);
		float sinY = sin(rotY);

		float halfSpanX = 0.0f;
		float halfSpanZ = 0.0f;
		float localOffsetX = 0.0f;
		float localOffsetZ = 0.0f;

		if (isWallMode)
		{
			halfSpanX = totalBlocks * BLOCK(0.5f);
			halfSpanZ = BLOCK(0.5f);
			localOffsetX = (totalBlocks - 1) * BLOCK(0.5f);
			localOffsetZ = WALL_FORWARD_OFFSET;
		}
		else
		{
			halfSpanX = BLOCK(0.5f);
			halfSpanZ = totalBlocks * BLOCK(0.5f);
			localOffsetZ = -(totalBlocks - 1) * BLOCK(0.5f);
		}

		float worldOffsetX = localOffsetX * cosY - localOffsetZ * sinY;
		float worldOffsetZ = localOffsetX * sinY + localOffsetZ * cosY;

		auto effectiveCenter = Vector3(
			item.Pose.Position.x + worldOffsetX,
			item.Pose.Position.y,
			item.Pose.Position.z + worldOffsetZ);

		float wallHeight = isWallMode ? (float)item.ItemFlags[0] : 0.0f;

		SpawnSparks(effectiveCenter, item.RoomNumber, halfSpanX, halfSpanZ, cosY, sinY, isWallMode, wallHeight);
		SpawnFloorLight(effectiveCenter, item.Index, item.RoomNumber, halfSpanX, halfSpanZ, cosY, sinY, isWallMode, wallHeight);
		CheckCollisions(item, effectiveCenter, halfSpanX, halfSpanZ, cosY, sinY, isWallMode);
	}
}