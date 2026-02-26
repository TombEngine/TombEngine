#include "framework.h"
#include "Objects/TR3/Trap/ElectricField.h"

#include "Game/animation/Animation.h"
#include "Game/collision/floordata.h"
#include "Game/collision/Point.h"
#include "Game/control/trigger.h"
#include "Game/effects/Electricity.h"
#include "Game/effects/item_fx.h"
#include "Game/effects/smoke.h"
#include "Game/effects/spark.h"
#include "Game/effects/tomb4fx.h"
#include "Game/items.h"
#include "Game/Lara/lara_helpers.h"
#include "Renderer/Renderer.h"
#include "Sound/sound.h"

using namespace TEN::Collision::Floordata;
using namespace TEN::Collision::Point;
using namespace TEN::Effects::Items;
using namespace TEN::Effects::Smoke;
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
	constexpr auto LIGHTNING_LIGHT_RADIUS = BLOCK(3);
	constexpr auto FLOOR_LIGHT_INTERVAL = 5;
	constexpr auto FLOOR_LIGHT_PROBABILITY = 0.2f;
	constexpr auto WALL_FIELD_THICKNESS = CLICK(0.5f);
	constexpr auto FLOOR_FIELD_THICKNESS = CLICK(0.25f);
	constexpr auto BRIDGE_DETECTION_TOLERANCE = CLICK(0.5f);
	constexpr auto WALL_FORWARD_OFFSET = CLICK(1.75f);

	// Lightning burst timing (in frames at 30 FPS).
	constexpr auto LIGHTNING_COOLDOWN_MIN = 5 * FPS;
	constexpr auto LIGHTNING_COOLDOWN_MAX = 10 * FPS;
	constexpr auto LIGHTNING_DURATION_MIN = FPS / 2;
	constexpr auto LIGHTNING_DURATION_MAX = FPS;

	constexpr auto LIGHTNING_FLAGS = (int)ElectricityFlags::Spline |
		(int)ElectricityFlags::ThinIn |
		(int)ElectricityFlags::SparkEnd;

	// ItemFlags layout:
	// [0] = Wall height (floor to ceiling distance).
	// [1] = Lightning cooldown counter (frames until next burst).
	// [2] = Lightning active duration counter (frames remaining in current burst).

	void InitializeElectricField(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];
		item.Status = ITEM_ACTIVE;

		auto& sector = GetFloor(item.RoomNumber, item.Pose.Position.x, item.Pose.Position.z);
		int ceilingHeight = sector.GetSurfaceHeight(item.Pose.Position.x, item.Pose.Position.z, false);

		item.ItemFlags[0] = item.Pose.Position.y - ceilingHeight;
		item.ItemFlags[1] = Random::GenerateInt(LIGHTNING_COOLDOWN_MIN, LIGHTNING_COOLDOWN_MAX);
		item.ItemFlags[2] = 0;
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

	static void SpawnArcSmoke(const Vector3& pos, const Vector3& surfaceNormal, int roomNumber)
	{
		auto offsetPos = pos + surfaceNormal * 32.0f;
		SpawnElectricArcSmoke(offsetPos, roomNumber);
		SpawnDynamicPointLight(pos, Color(0.6f, 0.8f, 1.0f), BLOCK(10), false, 0);

		// Burst of sparks at contact point, spraying away from surface.
		int sparkCount = Random::GenerateInt(4, 8);

		for (int i = 0; i < sparkCount; i++)
		{
			auto& spark = GetFreeSparkParticle();

			spark = {};
			spark.active = true;
			spark.age = 0;
			spark.life = Random::GenerateFloat(24, 48);
			spark.friction = 0.98f;
			spark.gravity = 3.0f;
			spark.height = Random::GenerateFloat(128.0f, 384.0f);
			spark.width = Random::GenerateFloat(16.0f, 32.0f);
			spark.room = roomNumber;
			spark.pos = pos + Vector3(Random::GenerateFloat(-8, 8), Random::GenerateFloat(-8, 8), Random::GenerateFloat(-8, 8));

			// Cone of sparks in surface normal direction with spread.
			float spread = Random::GenerateFloat(-0.4f, 0.4f);
			auto dir = surfaceNormal + Vector3(Random::GenerateFloat(-spread, spread), Random::GenerateFloat(-spread, spread), Random::GenerateFloat(-spread, spread));
			dir.Normalize(dir);

			spark.velocity = dir * Random::GenerateFloat(16, 48);
			spark.sourceColor = Vector4(0.6f, 0.8f, 1.0f, 1.0f);
			spark.destinationColor = Vector4(0.3f, 0.4f, 0.8f, 0.0f);
		}
	}

	static void SpawnLightning(
		ItemInfo& item,
		const Vector3& center,
		float halfSpanX,
		float halfSpanZ,
		float cosY,
		float sinY,
		bool isWallMode,
		float wallHeight)
	{
		// Manage burst timing via ItemFlags[1] (cooldown) and ItemFlags[2] (active duration).
		if (item.ItemFlags[2] > 0)
		{
			item.ItemFlags[2]--;
			return;
		}

		item.ItemFlags[1]--;

		if (item.ItemFlags[1] > 0)
			return;

		item.ItemFlags[2] = Random::GenerateInt(LIGHTNING_DURATION_MIN, LIGHTNING_DURATION_MAX);
		item.ItemFlags[1] = Random::GenerateInt(LIGHTNING_COOLDOWN_MIN, LIGHTNING_COOLDOWN_MAX);

		auto soundPose = Pose(center);
		float pitch = Random::GenerateFloat(0.8f, 1.2f);
		SoundEffect(SFX_TR4_LARA_ELECTRIC_CRACKLES, &soundPose, SoundEnvironment::Always, pitch);

		float r = Random::GenerateInt(32, 128) / 255.0f;
		float g = Random::GenerateInt(128, 192) / 255.0f;
		float b = Random::GenerateInt(192, 255) / 255.0f;

		if (isWallMode)
		{
			float leftX = -halfSpanX;
			float rightX = halfSpanX;
			float midX = Random::GenerateFloat(leftX * 0.6f, rightX * 0.6f);
			float randomY = Random::GenerateFloat(-wallHeight, 0);

			// Arc protrudes outward from the wall surface.
			float arcDepth = Random::GenerateFloat(CLICK(1), CLICK(3));
			float arcDir = Random::TestProbability(0.5f) ? 1.0f : -1.0f;

			auto origin = Vector3(
				center.x + (leftX * cosY),
				center.y + randomY,
				center.z + (leftX * sinY));

			auto midpoint = Vector3(
				center.x + (midX * cosY) + (-sinY * arcDepth * arcDir),
				center.y + randomY,
				center.z + (midX * sinY) + (cosY * arcDepth * arcDir));

			auto target = Vector3(
				center.x + (rightX * cosY),
				center.y + randomY,
				center.z + (rightX * sinY));

			int segments = Random::GenerateInt(8, 16);
			int life = Random::GenerateInt(16, 32);
			float width = Random::GenerateFloat(8.0f, 16.0f);
			int splitCount = Random::GenerateInt(1, 4);

			SpawnElectricity(origin, midpoint, segments, r * 255, g * 255, b * 255, life, LIGHTNING_FLAGS, width, splitCount);
			SpawnElectricity(midpoint, target, segments, r * 255, g * 255, b * 255, life, LIGHTNING_FLAGS, width, splitCount);

			auto wallNormal = Vector3(-sinY * arcDir, 0, cosY * arcDir);
			SpawnArcSmoke(origin, wallNormal, item.RoomNumber);
			SpawnArcSmoke(target, wallNormal, item.RoomNumber);

			SpawnDynamicPointLight(midpoint, Color(0.4f, 0.6f, 1.0f), LIGHTNING_LIGHT_RADIUS, false, 0);
		}
		else
		{
			float leftZ = -halfSpanZ;
			float rightZ = halfSpanZ;
			float midZ = Random::GenerateFloat(leftZ * 0.6f, rightZ * 0.6f);
			float randomX = Random::GenerateFloat(-halfSpanX, halfSpanX);
			float yOffset = -FLOOR_FIELD_THICKNESS;

			// Arc rises upward from the floor surface.
			float arcHeight = Random::GenerateFloat(CLICK(1), CLICK(3));

			auto origin = Vector3(
				center.x + (randomX * cosY - leftZ * sinY),
				center.y + yOffset,
				center.z + (randomX * sinY + leftZ * cosY));

			auto midpoint = Vector3(
				center.x + (randomX * cosY - midZ * sinY),
				center.y + yOffset - arcHeight,
				center.z + (randomX * sinY + midZ * cosY));

			auto target = Vector3(
				center.x + (randomX * cosY - rightZ * sinY),
				center.y + yOffset,
				center.z + (randomX * sinY + rightZ * cosY));

			int segments = Random::GenerateInt(8, 16);
			int life = Random::GenerateInt(16, 32);
			float width = Random::GenerateFloat(8.0f, 16.0f);
			int splitCount = Random::GenerateInt(1, 2);

			SpawnElectricity(origin, midpoint, segments, r * 255, g * 255, b * 255, life, LIGHTNING_FLAGS, width, splitCount);
			SpawnElectricity(midpoint, target, segments, r * 255, g * 255, b * 255, life, LIGHTNING_FLAGS, width, splitCount);

			auto upNormal = Vector3(0, -1, 0);
			SpawnArcSmoke(origin, upNormal, item.RoomNumber);
			SpawnArcSmoke(target, upNormal, item.RoomNumber);

			SpawnDynamicPointLight(midpoint, Color(0.4f, 0.6f, 1.0f), LIGHTNING_LIGHT_RADIUS, false, 0);
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

	static void SpawnBodyElectricity(ItemInfo& entity)
	{
		const auto& object = Objects[entity.ObjectNumber];
		int meshCount = object.nmeshes;

		if (meshCount < 2)
			return;

		int arcCount = Random::GenerateInt(2, 4);

		for (int i = 0; i < arcCount; i++)
		{
			int jointA = Random::GenerateInt(0, meshCount - 1);
			int jointB = Random::GenerateInt(0, meshCount - 1);

			if (jointA == jointB)
				continue;

			auto posA = GetJointPosition(entity, jointA).ToVector3();
			auto posB = GetJointPosition(entity, jointB).ToVector3();

			float r = Random::GenerateInt(32, 128) / 255.0f;
			float g = Random::GenerateInt(128, 192) / 255.0f;
			float b = Random::GenerateInt(192, 255) / 255.0f;

			SpawnElectricity(
				posA, posB,
				Random::GenerateInt(4, 8),
				r * 255, g * 255, b * 255,
				Random::GenerateInt(8, 16),
				LIGHTNING_FLAGS,
				Random::GenerateFloat(4.0f, 8.0f),
				Random::GenerateInt(1, 2));
		}

		// Single light at a random joint to illuminate the effect.
		int lightJoint = Random::GenerateInt(0, meshCount - 1);
		auto lightPos = GetJointPosition(entity, lightJoint).ToVector3();
		SpawnDynamicPointLight(lightPos, Color(0.4f, 0.6f, 1.0f), BLOCK(2), false, 0);
	}

	static void KillEntity(ItemInfo& entity, bool isWallMode)
	{
		SpawnBodyElectricity(entity);

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

			if (entity.IsLara() || entity.IsCreature())
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

				if (entity.IsLara() || entity.IsCreature())
					nearbyItems.push_back(&entity);

				itemNum = entity.NextItem;
			}
		}

		bool isKilling = false;

		for (auto* entity : nearbyItems)
		{
			bool inField = IsEntityInField(item, center, *entity, halfSpanX, halfSpanZ, cosY, sinY, isWallMode);

			// Continue body electricity on entities dying inside the field.
			if (entity->HitPoints <= 0 && inField)
			{
				SpawnBodyElectricity(*entity);
				SoundEffect(SFX_TR4_ELECTRIC_ARCING_LOOP, &entity->Pose);
				continue;
			}

			if (entity->HitPoints <= 0)
				continue;

			if (inField)
			{
				KillEntity(*entity, isWallMode);
				SoundEffect(SFX_TR4_ELECTRIC_ARCING_LOOP, &entity->Pose);
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
		SpawnLightning(item, effectiveCenter, halfSpanX, halfSpanZ, cosY, sinY, isWallMode, wallHeight);
		SpawnFloorLight(effectiveCenter, item.Index, item.RoomNumber, halfSpanX, halfSpanZ, cosY, sinY, isWallMode, wallHeight);
		bool isKilling = CheckCollisions(item, effectiveCenter, halfSpanX, halfSpanZ, cosY, sinY, isWallMode);

		if (!isKilling)
			item.ItemFlags[3] = 0;

		// Floor mode: trigger burst effects on first contact only.
		if (isKilling && !isWallMode && item.ItemFlags[3] == 0)
		{
			item.ItemFlags[3] = 1;

			// Find the entity that triggered the kill for positioning.
			auto collisionPos = effectiveCenter;

			const auto& room = g_Level.Rooms[item.RoomNumber];
			int itemNum = room.itemNumber;

			while (itemNum != NO_VALUE)
			{
				auto& entity = g_Level.Items[itemNum];

				if ((entity.IsLara() || entity.IsCreature()) &&
					IsEntityInField(item, effectiveCenter, entity, halfSpanX, halfSpanZ, cosY, sinY, isWallMode))
				{
					collisionPos = Vector3(
						(float)entity.Pose.Position.x,
						effectiveCenter.y,
						(float)entity.Pose.Position.z);
					break;
				}

				itemNum = entity.NextItem;
			}
			float r = Random::GenerateInt(32, 128) / 255.0f;
			float g = Random::GenerateInt(128, 192) / 255.0f;
			float b = Random::GenerateInt(192, 255) / 255.0f;

			float randomX = collisionPos.x - effectiveCenter.x;
			float leftZ = -halfSpanZ;
			float rightZ = halfSpanZ;
			float midZ = Random::GenerateFloat(leftZ * 0.6f, rightZ * 0.6f);
			float yOffset = -FLOOR_FIELD_THICKNESS;

			float arcHeight = Random::GenerateFloat(CLICK(1), CLICK(3));

			auto origin = Vector3(
				collisionPos.x + (leftZ * -sinY),
				effectiveCenter.y + yOffset,
				collisionPos.z + (leftZ * cosY));

			auto midpoint = Vector3(
				collisionPos.x + (midZ * -sinY),
				effectiveCenter.y + yOffset - arcHeight,
				collisionPos.z + (midZ * cosY));

			auto target = Vector3(
				collisionPos.x + (rightZ * -sinY),
				effectiveCenter.y + yOffset,
				collisionPos.z + (rightZ * cosY));

			int segments = Random::GenerateInt(8, 16);
			int life = Random::GenerateInt(16, 32);
			float width = Random::GenerateFloat(8.0f, 16.0f);
			int splitCount = Random::GenerateInt(1, 2);

			SpawnElectricity(origin, midpoint, segments, r * 255, g * 255, b * 255, life, LIGHTNING_FLAGS, width, splitCount);
			SpawnElectricity(midpoint, target, segments, r * 255, g * 255, b * 255, life, LIGHTNING_FLAGS, width, splitCount);

			auto upNormal = Vector3(0, -1, 0);
			SpawnArcSmoke(origin, upNormal, item.RoomNumber);
			SpawnArcSmoke(target, upNormal, item.RoomNumber);

			SpawnDynamicPointLight(midpoint, Color(0.4f, 0.6f, 1.0f), LIGHTNING_LIGHT_RADIUS, false, 0);

			auto soundPose = Pose(effectiveCenter);
			float pitch = Random::GenerateFloat(0.8f, 1.2f);
			SoundEffect(SFX_TR4_LARA_ELECTRIC_CRACKLES, &soundPose, SoundEnvironment::Always, pitch);

			// Blue shockwave at collision point.
			auto shockPose = Pose(Vector3i(collisionPos.x, collisionPos.y - 10, collisionPos.z));
			TriggerShockwave(&shockPose, 0, BLOCK(0.5f), 64, 121, 213, 242, 30, EulerAngles(0, 0, 0), 0, true, false, false, (int)ShockwaveStyle::Normal);
		}
	}
}