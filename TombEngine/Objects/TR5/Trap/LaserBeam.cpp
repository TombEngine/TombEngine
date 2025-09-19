#include "framework.h"
#include "Objects/TR5/Trap/LaserBeam.h"

#include "Game/collision/collide_room.h"
#include "Game/collision/floordata.h"
#include "Game/collision/Los.h"
#include "Game/collision/Point.h"
#include "Game/control/Los.h"
#include "Game/effects/effects.h"
#include "Game/effects/item_fx.h"
#include "Game/effects/spark.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/people.h"
#include "Math/Math.h"
#include "Renderer/Renderer.h"
#include "Specific/level.h"

using namespace TEN::Collision::Los;
using namespace TEN::Collision::Point;
using namespace TEN::Effects::Items;
using namespace TEN::Effects::Spark;
using namespace TEN::Math;
using namespace TEN::Renderer;

namespace TEN::Entities::Traps
{
	constexpr auto LASER_BEAM_LIGHT_INTENSITY	  = 0.2f;
	constexpr auto LASER_BEAM_LIGHT_AMPLITUDE_MAX = 0.1f;

	extern std::unordered_map<int, LaserBeamEffect> LaserBeams = {};
	static std::array<TransientBeam, MAX_TRANSIENT_LASERS> GTransientPool{};

	static int AcquireTransientIndex()
	{
		int freeIndex = -1;

		for (int i = 0; i < MAX_TRANSIENT_LASERS; ++i)
		{
			const auto& s = GTransientPool[i];
			if (!s.On || s.Life <= 0.0f || !s.Effect.IsActive)
			{
				freeIndex = i;
				break;
			}
		}

		if (freeIndex != -1)
			return freeIndex;

		float shortest = FLT_MAX;
		int   best = 0;
		for (int i = 0; i < MAX_TRANSIENT_LASERS; ++i)
		{
			const auto& s = GTransientPool[i];
			if (s.Life < shortest)
			{
				shortest = s.Life;
				best = i;
			}
		}
		return best;
	}

	void LaserBeamEffect::Initialize(const ItemInfo& item)
	{
		constexpr auto RADIUS_STEP = BLOCK(0.002f);

		Color = item.Model.Color;
		Color.w = 1.0f;
		Radius = (item.TriggerFlags == 0) ? RADIUS_STEP : (abs(item.TriggerFlags) * RADIUS_STEP);
		IsLethal = (item.TriggerFlags > 0);
		IsHeavyActivator = (item.TriggerFlags <= 0);
	}

	static void SpawnLaserSpark(const GameVector& pos, short angle, int count, const Vector4& colorStart)
	{
		for (int i = 0; i < count; i++)
		{
			float ang = TO_RAD(angle);
			auto vel = Vector3(
				sin(ang + Random::GenerateFloat(-PI_DIV_2, PI_DIV_2)),
				Random::GenerateFloat(-1, 1),
				cos(ang + Random::GenerateFloat(-PI_DIV_2, PI_DIV_2)));
			vel += Vector3(Random::GenerateFloat(-64.0f, 64.0f), Random::GenerateFloat(-64.0f, 64.0f), Random::GenerateFloat(-64, 64.0f));
			vel.Normalize(vel);

			auto& spark = GetFreeSparkParticle();
			spark = {};

			// TODO: Demagic.
			spark.age = 0.0f;
			spark.life = Random::GenerateFloat(10, 20);
			spark.friction = 0.98f;
			spark.gravity = 1.2f;
			spark.width = 7.0f;
			spark.height = 34.0f;
			spark.room = pos.RoomNumber;
			spark.pos = pos.ToVector3();
			spark.velocity = vel * Random::GenerateFloat(17.0f, 24.0f);
			spark.sourceColor = colorStart;
			spark.destinationColor = Vector4::Zero;
			spark.active = true;
		}
	}

	static void SpawnLaserBeamLight(const Vector3& pos, int roomNumber, const Color& color, float intensity, float amplitudeMax)
	{
		constexpr auto LASER_BEAM_FALLOFF = BLOCK(1.5f);

		float intensityNorm = intensity - Random::GenerateFloat(0.0f, amplitudeMax);
		SpawnDynamicPointLight(pos, color * intensityNorm, LASER_BEAM_FALLOFF);
	}

	void LaserBeamEffect::StoreInterpolationData()
	{
		for (int i = 0; i < Vertices.size(); i++)
		{
			OldVertices[i] = Vertices[i];
		}

		OldColor = Color;
	}

	void UpdateTransientLaserBeams()
	{
		for (int i = 0; i < MAX_TRANSIENT_LASERS; ++i)
		{
			auto& s = GTransientPool[i];

			s.Life -= 1.2f;
			if (!s.On || s.Life <= 0.0f || !s.Effect.IsActive)
				continue;

					if (s.Life <= 0.0f)
			{
				s.On = false;
				s.Effect.IsActive = false;
				continue;
			}
		}
	}

	void EmitTransientLaserBeam(const GameVector& position, const EulerAngles& orientation, float radius, const Vector4& color, bool isLethal, bool hasSparks, bool isHeavyActivator)
	{
		//free slot
		const int idx = AcquireTransientIndex();
		auto& slot = GTransientPool[idx];

		constexpr auto RADIUS_STEP = BLOCK(0.002f);
		
		slot.Pos = position.ToVector3();
		slot.RoomNumber = position.RoomNumber;
		slot.Orientation = orientation;
		slot.ModelColor = color;
		slot.Effect.Color = color;
		slot.Life = 2.0f;
		slot.On = true;
		slot.Effect.Radius = radius * RADIUS_STEP;
				
		auto orient = orientation;
		auto dir = orient.ToDirection();
		auto rotMatrix = orient.ToRotationMatrix();

		// Hit wall; spawn sparks and light.
		auto los = GetRoomLosCollision(position.ToVector3(), slot.RoomNumber, dir, MAX_VISIBILITY_DISTANCE);
		if (los.IsIntersected)
		{
			if (hasSparks)
			{
				auto targetGameVector = GameVector(los.Position, los.RoomNumber);
				SpawnLaserSpark(targetGameVector, Random::GenerateAngle(), 3, slot.Effect.Color);
				SpawnLaserSpark(targetGameVector, Random::GenerateAngle(), 3, slot.Effect.Color);
			}

			SpawnLaserBeamLight(los.Position, los.RoomNumber, slot.Effect.Color, LASER_BEAM_LIGHT_INTENSITY, LASER_BEAM_LIGHT_AMPLITUDE_MAX);
		}

		float length = Vector3::Distance(position.ToVector3(), los.Position);

		// Calculate cylinder vertices.
		float angle = 0.0f;
		for (int i = 0; i < LaserBeamEffect::SUBDIVISION_COUNT; i++)
		{
			float sinAngle = sin(angle);
			float cosAngle = cos(angle);

			auto relVertex = Vector3(slot.Effect.Radius * sinAngle, slot.Effect.Radius * cosAngle, 0.0f);
			auto vertex = position.ToVector3() + Vector3::Transform(relVertex, rotMatrix);

			slot.Effect.Vertices[i] = vertex;
			slot.Effect.Vertices[slot.Effect.SUBDIVISION_COUNT + i] = Geometry::TranslatePoint(vertex, dir, length);

			angle += PI_MUL_2 / slot.Effect.SUBDIVISION_COUNT;
		}

		//DrawDebugLine(position.ToVector3(), los.Position, Vector4(0, 1, 0, 1), RendererDebugPage::None);
		bool los2 = LOS(&position, &GameVector(los.Position, los.RoomNumber));

		auto hitPos = Vector3i::Zero;

		GameVector tempOrigin = position;
		GameVector tempTarget(los.Position, los.RoomNumber);

		if (ObjectOnLOS2(&tempOrigin, &tempTarget, &hitPos, nullptr, ID_LARA) == LaraItem->Index && !los2)
		{
			if (isLethal &&
				LaraItem->HitPoints > 0 && LaraItem->Effect.Type != EffectType::Smoke)
			{
				ItemRedLaserBurn(LaraItem, FPS * 2);
				DoDamage(LaraItem, MAXINT);
			}
			else if (isHeavyActivator)
			{
				//TestTriggers(&item, true, item.Flags & IFLAG_ACTIVATION_MASK);
			}

			slot.Effect.Color.w = Random::GenerateFloat(0.6f, 1.0f);
			SpawnLaserBeamLight(position.ToVector3(), position.RoomNumber, color, LASER_BEAM_LIGHT_INTENSITY, LASER_BEAM_LIGHT_AMPLITUDE_MAX);
		}
 
		std::copy(slot.Effect.Vertices.begin(), slot.Effect.Vertices.end(), slot.Effect.OldVertices.begin());
		slot.Effect.OldColor = slot.Effect.Color;
		slot.Effect.IsActive = true;
	}

	const std::array<TransientBeam, MAX_TRANSIENT_LASERS>& GetTransientLaserPool()
	{
		return GTransientPool;
	}


	void LaserBeamEffect::Update(const ItemInfo& item)
	{
		auto orient = EulerAngles(item.Pose.Orientation.x + ANGLE(180.0f), item.Pose.Orientation.y, item.Pose.Orientation.z);
		auto dir = orient.ToDirection();
		auto rotMatrix = orient.ToRotationMatrix();

		// Hit wall; spawn sparks and light.
		auto los = GetRoomLosCollision(item.Pose.Position.ToVector3(), item.RoomNumber, dir, MAX_VISIBILITY_DISTANCE);
		if (los.IsIntersected)
		{
			if (item.TriggerFlags > 0)
			{
				auto targetGameVector = GameVector(los.Position, los.RoomNumber);
				SpawnLaserSpark(targetGameVector, Random::GenerateAngle(), 3, Color);
				SpawnLaserSpark(targetGameVector, Random::GenerateAngle(), 3, Color);
			}

			SpawnLaserBeamLight(los.Position, los.RoomNumber, item.Model.Color, LASER_BEAM_LIGHT_INTENSITY, LASER_BEAM_LIGHT_AMPLITUDE_MAX);
		}

		float length = Vector3::Distance(item.Pose.Position.ToVector3(), los.Position);

		// Calculate cylinder vertices.
		float angle = 0.0f;
		for (int i = 0; i < LaserBeamEffect::SUBDIVISION_COUNT; i++)
		{
			float sinAngle = sin(angle);
			float cosAngle = cos(angle);

			auto relVertex = Vector3(Radius * sinAngle, Radius * cosAngle, 0.0f);
			auto vertex = item.Pose.Position.ToVector3() + Vector3::Transform(relVertex, rotMatrix);

			Vertices[i] = vertex;
			Vertices[SUBDIVISION_COUNT + i] = Geometry::TranslatePoint(vertex, dir, length);

			angle += PI_MUL_2 / SUBDIVISION_COUNT;
		}

		// Calculate bounding box.
		float boxApothem = (Radius - ((Radius * SQRT_2) - Radius) + Radius) / 2;
		auto center = (item.Pose.Position.ToVector3() + los.Position) / 2;
		auto extents = Vector3(boxApothem, boxApothem, length / 2);
		BoundingBox = BoundingOrientedBox(center, extents, orient.ToQuaternion());
	}

	void InitializeLaserBeam(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];

		auto beam = LaserBeamEffect{};
		beam.Initialize(item);

		LaserBeams.insert({ itemNumber, beam });
	}

	void ControlLaserBeam(short itemNumber)
	{
		if (!LaserBeams.count(itemNumber))
			return;

		auto& item = g_Level.Items[itemNumber];
		auto& beam = LaserBeams.at(itemNumber);

		if (!TriggerActive(&item))
		{
			beam.IsActive = false;
			beam.Color.w = 0.0f;
			item.Model.Color.w = 0.0f;
			return;
		}

		beam.StoreInterpolationData();

		// Brightness fade-in and distortion.
		if (item.Model.Color.w < 1.0f)
			item.Model.Color.w += 0.02f;

		if (beam.Color.w < 1.0f)
			beam.Color.w += 0.02f;

		// TODO: Weird.
		if (item.Model.Color.w > 8.0f)
		{
			beam.Color.w = 0.8f;
			item.Model.Color.w = 0.8f;
		}

		beam.IsActive = true;
		beam.Update(item);

		if (item.Model.Color.w >= 0.8f)
			SpawnLaserBeamLight(item.Pose.Position.ToVector3(), item.RoomNumber, item.Model.Color, LASER_BEAM_LIGHT_INTENSITY, LASER_BEAM_LIGHT_AMPLITUDE_MAX);

		SoundEffect(SFX_TR5_DOOR_BEAM, &item.Pose);
	}

	void CollideLaserBeam(short itemNumber, ItemInfo* playerItem, CollisionInfo* coll)
	{
		if (!LaserBeams.count(itemNumber))
			return;

		auto& item = g_Level.Items[itemNumber];
		auto& beam = LaserBeams.at(itemNumber);

		if (!beam.IsActive)
			return;

		auto origin = GameVector(item.Pose.Position, item.RoomNumber);
		auto basePos = origin.ToVector3();

		auto rotMatrix = EulerAngles(item.Pose.Orientation.x + ANGLE(180.0f), item.Pose.Orientation.y, item.Pose.Orientation.z);
		auto target = GameVector(Geometry::TranslatePoint(origin.ToVector3(), rotMatrix, MAX_VISIBILITY_DISTANCE), 0);

		auto pointColl = GetPointCollision(target.ToVector3i(), item.RoomNumber);
		if (pointColl.GetRoomNumber() != target.RoomNumber)
			target.RoomNumber = pointColl.GetRoomNumber();

		bool los2 = LOS(&origin, &target);

		auto hitPos = Vector3i::Zero;
		if (ObjectOnLOS2(&origin, &target, &hitPos, nullptr, ID_LARA) == LaraItem->Index && !los2)
		{
			if (beam.IsLethal &&
				playerItem->HitPoints > 0 && playerItem->Effect.Type != EffectType::Smoke)
			{
				ItemRedLaserBurn(playerItem, FPS * 2);
				DoDamage(playerItem, MAXINT);
			}
			else if (beam.IsHeavyActivator)
			{
				TestTriggers(&item, true, item.Flags & IFLAG_ACTIVATION_MASK);
			}

			beam.Color.w = Random::GenerateFloat(0.6f, 1.0f);
			SpawnLaserBeamLight(item.Pose.Position.ToVector3(), item.RoomNumber, item.Model.Color, LASER_BEAM_LIGHT_INTENSITY, LASER_BEAM_LIGHT_AMPLITUDE_MAX);
		}
	}

	void ClearLaserBeamEffects()
	{
		LaserBeams.clear();
		for (auto& s : GTransientPool) { s = {}; }
	}
}
