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
	constexpr auto LASER_BEAM_LIGHT_INTENSITY		= 0.2f;
	constexpr auto LASER_BEAM_LIGHT_AMPLITUDE_MAX	= 0.1f;
	constexpr auto LASER_BEAM_FADE_SPEED			= 0.05f;

	extern std::unordered_map<int, LaserBeamEffect> LaserBeams = {};

	void LaserBeamEffect::Initialize(const ItemInfo& item)
	{
		constexpr auto RADIUS_STEP = BLOCK(0.002f);

		Origin = GameVector(item.Pose.Position, item.RoomNumber);
		Rotation = EulerAngles(item.Pose.Orientation.x + ANGLE(180.0f), item.Pose.Orientation.y, item.Pose.Orientation.z);
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

			// TODO: Demagic.
			auto& spark = GetFreeSparkParticle();
			spark = {};
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
		// Store old data for interpolation.
		if (!IsDirty)
			return;

		OldVertices = Vertices;
		OldColor = Color;
	}

	void LaserBeamEffect::Update(const ItemInfo& item)
	{
		// Check for origin/rotation changes.
		auto newOrigin = GameVector(item.Pose.Position, item.RoomNumber);
		auto newRotation = EulerAngles(item.Pose.Orientation.x + ANGLE(180.0f), item.Pose.Orientation.y, item.Pose.Orientation.z);

		IsDirty = !IsActive || Rotation != newRotation || Origin != newOrigin;

		if (IsDirty)
		{
			Origin = newOrigin;
			Rotation = newRotation;
		}

		// Recalculate laser beam geometry.
		if (!IsDirty)
			return;

		StoreInterpolationData();

		auto pos = Origin.ToVector3();
		auto dir = Rotation.ToDirection();
		auto los = GetRoomLosCollision(pos, item.RoomNumber, dir, (float)item.ItemFlags[1]);

		Target = GameVector(los.Position, los.RoomNumber);
		float length = Vector3::Distance(pos, los.Position);

		// Calculate cylinder vertices.
		float angle = 0.0f;
		for (int i = 0; i < LaserBeamEffect::SUBDIVISION_COUNT; i++)
		{
			float sinAngle = sinf(angle);
			float cosAngle = cosf(angle);
			auto relVertex = Vector3(Radius * sinAngle, Radius * cosAngle, 0.0f);
			auto vertex = pos + Vector3::Transform(relVertex, Rotation.ToRotationMatrix());

			Vertices[i] = vertex;
			Vertices[SUBDIVISION_COUNT + i] = Geometry::TranslatePoint(vertex, dir, length);

			angle += PI_MUL_2 / SUBDIVISION_COUNT;
		}

		// Calculate bounding box.
		float boxApothem = (Radius - ((Radius * SQRT_2) - Radius) + Radius) / 2;
		auto center = (pos + los.Position) / 2;
		auto extents = Vector3(boxApothem, boxApothem, length / 2);

		BoundingBox = BoundingOrientedBox(center, extents, Rotation.ToQuaternion());
	}

	void InitializeLaserBeam(short itemNumber)
	{
		auto& item = g_Level.Items[itemNumber];
		item.ItemFlags[1] = (short)MAX_VISIBILITY_DISTANCE; // Set max laser beam length.

		// Create and initialize laser beam effect.
		auto beam = LaserBeamEffect{};
		beam.Initialize(item);

		LaserBeams.insert({ itemNumber, beam });
	}

	void ControlLaserBeam(short itemNumber)
	{
		// Skip if no laser beam effect exists for this item.
		if (LaserBeams.find(itemNumber) == LaserBeams.end())
			return;

		auto& item = g_Level.Items[itemNumber];
		auto& beam = LaserBeams.at(itemNumber);

		// Check if trigger is active and mark beam as disabled in such case.
		if (!TriggerActive(&item))
		{
			beam.IsActive = false;
			return;
		}

		// Reset color to zero to fade in, if beam was just activated.
		if (!beam.IsActive)
			beam.Color.w = item.Model.Color.w = 0.0f;

		// Brightness fade-in and distortion.
		if (item.Model.Color.w < 1.0f)
			item.Model.Color.w = beam.Color.w += LASER_BEAM_FADE_SPEED;

		// Clamp the value.
		if (item.Model.Color.w > 1.0f)
			beam.Color.w = item.Model.Color.w = 1.0f;

		beam.Update(item);
		beam.IsActive = true;

		// Check if target is calculated, then spawn effect.
		if (beam.Target != GameVector::Zero)
		{
			if (beam.IsLethal)
				SpawnLaserSpark(beam.Target, Random::GenerateAngle(), 6, beam.Color);

			SpawnLaserBeamLight(beam.Target.ToVector3(), beam.Target.RoomNumber, beam.Color, LASER_BEAM_LIGHT_INTENSITY * beam.Color.w, LASER_BEAM_LIGHT_AMPLITUDE_MAX);
		}

		SoundEffect(SFX_TR5_DOOR_BEAM, &item.Pose);
	}

	void CollideLaserBeam(short itemNumber, ItemInfo* playerItem, CollisionInfo* coll)
	{
		// Skip if no laser beam effect exists for this item.
		if (LaserBeams.find(itemNumber) == LaserBeams.end())
			return;

		auto& item = g_Level.Items[itemNumber];
		auto& beam = LaserBeams.at(itemNumber);
		if (!beam.IsActive)
			return;

		// Avoid calling the collision check every frame.
		if ((item.Index % 3) != (GlobalCounter % 3))
			return;

		// Populate the LosRoomNumbers for ObjectOnLOS2.
		LOS(&beam.Origin, &beam.Target);

		// Check collision with Lara.
		auto hitPos = Vector3i::Zero;
		if (ObjectOnLOS2(&beam.Origin, &beam.Target, &hitPos, nullptr, ID_LARA) == LaraItem->Index)
		{
			if (beam.IsLethal && playerItem->HitPoints > 0 && playerItem->Effect.Type != EffectType::Smoke)
			{
				ItemRedLaserBurn(playerItem, FPS * 2);
				DoDamage(playerItem, MAXINT);
			}
			else if (beam.IsHeavyActivator)
			{
				TestTriggers(&item, true, item.Flags & IFLAG_ACTIVATION_MASK);
			}
			beam.Color.w = Random::GenerateFloat(0.6f, 1.0f);
		}
	}

	void ClearLaserBeamEffects()
	{
		LaserBeams.clear();
	}
}
