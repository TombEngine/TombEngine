#include "framework.h"
#include "Math/Random.h"

#include <random>

#include "Math/Constants.h"
#include "Math/Objects/EulerAngles.h"

namespace TEN::Math::Random
{
	static std::mt19937 Engine;

	int GenerateInt(int low, int high)
	{
		if (low == high)
			return low;

		if (low > high)
			std::swap(low, high);

		return (Engine() / (Engine.max() / (high - low + 1) + 1) + low);
	}

	float GenerateFloat(float low, float high)
	{
		if (low == high)
			return low;

		if (low > high)
			std::swap(low, high);

		return ((high - low) * Engine() / Engine.max() + low);
	}

	short GenerateAngle(short low, short high)
	{
		return (short)GenerateInt(low, high);
	}

	Vector2 GenerateDirection2D()
	{
		float angle = GenerateFloat(0.0f, PI_MUL_2); // Generate angle in full circle.
		return Vector2(cos(angle), sin(angle));
	}

	Vector2 GeneratePoint2DInSquare(const Vector2& pos, short orient, float apothem)
	{
		auto rotMatrix = Matrix::CreateRotationZ(orient);
		auto relPoint = Vector2(
			GenerateFloat(-apothem, apothem),
			GenerateFloat(-apothem, apothem));

		return (pos + Vector2::Transform(relPoint, rotMatrix));
	}
	
	Vector2 GeneratePoint2DInCircle(const Vector2& pos, float radius)
	{
		// Use rejection sampling method.
		auto relPoint = Vector2::Zero;
		do
		{
			relPoint = Vector2(
				GenerateFloat(-1.0f, 1.0f),
				GenerateFloat(-1.0f, 1.0f));
		}
		while (relPoint.LengthSquared() > 1.0f);

		return (pos + (relPoint * radius));
	}

	Vector3 GenerateDirection()
	{
		float theta = GenerateFloat(0.0f, PI_MUL_2); // Generate angle in full circle.
		float phi = GenerateFloat(0.0f, PI);		 // Generate angle in sphere's upper half.

		auto dir = Vector3(
			sin(phi) * cos(theta),
			sin(phi) * sin(theta),
			cos(phi));
		dir.Normalize();
		return dir;
	}

	Vector3 GenerateDirectionInCone(const Vector3& dir, float semiangleInDeg)
	{
		float x = GenerateFloat(-semiangleInDeg, semiangleInDeg) * RADIAN;
		float y = GenerateFloat(-semiangleInDeg, semiangleInDeg) * RADIAN;
		float z = GenerateFloat(-semiangleInDeg, semiangleInDeg) * RADIAN;
		auto rotMatrix = Matrix::CreateRotationX(x) * Matrix::CreateRotationY(y) * Matrix::CreateRotationZ(z);

		auto dirInCone = Vector3::TransformNormal(dir, rotMatrix);
		dirInCone.Normalize();
		return dirInCone;
	}

	Vector3 GeneratePointInBox(const BoundingOrientedBox& box)
	{
		auto rotMatrix = Matrix::CreateFromQuaternion(box.Orientation);
		auto relPoint = Vector3(
			GenerateFloat(-box.Extents.x, box.Extents.x),
			GenerateFloat(-box.Extents.y, box.Extents.y),
			GenerateFloat(-box.Extents.z, box.Extents.z));

		return (box.Center + Vector3::Transform(relPoint, rotMatrix));
	}

	Vector3 GeneratePointInSphere(const BoundingSphere& sphere)
	{
		// Use rejection sampling method.
		auto relPoint = Vector3::Zero;
		do
		{
			relPoint = Vector3(
				GenerateFloat(-1.0f, 1.0f),
				GenerateFloat(-1.0f, 1.0f),
				GenerateFloat(-1.0f, 1.0f));
		}
		while (relPoint.LengthSquared() > 1.0f);

		return (sphere.Center + (relPoint * sphere.Radius));
	}

	Vector3 GeneratePointOnSphere(const BoundingSphere& sphere)
	{
		float u = GenerateFloat(0.0f, 1.0f);
		float v = GenerateFloat(0.0f, 1.0f);

		float theta = u * PI_MUL_2;
		float phi = acos((v * 2) - 1.0f);

		auto relPoint = Vector3(
			sin(phi) * cos(theta),
			sin(phi) * sin(theta),
			cos(phi));
		return (sphere.Center + (relPoint * sphere.Radius));
	}

	Vector3 GeneratePointInSpheroid(const Vector3& center, const EulerAngles& orient, const Vector3& semiMajorAxis)
	{
		// Use rejection sampling method.
		auto relPoint = Vector3::Zero;
		do
		{
			relPoint = Vector3(
				Random::GenerateFloat(-semiMajorAxis.x, semiMajorAxis.x),
				Random::GenerateFloat(-semiMajorAxis.y, semiMajorAxis.y),
				Random::GenerateFloat(-semiMajorAxis.z, semiMajorAxis.z));
		}
		while ((SQUARE(relPoint.x) / SQUARE(semiMajorAxis.x) +
				SQUARE(relPoint.y) / SQUARE(semiMajorAxis.y) +
				SQUARE(relPoint.z) / SQUARE(semiMajorAxis.z)) > 1.0f);

		// Rotate relative point.
		auto rotMatrix = orient.ToRotationMatrix();
		relPoint = Vector3::Transform(relPoint, rotMatrix);

		return (center + relPoint);
	}

	bool TestProbability(float prob)
	{
		if (prob <= 0.0f)
		{
			return false;
		}
		else if (prob >= 1.0f)
		{
			return true;
		}

		return (GenerateFloat(0.0f, 1.0f) < prob);
	}
}
