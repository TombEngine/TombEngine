#include "framework.h"
#include "Objects/TR3/Trap/ElectricField.h"
#include "Game/Lara/lara_helpers.h"

#include "Game/collision/Point.h"
#include "Game/collision/floordata.h"
#include "Game/effects/Electricity.h"
#include "Game/effects/spark.h"
#include "Game/effects/item_fx.h"
#include "Game/items.h"
#include "Renderer/Renderer.h"
#include "Sound/sound.h"
#include "Game/control/trigger.h"

using namespace TEN::Collision::Point;
using namespace TEN::Effects::Spark;
using namespace TEN::Effects::Items;

namespace TEN::Entities::Traps
{
	constexpr auto ELECTRIC_FIELD_DAMAGE = INT_MAX;

	// Performance tuning constants
	constexpr auto EFFECT_UPDATE_INTERVAL = 2;   // Update visual effects every N frames
	constexpr auto SOUND_UPDATE_INTERVAL = 10;  // Update sound every N frames

	// Debug visualization
	constexpr auto ELECTRIC_FIELD_DEBUG = true;  // Set to false to disable debug visualization

	void InitializeElectricField(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];
		item.Status = ITEM_ACTIVE;

		// Get ceiling height ONLY within the current room (not rooms above)
		using namespace TEN::Collision::Floordata;
		auto& sector = GetFloor(item.RoomNumber, item.Pose.Position.x, item.Pose.Position.z);
		int ceilingHeight = sector.GetSurfaceHeight(item.Pose.Position.x, item.Pose.Position.z, false);  // false = ceiling

		// Store wall height in ItemFlags[0] (distance from item position UP to room ceiling)
		item.ItemFlags[0] = item.Pose.Position.y - ceilingHeight;
	}

	static void SpawnSparks(const Vector3& center, int roomNumber, float halfSpanX, float halfSpanZ, float cosY, float sinY, bool isWallMode, float wallHeight = 0.0f)
	{
		// Throttle: Only update every 2nd frame (50% reduction)
		if (GlobalCounter % EFFECT_UPDATE_INTERVAL != 0)
			return;

		// Reduced probability from 0.6 to 0.4
		if (!Random::TestProbability(0.4f))
			return;

		float localX = Random::GenerateFloat(-halfSpanX, halfSpanX);
		float localY = isWallMode ? Random::GenerateFloat(-wallHeight, 0.0f) : 0;  // Sparks from floor (0) to ceiling (-wallHeight)
		float localZ = isWallMode ? 0 : Random::GenerateFloat(-halfSpanZ, halfSpanZ);

		float sparkX = center.x + (localX * cosY - localZ * sinY);
		float sparkY = center.y + localY;
		float sparkZ = center.z + (localX * sinY + localZ * cosY);

		// Always spawn exactly 1 spark (not 1-2) for better performance
		auto& s = GetFreeSparkParticle();
		s = {};
		s.age = 0;
		s.life = Random::GenerateFloat(10, 18);
		s.friction = 1.0f;
		s.gravity = 2.0f;
		s.height = Random::GenerateFloat(128.0f, 384.0f);
		s.width = Random::GenerateFloat(16.0f, 32.0f);
		s.room = roomNumber;
		s.pos = Vector3(
			sparkX + Random::GenerateFloat(-16, 16),
			sparkY + Random::GenerateFloat(-16, 16),
			sparkZ + Random::GenerateFloat(-16, 16));

		if (isWallMode)
		{
			float	direction = Random::TestProbability(0.5f) ? 1.0f : -1.0f;
			Vector3 v = Vector3(-sinY * direction, Random::GenerateFloat(-0.2f, 0.2f), -cosY * direction);
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
			Pose soundPose = Pose(center);
			SoundEffect(SFX_TR5_ELECTRIC_LIGHT_CRACKLES, &soundPose);

			// Reduced light probability from 0.1 to 0.05 and radius from BLOCK(3) to BLOCK(2)
			if (Random::TestProbability(0.05f))
			{
				Vector3 lightPos = Vector3(sparkX, sparkY, sparkZ);
				Color	lightColor = Color(0.4f, 0.6f, 1.0f);
				SpawnDynamicPointLight(lightPos, lightColor, BLOCK(2), false, 0);
			}
		}
	}

	static void SpawnFloorLight(const Vector3& center, int itemIndex, int roomNumber, float halfSpanX, float halfSpanZ, float cosY, float sinY, bool isWallMode, float wallHeight = 0.0f)
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
			float localY = Random::GenerateFloat(-wallHeight * 0.8f, -wallHeight * 0.2f);  // Middle 60% of wall height
			float wallZ = 0;

			Vector3 lightPos = Vector3(
				center.x + (localX * cosY - wallZ * sinY),
				center.y + localY,
				center.z + (localX * sinY + wallZ * cosY));

			// Reduced flicker range to be gentler (was 0.8-1.2, now 0.9-1.1)
			float flicker = Random::GenerateFloat(0.9f, 1.1f);
			Color lightColor = Color(0.6f * flicker, 0.9f * flicker, 1.50f * flicker);

			// Smaller radius to keep light more contained within field bounds
			float lightRadius = std::min(BLOCK(3.0f), halfSpanX * 1.5f);
			SpawnDynamicPointLight(lightPos, lightColor, lightRadius, false, itemIndex);  // No shadows
		}
		else
		{
			// Random position along floor depth - FULLY contained within field bounds
			float localZ = Random::GenerateFloat(-halfSpanZ, halfSpanZ);
			float localX = Random::GenerateFloat(-halfSpanX, halfSpanX);
			float yOffset = -CLICK(0.25f);

			Vector3 lightPos = Vector3(
				center.x + (localX * cosY - localZ * sinY),
				center.y + (yOffset - CLICK(0.75f)),
				center.z + (localX * sinY + localZ * cosY));

			// Reduced flicker range
			float flicker = Random::GenerateFloat(0.9f, 1.1f);
			Color lightColor = Color(0.6f * flicker, 0.9f * flicker, 1.50f * flicker);

			// Smaller radius to keep light more contained
			float lightRadius = std::min(BLOCK(3.0f), halfSpanZ * 1.5f);
			SpawnDynamicPointLight(lightPos, lightColor, lightRadius, false, itemIndex);  // No shadows
		}
	}

	static bool IsEntityInField(const ItemInfo& item, const Vector3& center, const ItemInfo& entity, float halfSpanX, float halfSpanZ, float cosY, float sinY, bool isWallMode)
	{
		float worldDeltaX = entity.Pose.Position.x - center.x;
		float worldDeltaZ = entity.Pose.Position.z - center.z;

		float localX = worldDeltaX * cosY + worldDeltaZ * sinY;
		float localZ = -worldDeltaX * sinY + worldDeltaZ * cosY;

		auto bounds = GameBoundingBox(&entity);
		int	 entityTop = entity.Pose.Position.y + bounds.Y1;
		int	 entityBottom = entity.Pose.Position.y + bounds.Y2;

		if (isWallMode)
		{
			if (abs(localX) > halfSpanX)
				return false;

			// Wall starts at floor (center.y) and extends UP to ceiling
			int fieldBottom = center.y;  // Floor level
			int fieldTop = center.y - item.ItemFlags[0];  // Ceiling level (negative Y is up)

			if (entityTop > fieldBottom || entityBottom < fieldTop)
				return false;

			int fieldThickness = CLICK(0.5f);
			if (abs(localZ) > fieldThickness)
				return false;

			return true;
		}
		else
		{
			if (abs(localX) > halfSpanX || abs(localZ) > halfSpanZ)
				return false;

			int fieldThickness = CLICK(0.25f);
			int fieldTop = center.y - fieldThickness;
			int fieldBottom = center.y + fieldThickness;

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

	static bool CheckCollisions(const ItemInfo& item, const Vector3& center, float halfSpanX, float halfSpanZ, float cosY, float sinY, bool isWallMode)
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
		bool isKilling = false;
		for (auto* entity : nearbyItems)
		{
			if (IsEntityInField(item, center, *entity, halfSpanX, halfSpanZ, cosY, sinY, isWallMode))
			{
				KillEntity(*entity);
				isKilling = true;
			}
		}

		return isKilling;
	}

	static void DrawDebugVisualization(const ItemInfo& item, const Vector3& center, float halfSpanX, float halfSpanZ, float cosY, float sinY, bool isWallMode, bool isKilling)
	{
		if (!ELECTRIC_FIELD_DEBUG)
			return;

		using namespace TEN::Renderer;

		// Color: Green when idle, Red when killing
		Vector4 color = isKilling ? Vector4(1.0f, 0.0f, 0.0f, 1.0f) : Vector4(0.0f, 1.0f, 0.0f, 1.0f);

		if (isWallMode)
		{
			// Wall mode: Draw vertical plane showing wall bounds

			// Calculate corners of wall plane in local space
			float leftX = -halfSpanX;
			float rightX = halfSpanX;
			float wallZ = 0;

			// Wall starts at floor (center.y) and extends UP to ceiling
			int bottomY = center.y;  // Floor level
			int topY = center.y - item.ItemFlags[0];  // Ceiling level (negative Y is up)

			// Transform corners to world space
			Vector3 bottomLeft = Vector3(
				center.x + (leftX * cosY - wallZ * sinY),
				bottomY,
				center.z + (leftX * sinY + wallZ * cosY));

			Vector3 bottomRight = Vector3(
				center.x + (rightX * cosY - wallZ * sinY),
				bottomY,
				center.z + (rightX * sinY + wallZ * cosY));

			Vector3 topLeft = Vector3(
				center.x + (leftX * cosY - wallZ * sinY),
				topY,
				center.z + (leftX * sinY + wallZ * cosY));

			Vector3 topRight = Vector3(
				center.x + (rightX * cosY - wallZ * sinY),
				topY,
				center.z + (rightX * sinY + wallZ * cosY));

			// Draw wall rectangle
			g_Renderer.AddDebugLine(bottomLeft, bottomRight, color, RendererDebugPage::CollisionStats);
			g_Renderer.AddDebugLine(bottomRight, topRight, color, RendererDebugPage::CollisionStats);
			g_Renderer.AddDebugLine(topRight, topLeft, color, RendererDebugPage::CollisionStats);
			g_Renderer.AddDebugLine(topLeft, bottomLeft, color, RendererDebugPage::CollisionStats);

			// Draw diagonal cross for visibility
			g_Renderer.AddDebugLine(bottomLeft, topRight, color, RendererDebugPage::CollisionStats);
			g_Renderer.AddDebugLine(bottomRight, topLeft, color, RendererDebugPage::CollisionStats);

			// Draw yellow line from origin to center
			Vector3 originPos = Vector3(item.Pose.Position.x, center.y, item.Pose.Position.z);
			Vector3 centerPos = Vector3(center.x, center.y, center.z);
			g_Renderer.AddDebugLine(originPos, centerPos, Vector4(1.0f, 1.0f, 0.0f, 1.0f), RendererDebugPage::CollisionStats);

			// Draw origin marker (small vertical line)
			Vector3 originBottom = Vector3(item.Pose.Position.x, bottomY, item.Pose.Position.z);
			Vector3 originTop = Vector3(item.Pose.Position.x, topY, item.Pose.Position.z);
			g_Renderer.AddDebugLine(originBottom, originTop, Vector4(1.0f, 0.5f, 0.0f, 1.0f), RendererDebugPage::CollisionStats);
		}
		else
		{
			// Floor mode: Draw horizontal rectangle on floor showing bounds

			// Calculate corners in local space
			float leftX = -halfSpanX;
			float rightX = halfSpanX;
			float nearZ = -halfSpanZ;
			float farZ = halfSpanZ;

			float floorY = center.y;

			// Transform corners to world space
			Vector3 nearLeft = Vector3(
				center.x + (leftX * cosY - nearZ * sinY),
				floorY,
				center.z + (leftX * sinY + nearZ * cosY));

			Vector3 nearRight = Vector3(
				center.x + (rightX * cosY - nearZ * sinY),
				floorY,
				center.z + (rightX * sinY + nearZ * cosY));

			Vector3 farLeft = Vector3(
				center.x + (leftX * cosY - farZ * sinY),
				floorY,
				center.z + (leftX * sinY + farZ * cosY));

			Vector3 farRight = Vector3(
				center.x + (rightX * cosY - farZ * sinY),
				floorY,
				center.z + (rightX * sinY + farZ * cosY));

			// Draw floor rectangle
			g_Renderer.AddDebugLine(nearLeft, nearRight, color, RendererDebugPage::CollisionStats);
			g_Renderer.AddDebugLine(nearRight, farRight, color, RendererDebugPage::CollisionStats);
			g_Renderer.AddDebugLine(farRight, farLeft, color, RendererDebugPage::CollisionStats);
			g_Renderer.AddDebugLine(farLeft, nearLeft, color, RendererDebugPage::CollisionStats);

			// Draw diagonal cross for visibility
			g_Renderer.AddDebugLine(nearLeft, farRight, color, RendererDebugPage::CollisionStats);
			g_Renderer.AddDebugLine(nearRight, farLeft, color, RendererDebugPage::CollisionStats);

			// Draw yellow line from origin to center
			Vector3 originPos = Vector3(item.Pose.Position.x, floorY, item.Pose.Position.z);
			Vector3 centerPos = Vector3(center.x, floorY, center.z);
			g_Renderer.AddDebugLine(originPos, centerPos, Vector4(1.0f, 1.0f, 0.0f, 1.0f), RendererDebugPage::CollisionStats);

			// Draw origin marker (vertical line at origin)
			Vector3 originDown = Vector3(item.Pose.Position.x, floorY, item.Pose.Position.z);
			Vector3 originUp = Vector3(item.Pose.Position.x, floorY - CLICK(2), item.Pose.Position.z);
			g_Renderer.AddDebugLine(originDown, originUp, Vector4(1.0f, 0.5f, 0.0f, 1.0f), RendererDebugPage::CollisionStats);
		}
	}

	void ControlElectricField(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		if (!TriggerActive(&item))
			return;

		bool isWallMode = (item.TriggerFlags < 0);

		// OCB directly represents total width/depth in blocks
		int totalBlocks = abs(item.TriggerFlags);
		if (totalBlocks == 0)
			totalBlocks = 1;  // Default to 1 block if OCB is 0

		float halfSpanX, halfSpanZ;
		float localOffsetX = 0.0f;
		float localOffsetZ = 0.0f;

		if (isWallMode)
		{
			// Wall mode: Origin at LEFT edge, wall extends RIGHT
			// Width is variable (OCB), depth is always 1 block
			halfSpanX = totalBlocks * BLOCK(0.5f);
			halfSpanZ = BLOCK(0.5f);

			// Offset center to the right: origin is at left edge, so shift by (width - 1) / 2
			localOffsetX = (totalBlocks - 1) * BLOCK(0.5f);  // For OCB=1: 0, OCB=2: BLOCK(0.5), OCB=3: BLOCK(1)
			localOffsetZ = +CLICK(1.75f);  // Forward offset of 448 units so wall doesn't overlap geometry
		}
		else
		{
			// Floor mode: Origin at BACK edge, field extends FORWARD (edge-aligned)
			// Width is always 1 block, depth is variable (OCB)
			halfSpanX = BLOCK(0.5f);
			halfSpanZ = totalBlocks * BLOCK(0.5f);

			// Offset center forward: origin is at back edge, so shift by (depth - 1) / 2
			localOffsetX = 0.0f;
			localOffsetZ = -(totalBlocks - 1) * BLOCK(0.5f);  // For OCB=1: 0, OCB=2: -BLOCK(0.5), OCB=3: -BLOCK(1)
		}

		float rotY = TO_RAD(-item.Pose.Orientation.y);  // Negate to match editor rotation direction
		float cosY = cos(rotY);
		float sinY = sin(rotY);

		// Transform local offset to world space
		float worldOffsetX = localOffsetX * cosY - localOffsetZ * sinY;
		float worldOffsetZ = localOffsetX * sinY + localOffsetZ * cosY;

		// Calculate effective center position (origin + offset)
		Vector3 effectiveCenter = Vector3(
			item.Pose.Position.x + worldOffsetX,
			item.Pose.Position.y,
			item.Pose.Position.z + worldOffsetZ);

		// Get wall height for effects (only used in wall mode)
		float wallHeight = isWallMode ? item.ItemFlags[0] : 0.0f;

		SpawnSparks(effectiveCenter, item.RoomNumber, halfSpanX, halfSpanZ, cosY, sinY, isWallMode, wallHeight);
		SpawnFloorLight(effectiveCenter, item.Index, item.RoomNumber, halfSpanX, halfSpanZ, cosY, sinY, isWallMode, wallHeight);

		bool isKilling = CheckCollisions(item, effectiveCenter, halfSpanX, halfSpanZ, cosY, sinY, isWallMode);

		// Debug visualization - Green when idle, Red when killing
		DrawDebugVisualization(item, effectiveCenter, halfSpanX, halfSpanZ, cosY, sinY, isWallMode, isKilling);
	}
}