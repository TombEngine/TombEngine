#include "framework.h"
#include "Game/effects/Footprint.h"

#include "Game/Animation/Animation.h"
#include "Game/collision/collide_room.h"
#include "Game/collision/floordata.h"
#include "Game/collision/Point.h"
#include "Game/control/control.h"
#include "Game/effects/effects.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Objects/Utils/object_helper.h"
#include "Sound/sound.h"
#include "Specific/clock.h"
#include "Specific/level.h"

using namespace TEN::Animation;
using namespace TEN::Collision::Floordata;
using namespace TEN::Collision::Point;
using namespace TEN::Math;

namespace TEN::Effects::Footprint
{
	const auto FootprintMaterials = std::vector<MaterialType>
	{
		MaterialType::Mud,
		MaterialType::Snow,
		MaterialType::Sand,
		MaterialType::Gravel,
		MaterialType::Custom1,
		MaterialType::Custom2,
		MaterialType::Custom3,
		MaterialType::Custom4
	};

	struct FootprintPositionData
	{
		bool	CanSpawn = false;
		Vector3 Position = Vector3::Zero;
	};

	std::vector<Footprint> Footprints = {};

	static SOUND_EFFECTS GetFootprintSfx(MaterialType material)
	{
		static const auto SOUND_MAP = std::unordered_map<MaterialType, SOUND_EFFECTS>
		{
			{ MaterialType::Mud, SOUND_EFFECTS::SFX_TR4_LARA_FOOTSTEPS_MUD },
			{ MaterialType::Snow, SOUND_EFFECTS::SFX_TR4_LARA_FOOTSTEPS_SNOW },
			{ MaterialType::Sand, SOUND_EFFECTS::SFX_TR4_LARA_FOOTSTEPS_SAND },
			{ MaterialType::Gravel, SOUND_EFFECTS::SFX_TR4_LARA_FOOTSTEPS_GRAVEL },
			{ MaterialType::Ice, SOUND_EFFECTS::SFX_TR4_LARA_FOOTSTEPS_ICE },
			{ MaterialType::Water, SOUND_EFFECTS::SFX_TR4_LARA_WET_FEET },
			{ MaterialType::Stone, SOUND_EFFECTS::SFX_TR4_LARA_FOOTSTEPS },
			{ MaterialType::Wood, SOUND_EFFECTS::SFX_TR4_LARA_FOOTSTEPS_WOOD },
			{ MaterialType::Metal, SOUND_EFFECTS::SFX_TR4_LARA_FOOTSTEPS_METAL },
			{ MaterialType::Marble, SOUND_EFFECTS::SFX_TR4_LARA_FOOTSTEPS_MARBLE },
			{ MaterialType::Grass, SOUND_EFFECTS::SFX_TR4_LARA_FOOTSTEPS_GRASS },
			{ MaterialType::Concrete, SOUND_EFFECTS::SFX_TR4_LARA_FOOTSTEPS },
			{ MaterialType::OldWood, SOUND_EFFECTS::SFX_TR4_LARA_FOOTSTEPS_WOOD },
			{ MaterialType::OldMetal, SOUND_EFFECTS::SFX_TR4_LARA_FOOTSTEPS_METAL },
			{ MaterialType::Custom1, SOUND_EFFECTS::SFX_TEN_CUSTOM_FOOTSTEPS_1 },
			{ MaterialType::Custom2, SOUND_EFFECTS::SFX_TEN_CUSTOM_FOOTSTEPS_2 },
			{ MaterialType::Custom3, SOUND_EFFECTS::SFX_TEN_CUSTOM_FOOTSTEPS_3 },
			{ MaterialType::Custom4, SOUND_EFFECTS::SFX_TEN_CUSTOM_FOOTSTEPS_4 },
			{ MaterialType::Custom5, SOUND_EFFECTS::SFX_TEN_CUSTOM_FOOTSTEPS_5 },
			{ MaterialType::Custom6, SOUND_EFFECTS::SFX_TEN_CUSTOM_FOOTSTEPS_6 },
			{ MaterialType::Custom7, SOUND_EFFECTS::SFX_TEN_CUSTOM_FOOTSTEPS_7 },
			{ MaterialType::Custom8, SOUND_EFFECTS::SFX_TEN_CUSTOM_FOOTSTEPS_8 }
		};

		auto it = SOUND_MAP.find(material);
		return ((it != SOUND_MAP.end()) ? it->second : SOUND_EFFECTS::SFX_TR4_LARA_FOOTSTEPS);
	}

	static std::array<Vector3, Footprint::VERTEX_COUNT> GetFootprintVertexPoints(const ItemInfo& item, const Vector3& pos, const Vector3& normal)
	{
		constexpr auto SCALE   = BLOCK(1 / 16.0f);
		constexpr auto POINT_0 = Vector3( SCALE, 0.0f,  SCALE);
		constexpr auto POINT_1 = Vector3(-SCALE, 0.0f,  SCALE);
		constexpr auto POINT_2 = Vector3(-SCALE, 0.0f, -SCALE);
		constexpr auto POINT_3 = Vector3( SCALE, 0.0f, -SCALE);

		// Determine rotation matrix.
		auto rotMatrix = Geometry::GetRelOrientToNormal(item.Pose.Orientation.y, normal).ToRotationMatrix();

		return std::array<Vector3, Footprint::VERTEX_COUNT>
		{
			pos + Vector3::Transform(POINT_0, rotMatrix),
			pos + Vector3::Transform(POINT_1, rotMatrix),
			pos + Vector3::Transform(POINT_2, rotMatrix),
			pos + Vector3::Transform(POINT_3, rotMatrix)
		};
	}

	static FootprintPositionData GetFootprintPositionData(const ItemInfo& item, int jointIndex)
	{
		constexpr auto SURFACE_OFFSET  = 4;
		constexpr auto ABS_FLOOR_BOUND = CLICK(0.25f);
		constexpr auto HEIGHT_OFFSET   = CLICK(0.25f);
		constexpr auto FOOT_OFFSET	   = Vector3i(0, HEIGHT_OFFSET, 0);

		auto footPos = GetJointPosition(item, jointIndex, FOOT_OFFSET);
		int floorHeight = GetPointCollision(footPos, item.RoomNumber, -Vector3::UnitY, CLICK(1)).GetFloorHeight();

		bool canSpawn = (abs(footPos.y - floorHeight) < ABS_FLOOR_BOUND);
		auto pos = Vector3(footPos.x, floorHeight - SURFACE_OFFSET, footPos.z);
		return FootprintPositionData{ canSpawn, pos };
	}

	static bool TestFootprintFloor(const ItemInfo& item, const Vector3& pos, const std::array<Vector3, Footprint::VERTEX_COUNT>& vertexPoints)
	{
		constexpr auto ABS_FLOOR_BOUND = CLICK(0.5f);

		// Get point collision at every vertex point.
		auto pointColl0 = GetPointCollision(Vector3i(vertexPoints[0].x, pos.y - CLICK(1), vertexPoints[0].z), item.RoomNumber);
		auto pointColl1 = GetPointCollision(Vector3i(vertexPoints[1].x, pos.y - CLICK(1), vertexPoints[1].z), item.RoomNumber);
		auto pointColl2 = GetPointCollision(Vector3i(vertexPoints[2].x, pos.y - CLICK(1), vertexPoints[2].z), item.RoomNumber);
		auto pointColl3 = GetPointCollision(Vector3i(vertexPoints[3].x, pos.y - CLICK(1), vertexPoints[3].z), item.RoomNumber);

		// Don't spawn footprint if floor heights at vertex points are outside upper/lower floor height bound.
		if ((abs(pointColl0.GetFloorHeight() - pointColl1.GetFloorHeight()) > ABS_FLOOR_BOUND) ||
			(abs(pointColl1.GetFloorHeight() - pointColl2.GetFloorHeight()) > ABS_FLOOR_BOUND) ||
			(abs(pointColl2.GetFloorHeight() - pointColl3.GetFloorHeight()) > ABS_FLOOR_BOUND) ||
			(abs(pointColl3.GetFloorHeight() - pointColl0.GetFloorHeight()) > ABS_FLOOR_BOUND))
		{
			return false;
		}

		return true;
	}

	void SpawnFootprint(bool isRight, const std::array<Vector3, Footprint::VERTEX_COUNT>& vertexPoints)
	{
		if (!CheckIfSlotExists(ID_MISC_SPRITES, "Footprint rendering"))
			return;

		constexpr auto LIFE_MAX			 = 20.0f;
		constexpr auto LIFE_START_FADING = 10.0f;
		constexpr auto OPACITY_MAX		 = 0.5f;

		auto& footprint = GetNewEffect(Footprints, Footprint::COUNT_MAX);

		footprint.SpriteIndex = Objects[ID_MISC_SPRITES].meshIndex + (1 + (int)isRight);
		footprint.IsRight = isRight;
		footprint.VertexPoints = vertexPoints;
		footprint.Life = std::round(LIFE_MAX * FPS);
		footprint.LifeStartFading = std::round(LIFE_START_FADING * FPS);
		footprint.Opacity =
		footprint.OpacityStart = OPACITY_MAX;
	}

	void SpawnFootprint(const ItemInfo& item, bool isRight)
	{
		if (!item.IsLara())
			return;

		// Don't spawn footprint if foot isn't on floor.
		int jointIndex = isRight ? LM_RFOOT : LM_LFOOT;
		auto posData = GetFootprintPositionData(item, jointIndex);
		if (!posData.CanSpawn)
			return;

		// Slightly randomize 2D position.
		posData.Position += Vector3(Random::GenerateFloat(-5.0f, 5.0f), 0.0f, Random::GenerateFloat(-5.0f, 5.0f));

		auto pointColl = GetPointCollision(posData.Position, item.RoomNumber, -Vector3::UnitY, CLICK(1));

		// Don't process material if foot hit bridge object.
		// TODO: Handle bridges once bridge collision is less stupid.
		if (pointColl.GetFloorBridgeItemNumber() != NO_VALUE)
			return;

		// Get and emit footstep sound for floor material.
		auto sfx = GetFootprintSfx(pointColl.GetBottomSector().GetSurfaceMaterial(pointColl.GetPosition().x, pointColl.GetPosition().z, true));
		if (sfx != SOUND_EFFECTS::SFX_TR4_LARA_FOOTSTEPS) // HACK: Must be here until reference WAD2 is revised.
		{
			auto pose = item.Pose;
			SoundEffect(sfx, &pose);
		}

		// Check floor material.
		if (!TestMaterial(pointColl.GetBottomSector().GetSurfaceMaterial(pointColl.GetPosition().x, pointColl.GetPosition().z, true), FootprintMaterials))
			return;

		auto vertexPoints = GetFootprintVertexPoints(item, posData.Position, pointColl.GetFloorNormal());

		// Test floor continuity.
		if (!TestFootprintFloor(item, posData.Position, vertexPoints))
			return;

		SpawnFootprint(isRight, vertexPoints);
	}

	void UpdateFootprints()
	{
		if (Footprints.empty())
			return;

		for (auto& footprint: Footprints)
		{
			if (footprint.Life <= 0.0f)
				continue;

			// Update opacity.
			if (footprint.Life <= footprint.LifeStartFading)
			{
				float alpha = 1.0f - (footprint.Life / footprint.LifeStartFading);
				footprint.Opacity = Lerp(footprint.OpacityStart, 0.0f, alpha);
			}

			// Update life.
			footprint.Life -= 1.0f;
		}

		ClearInactiveEffects(Footprints);
	}

	void ClearFootprints()
	{
		Footprints.clear();
	}
}
