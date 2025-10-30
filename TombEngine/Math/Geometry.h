#pragma once

class AxisAngle;
class EulerAngles;
class Pose;
class Vector3i;

namespace TEN::Math::Geometry
{
	// Float-based 2D point translation

	Vector2 TranslatePoint(const Vector2& point, short orient, const Vector2& relOffset);
	Vector2 TranslatePoint(const Vector2& point, short orient, float dist);
	Vector2 TranslatePoint(const Vector2& point, const Vector2& dir, float dist);

	// Integer-based 3D point translation

	Vector3i TranslatePoint(const Vector3i& point, short headingAngle, float forward, float down = 0.0f, float right = 0.0f, const Vector3& axis = Vector3::UnitY);
	Vector3i TranslatePoint(const Vector3i& point, short headingAngle, const Vector3i& relOffset, const Vector3& axis = Vector3::UnitY);
	Vector3i TranslatePoint(const Vector3i& point, const EulerAngles& orient, const Vector3i& relOffset);
	Vector3i TranslatePoint(const Vector3i& point, const EulerAngles& orient, float dist);
	Vector3i TranslatePoint(const Vector3i& point, const AxisAngle& orient, float dist);
	Vector3i TranslatePoint(const Vector3i& point, const Vector3& dir, float dist);

	// Float-based 3D point translation

	Vector3 TranslatePoint(const Vector3& point, short headingAngle, float forward, float down = 0.0f, float right = 0.0f, const Vector3& axis = Vector3::UnitY);
	Vector3 TranslatePoint(const Vector3& point, short headingAngle, const Vector3& relOffset, const Vector3& axis = Vector3::UnitY);
	Vector3 TranslatePoint(const Vector3& point, const EulerAngles& orient, const Vector3& relOffset);
	Vector3 TranslatePoint(const Vector3& point, const EulerAngles& orient, float dist);
	Vector3 TranslatePoint(const Vector3& point, const AxisAngle& orient, float dist);
	Vector3 TranslatePoint(const Vector3& point, const Vector3& dir, float dist);

	// Rotation

	Vector3 RotatePoint(const Vector3& point, const EulerAngles& rot);
	Vector3 RotatePoint(const Vector3& point, const AxisAngle& rot);

	// Angle getters

	short GetShortestAngle(short fromAngle, short toAngle);
	short GetSurfaceSlopeAngle(const Vector3& normal, const Vector3& axis = Vector3::UnitY);
	short GetSurfaceAspectAngle(const Vector3& normal, const Vector3& axis = Vector3::UnitY);

	// Line getters

	float	GetDistanceToLine(const Vector3& origin, const Vector3& linePoint0, const Vector3& linePoint1);
	Vector3 GetClosestPointOnLine(const Vector3& origin, const Vector3& linePoint0, const Vector3& linePoint1);
	Vector3 GetClosestPointOnLinePerp(const Vector3& origin, const Vector3& linePoint0, const Vector3& linePoint1, const Vector3& axis = Vector3::UnitY);

	// Box getters

	BoundingBox GetBoundingBox(const BoundingOrientedBox& box);
	BoundingBox GetBoundingBox(const std::vector<Vector3>& points);
	float		GetBoundingBoxArea(const BoundingBox& box);

	// Misc. getters

	EulerAngles GetOrientToPoint(const Vector3& origin, const Vector3& target);
	EulerAngles GetRelOrientToNormal(short orient, const Vector3& normal, const Vector3& axis = Vector3::UnitY);

	// Converters

	Quaternion ConvertDirectionToQuat(const Vector3& dir);
	Vector3	   ConvertQuatToDirection(const Quaternion& quat);

	// Point relation inquirers

	bool IsPointInFront(const Pose& pose, const Vector3& target);
	bool IsPointInFront(const Vector3& origin, const Vector3& target, const EulerAngles& orient);
	bool IsPointInFront(const Vector3& origin, const Vector3& target, const Vector3& refPoint);
	bool IsPointOnLeft(const Pose& pose, const Vector3& target);
	bool IsPointOnLeft(const Vector3& origin, const Vector3& target, const EulerAngles& orient);
	bool IsPointOnLeft(const Vector3& origin, const Vector3& target, const Vector3& refPoint);
	bool IsPointInBox(const Vector3& point, const BoundingBox& box);
	bool IsPointInBox(const Vector3& point, const BoundingOrientedBox& box);
	bool IsPointInSphere(const Vector3& point, const BoundingSphere& sphere);

	// Intersection inquirers

	bool CircleIntersects(const Vector3& circle0, const Vector3& circle1);
}
