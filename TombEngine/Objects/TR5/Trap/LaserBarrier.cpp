#include "framework.h"
#include "Objects/TR5/Trap/LaserBarrier.h"

#include "Game/collision/collide_room.h"
#include "Game/collision/floordata.h"
#include "Game/collision/Point.h"
#include "Game/effects/effects.h"
#include "Game/effects/item_fx.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Specific/level.h"

using namespace TEN::Collision::Point;
using namespace TEN::Effects::Items;

namespace TEN::Entities::Traps
{
	// NOTES:
	// item.ItemFlags[0] = barrier height.
	
	// TODO:
	// - Randomize opacity pulses for each barrier.
	// - Make SpawnLaserBarrierLight() spawn a line of light once engine allows it.
	// - Make beam counts an attribute once attributes are implemented.

	extern std::unordered_map<int, LaserBarrier> LaserBarriers = {};

	void LaserBarrier::Initialize(const ItemInfo& item)
	{
		float barrierHeight = item.ItemFlags[0];
		int beamCount = std::max((int)floor((barrierHeight / 2) / LaserBarrierBeam::HEIGHT), 1);

		Color = item.Model.Color;
		Color.w = 0.0f;
		Beams.resize(beamCount);
		IsLethal = (item.TriggerFlags > 0);
		IsHeavyActivator = (item.TriggerFlags <= 0);
		Update(item);
	}

	void LaserBarrier::Update(const ItemInfo& item)
	{
		// Calculate dimension values.
		float barrierHalfWidth = (abs(item.TriggerFlags) * BLOCK(1)) / 2;
		float barrierHeight = item.ItemFlags[0];
		float beamStepHeight = barrierHeight / Beams.size();

		// Determine beam vertex base.
		auto basePos = item.Pose.Position.ToVector3();
		auto rotMatrix = EulerAngles(0, item.Pose.Orientation.y + ANGLE(90.0f), 0).ToRotationMatrix();
		auto baseVertices = std::array<Vector3, LaserBarrierBeam::VERTEX_COUNT>
		{
			basePos + Vector3::Transform(Vector3(barrierHalfWidth, -LaserBarrierBeam::HEIGHT / 2, 0.0f), rotMatrix),
			basePos + Vector3::Transform(Vector3(-barrierHalfWidth, -LaserBarrierBeam::HEIGHT / 2, 0.0f), rotMatrix),
			basePos + Vector3::Transform(Vector3(-barrierHalfWidth, LaserBarrierBeam::HEIGHT / 2, 0.0f), rotMatrix),
			basePos + Vector3::Transform(Vector3(barrierHalfWidth, LaserBarrierBeam::HEIGHT / 2, 0.0f), rotMatrix)
		};

		// Set vertex positions.
		auto beamOffset = Vector3(0.0f, -LaserBarrierBeam::HEIGHT, 0.0f);
		for (auto& beam : Beams)
		{
			TENAssert(beam.VertexPoints.size() == baseVertices.size(), "Laser barrier beam vertex count out of sync.");

			for (int i = 0; i < beam.VertexPoints.size(); i++)
				beam.VertexPoints[i] = baseVertices[i] + beamOffset;

			beamOffset.y -= beamStepHeight;
		}

		// Determine bounding box reference points.
		auto point0 = Beams.back().VertexPoints[0];
		auto point1 = Beams.back().VertexPoints[1];
		auto point2 = Beams.front().VertexPoints[2];
		auto point3 = Beams.front().VertexPoints[3];

		// Update bounding box.
		BoundingBox.Center = (point0 + point1 + point2 + point3) / 4;
		BoundingBox.Extents = Vector3(
			std::abs(point0.x - point1.x) / 2,
			std::abs(point0.y - point2.y) / 2,
			std::abs(point0.z - point2.z) / 2);
	}

	void InitializeLaserBarrier(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		// Initialize barrier height.
		auto pointColl = GetPointCollision(item);
		float barrierHeight = item.Pose.Position.y - pointColl.GetCeilingHeight();
		item.ItemFlags[0] = barrierHeight;

		item.Collidable = false;

		// Initialize barrier effect.
		auto barrier = LaserBarrier{};
		barrier.Initialize(item);
		LaserBarriers.insert({ itemNumber, barrier });
	}

	static void SpawnLaserBarrierLight(const ItemInfo& item, float intensity, float amplitude)
	{
		float intensityNorm = intensity - Random::GenerateFloat(0.0f, amplitude);
		SpawnDynamicLight(
			item.Pose.Position.x, item.Pose.Position.y, item.Pose.Position.z,
			8,
			intensityNorm * (item.Model.Color.x / 2),
			intensityNorm * (item.Model.Color.y / 2),
			intensityNorm * (item.Model.Color.z / 2));
	}

	void ControlLaserBarrier(short itemNumber)
	{
		constexpr auto LIGHT_INTENSITY = 150.0f;
		constexpr auto LIGHT_AMPLITUDE = 31.0f;

		if (!LaserBarriers.count(itemNumber))
			return;

		auto& item = g_Level.Items[itemNumber];
		auto& barrier = LaserBarriers.at(itemNumber);

		if (!TriggerActive(&item))
		{
			barrier.IsActive = false;
			barrier.Color.w = 0.0f;
			item.Model.Color.w = 0.0f;
			return;
		}

		// Brightness fade-in and distortion.
		if (item.Model.Color.w < 1.0f)
			item.Model.Color.w += 0.02f;

		if (barrier.Color.w < 1.0f)
			barrier.Color.w += 0.02f;

		// TODO: Weird.
		if (item.Model.Color.w > 8.0f)
		{
			barrier.Color.w = 0.8f;
			item.Model.Color.w = 0.8f;
		}
			
		barrier.IsActive = true;
		barrier.Update(item);

		if (item.Model.Color.w >= 0.8f)
			SpawnLaserBarrierLight(item, LIGHT_INTENSITY, LIGHT_AMPLITUDE);

		SoundEffect(SFX_TR5_DOOR_BEAM, &item.Pose);
	}

	void CollideLaserBarrier(short itemNumber, ItemInfo* playerItem, CollisionInfo* coll)
	{
		constexpr auto LIGHT_INTENSITY = 255.0f;
		constexpr auto LIGHT_AMPLITUDE = 100.0f;

		if (!LaserBarriers.count(itemNumber))
			return;

		auto& item = g_Level.Items[itemNumber];
		auto& barrier = LaserBarriers.at(itemNumber);

		if (!barrier.IsActive)
			return;

		auto playerBox = GameBoundingBox(playerItem).ToBoundingOrientedBox(playerItem->Pose);
		if (barrier.BoundingBox.Intersects(playerBox))
		{
			if (barrier.IsLethal &&
				playerItem->HitPoints > 0 && playerItem->Effect.Type != EffectType::Smoke)
			{
				ItemRedLaserBurn(playerItem, 2.0f * FPS);
				DoDamage(playerItem, MAXINT);
			}
			else if (barrier.IsHeavyActivator)
			{
				TestTriggers(&item, true, item.Flags & IFLAG_ACTIVATION_MASK);
			}

			barrier.Color.w = Random::GenerateFloat(0.6f, 1.0f);
			SpawnLaserBarrierLight(item, LIGHT_INTENSITY, LIGHT_AMPLITUDE);
		}		
	}

	void ClearLaserBarrierEffects()
	{
		LaserBarriers.clear();
	}
}
