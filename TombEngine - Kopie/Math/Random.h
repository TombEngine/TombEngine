#pragma once

class EulerAngles;

namespace TEN::Math::Random
{
	// Value generation

	int	  GenerateInt(int low = 0, int high = SHRT_MAX);
	float GenerateFloat(float low = 0.0f, float high = 1.0f);
	short GenerateAngle(short low = SHRT_MIN, short high = SHRT_MAX);

	// 2D geometric generation

	Vector2 GenerateDirection2D();
	Vector2 GeneratePoint2DInSquare(const Vector2& pos, short orient, float apothem);
	Vector2 GeneratePoint2DInCircle(const Vector2& pos, float radius);

	// 3D geometric generation

	Vector3 GenerateDirection();
	Vector3 GenerateDirectionInCone(const Vector3& dir, float semiangleInDeg);
	Vector3 GeneratePointInBox(const BoundingOrientedBox& box);
	Vector3 GeneratePointInSphere(const BoundingSphere& sphere);
	Vector3 GeneratePointOnSphere(const BoundingSphere& sphere);
	Vector3 GeneratePointInSpheroid(const Vector3& center, const EulerAngles& orient, const Vector3& semiMajorAxis);

	// Probability

	bool TestProbability(float prob);
}
