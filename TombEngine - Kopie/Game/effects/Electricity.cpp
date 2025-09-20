#include "framework.h"
#include "Game/effects/Electricity.h"

#include "Game/effects/effects.h"
#include "Game/effects/spark.h"
#include "Game/people.h"
#include "Game/Setup.h"
#include "Math/Math.h"

using namespace TEN::Math;
using namespace TEN::Effects::Spark;

namespace TEN::Effects::Electricity
{
	constexpr auto HELICAL_LASER_LIFE_MAX = 18.0f;

	std::vector<Electricity>  ElectricityArcs = {};
	std::vector<HelicalLaser> HelicalLasers = {};

	std::array<Vector3, ELECTRICITY_KNOTS_SIZE>	 ElectricityKnots = {};
	std::array<Vector3, ELECTRICITY_BUFFER_SIZE> ElectricityBuffer = {};

	// BIG TODO: Make a family of Bezier, B-Spline, and Catmull-Rom curve classes.

	// More standard version.
	// TODO: Adopt this in place of the one below.
	static Vector3 CatmullRomSpline(float alpha, const std::array<Vector3, 4>& knots)
	{
		auto point1 = knots[1] + ((knots[2] - knots[0]) * (1 / 6.0f));
		auto point2 = knots[2];
		auto point3 = knots[2] + ((knots[3] - knots[1]) * (-1 / 6.0f));
		auto point4 = knots[3];

		auto spline = ((point2 * 2) + (point3 - point1) * alpha) +
			(((point1 * 2) - (point2 * 5) + (point3 * 4) - point4) * SQUARE(alpha)) +
			(((point1 * -1) + (point2 * 3) - (point3 * 3) + point4) * CUBE(alpha));
		return spline;
	}

	// 4-point Catmull-Rom spline interpolation.
	// Function takes reference to array of knots and
	// calculates using subset of 4 determined by alpha value.
	static Vector3 ElectricitySpline(const std::array<Vector3, ELECTRICITY_KNOTS_SIZE>& knots, float alpha)
	{
		alpha *= ELECTRICITY_KNOTS_SIZE - 3;

		int span = alpha;
		if (span >= (ELECTRICITY_KNOTS_SIZE - 3))
			span = ELECTRICITY_KNOTS_SIZE - 4;

		float something = alpha - span;

		// Determine subset of 4 knots.
		const auto& knot0 = knots[span];
		const auto& knot1 = knots[span + 1];
		const auto& knot2 = knots[span + 2];
		const auto& knot3 = knots[span + 3];

		auto point1 = knot1 + (knot1 / 2) - (knot2 / 2) - knot2 + (knot3 / 2) + ((-knot0 - Vector3::One) / 2);
		auto ret = point1 * something;
		auto point2 = ret + Vector3(2.0f) * knot2 - 2 * knot1 - (knot1 / 2) - (knot3 / 2) + knot0;
		ret = point2 * something;
		auto point3 = ret + (knot2 / 2) + ((-knot0 - Vector3::One) / 2);
		ret = point3 * something;

		return (ret + knot1);
	}

	// TODO: Pass const Vector4& for color.
	void SpawnElectricity(const Vector3& origin, const Vector3& target, float amplitude, byte r, byte g, byte b, float life, int flags, float width, unsigned int numSegments)
	{
		auto arc = Electricity();

		arc.pos1 = origin;
		arc.pos2 = ((origin * 3) + target) / 4;
		arc.pos3 = ((target * 3) + origin) / 4;
		arc.pos4 = target;
		arc.flags = flags;

		for (int i = 0; i < arc.interpolation.size(); i++)
		{
			if (arc.flags & (int)ElectricityFlags::MoveEnd || i < (arc.interpolation.size() - 1))
			{
				arc.interpolation[i] = Vector3(
					fmod(Random::GenerateInt(), amplitude),
					fmod(Random::GenerateInt(), amplitude),
					fmod(Random::GenerateInt(), amplitude)) -
					Vector3(amplitude / 2);
			}
			else
			{
				arc.interpolation[i] = Vector3::Zero;
			}
		}

		arc.r = r;
		arc.g = g;
		arc.b = b;
		arc.life = life;
		arc.segments = numSegments;
		arc.amplitude = amplitude;
		arc.width = width;

		ElectricityArcs.push_back(arc);
	}

	void SpawnElectricityGlow(const Vector3& pos, float scale, byte r, byte g, byte b)
	{
		auto& spark = *GetFreeParticle();

		spark.on = true;
		spark.SpriteSeqID = ID_MISC_SPRITES;
		spark.SpriteID = 0;
		spark.blendMode = BlendMode::Additive;
		spark.x = pos.x;
		spark.y = pos.y;
		spark.z = pos.z;
		spark.xVel = 0;
		spark.yVel = 0;
		spark.zVel = 0;
		spark.sR = r;
		spark.sG = g;
		spark.sB = b;
		spark.dR = r;
		spark.dG = g;
		spark.dB = b;
		spark.life = 4;
		spark.sLife = 4;
		spark.colFadeSpeed = 2;
		spark.fadeToBlack = 0;
		spark.scalar = 3;
		spark.maxYvel = 0;
		spark.gravity = 0;
		spark.sSize =
		spark.dSize =
		spark.size = scale + Random::GenerateInt(0, 4);
		spark.flags = SP_DEF | SP_SCALE;
	}

	void SpawnElectricEffect(const ItemInfo& item, int jointNumber, const Vector3i& offset, const float spawnRadius, float beamOriginRadius, float beamTargetRadius, int frequency, const Vector3& pos)
	{
		// TODO: Make electric effect correctly spawn randomly on any mesh bounds surface and water surfaces. -- Tomo
		
		int randomIndex = Random::GenerateInt(0, 100);
		auto pos1 = Vector3::Zero;

		if (randomIndex < frequency)
		{
			if (pos != Vector3::Zero)
			{
				pos1 = pos + offset.ToVector3();		
			}
			else
			{
				pos1 = GetJointPosition(item, jointNumber, offset).ToVector3();
			}

			auto sphere = BoundingSphere(pos1, spawnRadius);
			auto pos2 = Random::GeneratePointOnSphere(sphere);

			SpawnElectricityGlow(pos2, 28, 32, 32, 64);

			SpawnCyborgSpark(pos2);
			SpawnDynamicLight(pos2.x, pos2.y, pos2.z, Random::GenerateInt(4, 8), 31, 63, 127);

			sphere = BoundingSphere(pos1, beamOriginRadius);
			auto sphere1 = BoundingSphere(pos1, beamTargetRadius);
			pos1 = Random::GeneratePointOnSphere(sphere);
			pos2 = Random::GeneratePointOnSphere(sphere1);

			SpawnElectricity(pos1, pos2, Random::GenerateInt(8, 16), 32, 64, 128, 24, (int)ElectricityFlags::Spline | (int)ElectricityFlags::ThinOut | (int)ElectricityFlags::ThinIn, 6, 8);
		}
	}

	void SpawnHelicalLaser(const Vector3& origin, const Vector3& target)
	{
		constexpr auto SEGMENT_COUNT_MAX = 128;
		constexpr auto COLOR			 = Vector4(0.0f, 0.375f, 1.0f, 1.0f);
		constexpr auto LENGTH_MAX		 = BLOCK(4);
		constexpr auto ROT				 = ANGLE(-10.0f);
		constexpr auto ELECTRICITY_FLAGS = (int)ElectricityFlags::ThinIn | (int)ElectricityFlags::ThinOut;

		auto laser = HelicalLaser();

		laser.NumSegments = SEGMENT_COUNT_MAX;
		laser.Origin = origin;
		laser.Target = target;
		laser.Orientation2D = Random::GenerateAngle();
		laser.LightPosition = origin;
		laser.Color = COLOR;
		laser.Life = HELICAL_LASER_LIFE_MAX;
		laser.Radius = 0.0f;
		laser.Length = LENGTH_MAX / 2;
		laser.LengthEnd = LENGTH_MAX;
		laser.Opacity = 1.0f;
		laser.Rotation = ROT;

		HelicalLasers.push_back(laser);

		SpawnElectricity(origin, target, 1, 0, laser.Color.x * UCHAR_MAX, laser.Color.z * UCHAR_MAX, 20, ELECTRICITY_FLAGS, 19, 5);
		SpawnElectricity(origin, target, 1, 110, 255, 250, 20, ELECTRICITY_FLAGS, 4, 5);
		SpawnElectricityGlow(laser.LightPosition, 0, 0, (laser.Color.x / 2) * UCHAR_MAX, (laser.Color.z / 2) * UCHAR_MAX);
	}

	void UpdateHelicalLasers()
	{
		constexpr auto LIFE_START_FADING = HELICAL_LASER_LIFE_MAX / 2;
		constexpr auto LENGTH_LERP_ALPHA = 0.25f;

		if (HelicalLasers.empty())
			return;

		for (auto& laser : HelicalLasers)
		{
			laser.StoreInterpolationData();

			// Set to despawn.
			laser.Life -= 1.0f;
			if (laser.Life <= 0.0f)
				continue;

			// Update length.
			laser.Length = Lerp(laser.Length, laser.LengthEnd, LENGTH_LERP_ALPHA);

			// Update radius.
			laser.Radius += 1 / 8.0f;

			// Update opacity.
			float alpha = laser.Life / LIFE_START_FADING;
			laser.Opacity = Lerp(0.0f, 1.0f, alpha);

			// Update orientation.
			laser.Orientation2D += laser.Rotation;
		}

		// Despawn inactive effects.
		HelicalLasers.erase(
			std::remove_if(
				HelicalLasers.begin(), HelicalLasers.end(),
				[](const HelicalLaser& laser) { return (laser.Life <= 0.0f); }), HelicalLasers.end());
	}

	void UpdateElectricityArcs()
	{
		// No active effects; return early.
		if (ElectricityArcs.empty())
			return;

		for (auto& arc : ElectricityArcs)
		{
			// Set to despawn.
			if (arc.life <= 0.0f)
				continue;

			arc.StoreInterpolationData();
			
			// If/when this behaviour is changed, modify AddLightningArc accordingly.
			arc.life -= 2.0f;
			if (arc.life > 0.0f)
			{
				// TODO: Find a better way to do this.
				auto* posPtr = (Vector3*)&arc.pos2;
				for (auto& interpPos : arc.interpolation)
				{
					*posPtr += interpPos * 2;
					interpPos -= (interpPos / 16);

					posPtr++;
				}
			}
		}

		// Despawn inactive effects.
		ElectricityArcs.erase(
			std::remove_if(
				ElectricityArcs.begin(), ElectricityArcs.end(),
				[](const Electricity& arc) { return (arc.life <= 0.0f); }), ElectricityArcs.end());
	}

	void CalculateElectricitySpline(const Electricity& arc, const std::array<Vector3, ELECTRICITY_KNOTS_SIZE>& knots, std::array<Vector3, ELECTRICITY_BUFFER_SIZE>& buffer)
	{
		int bufferIndex = 0;

		buffer[bufferIndex] = knots[0];
		bufferIndex++;

		// Splined arc.
		if (arc.flags & (int)ElectricityFlags::Spline)
		{
			float interpStep = 1.0f / ((arc.segments * 3) - 1);
			float alpha = interpStep;

			if (((arc.segments * 3) - 2) > 0)
			{
				for (int i = (arc.segments * 3) - 2; i > 0; i--)
				{
					auto spline = ElectricitySpline(knots, alpha);
					auto sphere = BoundingSphere(Vector3::Zero, 8.0f);
					auto offset = Random::GeneratePointInSphere(sphere);

					buffer[bufferIndex] = spline + offset;

					alpha += interpStep;
					bufferIndex++;
				}
			}
		}
		// Straight arc.
		else
		{
			int numSegments = (arc.segments * 3) - 1;

			auto deltaPos = (knots[knots.size() - 1] - knots[0]) / numSegments;
			auto pos = knots[0] + deltaPos + Vector3(
				fmod(Random::GenerateInt(), arc.amplitude * 2),
				fmod(Random::GenerateInt(), arc.amplitude * 2),
				fmod(Random::GenerateInt(), arc.amplitude * 2)) -
				Vector3(arc.amplitude);

			if (((arc.segments * 3) - 2) > 0)
			{
				for (int i = (arc.segments * 3) - 2; i > 0; i--)
				{
					buffer[bufferIndex] = pos;
					bufferIndex++;

					pos += deltaPos + Vector3(
						fmod(Random::GenerateInt(), arc.amplitude * 2),
						fmod(Random::GenerateInt(), arc.amplitude * 2),
						fmod(Random::GenerateInt(), arc.amplitude * 2)) -
						Vector3(arc.amplitude);
				}
			}
		}

		buffer[bufferIndex] = knots[5];
	}

	void CalculateHelixSpline(const HelicalLaser& laser, std::array<Vector3, ELECTRICITY_KNOTS_SIZE>& knots, std::array<Vector3, ELECTRICITY_BUFFER_SIZE>& buffer)
	{
		constexpr auto ANGLE_STEP = ANGLE(25.0f);

		int bufferIndex = 0;

		buffer[bufferIndex] = knots[0];
		bufferIndex++;

		const auto& origin = knots[0];
		const auto& target = knots[1];
		auto dir = target - origin;
		dir.Normalize();

		float lengthStep = laser.Length / laser.NumSegments;
		auto orient = AxisAngle(dir, laser.Orientation2D);

		for (int i = 0; i < laser.NumSegments; i++)
		{
			// Calculate knot.
			orient.SetAngle(orient.GetAngle() + ANGLE_STEP);
			auto knotBase = Geometry::TranslatePoint(origin, orient.GetAxis(), lengthStep * i);
			auto knot = Geometry::TranslatePoint(knotBase, orient, laser.Radius * i);

			buffer[bufferIndex] = origin + knot;
			bufferIndex++;
		}

		buffer[bufferIndex] = knots[1];
	}
}