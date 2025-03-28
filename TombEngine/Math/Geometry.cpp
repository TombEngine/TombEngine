#include "framework.h"
#include "Math/Geometry.h"

#include "Math/Constants.h"
#include "Math/Legacy.h"
#include "Math/Objects/AxisAngle.h"
#include "Math/Objects/EulerAngles.h"
#include "Math/Objects/Pose.h"
#include "Math/Objects/Vector3i.h"

namespace TEN::Math::Geometry
{
	Vector2 TranslatePoint(const Vector2& point, short orient, const Vector2& relOffset)
	{
		float sinOrient = sin(TO_RAD(orient));
		float cosOrient = cos(TO_RAD(orient));

		auto offset = Vector2(
			(cosOrient * relOffset.x) - (sinOrient * relOffset.y),
			(sinOrient * relOffset.x) + (cosOrient * relOffset.y));

		return (point + offset);
	}

	Vector2 TranslatePoint(const Vector2& point, short orient, float dist)
	{
		auto dir = Vector2(
			cos(TO_RAD(orient)),
			sin(TO_RAD(orient)));

		return TranslatePoint(point, dir, dist);
	}

	Vector2 TranslatePoint(const Vector2& point, const Vector2& dir, float dist)
	{
		auto dirNorm = dir;
		dirNorm.Normalize();

		return (point + (dirNorm * dist));
	}

	Vector3i TranslatePoint(const Vector3i& point, short headingAngle, float forward, float down, float right, const Vector3& axis)
	{
		return Vector3i(TranslatePoint(point.ToVector3(), headingAngle, forward, down, right, axis));
	}

	Vector3i TranslatePoint(const Vector3i& point, short headingAngle, const Vector3i& relOffset, const Vector3& axis)
	{
		return Vector3i(TranslatePoint(point.ToVector3(), headingAngle, relOffset.ToVector3(), axis));
	}

	Vector3i TranslatePoint(const Vector3i& point, const EulerAngles& orient, const Vector3i& relOffset)
	{
		return Vector3i(TranslatePoint(point.ToVector3(), orient, relOffset.ToVector3()));
	}

	Vector3i TranslatePoint(const Vector3i& point, const EulerAngles& orient, float dist)
	{
		return Vector3i(TranslatePoint(point.ToVector3(), orient, dist));
	}

	Vector3i TranslatePoint(const Vector3i& point, const AxisAngle& orient, float dist)
	{
		return Vector3i(TranslatePoint(point.ToVector3(), orient, dist));
	}

	Vector3i TranslatePoint(const Vector3i& point, const Vector3& dir, float dist)
	{
		return Vector3i(TranslatePoint(point.ToVector3(), dir, dist));
	}

	Vector3 TranslatePoint(const Vector3& point, short headingAngle, float forward, float down, float right, const Vector3& axis)
	{
		if (forward == 0.0f && down == 0.0f && right == 0.0f)
			return point;

		auto relOffset = Vector3(right, down, forward);
		return TranslatePoint(point, headingAngle, relOffset, axis);
	}

	Vector3 TranslatePoint(const Vector3& point, short headingAngle, const Vector3& relOffset, const Vector3& axis)
	{
		auto orient = AxisAngle(axis, headingAngle);
		auto rotMatrix = orient.ToRotationMatrix();
		return (point + Vector3::Transform(relOffset, rotMatrix));
	}

	Vector3 TranslatePoint(const Vector3& point, const EulerAngles& orient, const Vector3& relOffset)
	{
		auto rotMatrix = orient.ToRotationMatrix();
		return (point + Vector3::Transform(relOffset, rotMatrix));
	}

	// NOTE: Roll (Z axis) of EulerAngles orientation is disregarded.
	Vector3 TranslatePoint(const Vector3& point, const EulerAngles& orient, float dist)
	{
		if (dist == 0.0f)
			return point;

		auto dir = orient.ToDirection();
		return TranslatePoint(point, dir, dist);
	}

	Vector3 TranslatePoint(const Vector3& point, const AxisAngle& orient, float dist)
	{
		auto dir = orient.ToDirection();
		return TranslatePoint(point, dir, dist);
	}

	Vector3 TranslatePoint(const Vector3& point, const Vector3& dir, float dist)
	{
		if (dist == 0.0f)
			return point;

		// Ensure direction is normalized.
		auto dirNorm = dir;
		dirNorm.Normalize();

		return (point + (dirNorm * dist));
	}

	Vector3 RotatePoint(const Vector3& point, const EulerAngles& rot)
	{
		auto rotMatrix = rot.ToRotationMatrix();
		return Vector3::Transform(point, rotMatrix);
	}

	Vector3 RotatePoint(const Vector3& point, const AxisAngle& rot)
	{
		auto rotMatrix = rot.ToRotationMatrix();
		return Vector3::Transform(point, rotMatrix);
	}

	short GetShortestAngle(short fromAngle, short toAngle)
	{
		if (fromAngle == toAngle)
			return 0;

		return short(toAngle - fromAngle);
	}

	short GetSurfaceSlopeAngle(const Vector3& normal, const Vector3& axis)
	{
		if (normal == -axis)
			return 0;

		return FROM_RAD(acos(normal.Dot(-axis)));
	}

	short GetSurfaceAspectAngle(const Vector3& normal, const Vector3& axis)
	{
		if (normal == -axis)
			return 0;

		// TODO: Consider axis.
		return FROM_RAD(atan2(normal.x, normal.z));
	}

	BoundingBox GetBoundingBox(const BoundingOrientedBox& box)
	{
		// Get corners.
		auto corners = std::array<Vector3, BoundingOrientedBox::CORNER_COUNT>{};
		box.GetCorners(corners.data());

		// Transfer corners to vector.
		auto cornersVector = std::vector<Vector3>{};
		cornersVector.insert(cornersVector.end(), corners.begin(), corners.end());

		// Return bounding box.
		return Geometry::GetBoundingBox(cornersVector);
	}

	BoundingBox GetBoundingBox(const std::vector<Vector3>& points)
	{
		auto minPoint = Vector3(INFINITY);
		auto maxPoint = Vector3(-INFINITY);

		// Determine max and min AABB points.
		for (const auto& point : points)
		{
			minPoint = Vector3(
				std::min(minPoint.x, point.x),
				std::min(minPoint.y, point.y),
				std::min(minPoint.z, point.z));

			maxPoint = Vector3(
				std::max(maxPoint.x, point.x),
				std::max(maxPoint.y, point.y),
				std::max(maxPoint.z, point.z));
		}

		// Construct and return AABB.
		auto center = (minPoint + maxPoint) / 2;
		auto extents = (maxPoint - minPoint) / 2;
		return BoundingBox(center, extents);
	}

	float GetBoundingBoxArea(const BoundingBox& box)
	{
		float width = box.Extents.x * 2;
		float height = box.Extents.y * 2;
		float depth = box.Extents.z * 2;

		return (((width * height) + (width * depth) + (height * depth)) * 2);
	}

	float GetDistanceToLine(const Vector3& origin, const Vector3& linePoint0, const Vector3& linePoint1)
	{
		auto target = GetClosestPointOnLine(origin, linePoint0, linePoint1);
		return Vector3::Distance(origin, target);
	}

	Vector3 GetClosestPointOnLine(const Vector3& origin, const Vector3& linePoint0, const Vector3& linePoint1)
	{
		if (linePoint0 == linePoint1)
			return linePoint0;

		auto line = linePoint1 - linePoint0;
		float alpha = line.Dot(origin - linePoint0) / line.Dot(line);

		// Closest point out of line range; return start or end point.
		if (alpha <= 0.0f)
		{
			return linePoint0;
		}
		else if (alpha >= 1.0f)
		{
			return linePoint1;
		}

		// Return closest point on line.
		return (linePoint0 + (line * alpha));
	}

	Vector3 GetClosestPointOnLinePerp(const Vector3& origin, const Vector3& linePoint0, const Vector3& linePoint1, const Vector3& axis)
	{
		if (linePoint0 == linePoint1)
			return linePoint0;

		// Ensure axis is normalized.
		auto axisNorm = axis;
		axisNorm.Normalize();

		// Calculate line.
		auto line = linePoint1 - linePoint0;

		// Project line and origin onto plane perpendicular to input axis.
		auto linePerp = line - (axisNorm * line.Dot(axisNorm));
		auto originPerp = origin - (axisNorm * origin.Dot(axisNorm));

		// Calculate alpha from line projection.
		float alpha = linePerp.Dot(originPerp - linePoint0) / linePerp.Dot(linePerp);

		// Closest point out of line range; return start or end point.
		if (alpha <= 0.0f)
		{
			return linePoint0;
		}
		else if (alpha >= 1.0f)
		{
			return linePoint1;
		}

		// Return closest point on line perpendicular to input axis.
		return (linePoint0 + (line * alpha));
	}

	EulerAngles GetOrientToPoint(const Vector3& origin, const Vector3& target)
	{
		if (origin == target)
			return EulerAngles::Identity;

		return EulerAngles(target - origin);
	}

	EulerAngles GetRelOrientToNormal(short orient, const Vector3& normal, const Vector3& gravity)
	{
		// TODO: Consider axis.

		// Determine relative angle properties of normal.
		short aspectAngle = Geometry::GetSurfaceAspectAngle(normal);
		short slopeAngle = Geometry::GetSurfaceSlopeAngle(normal);

		short deltaAngle = Geometry::GetShortestAngle(orient, aspectAngle);
		float sinDeltaAngle = phd_sin(deltaAngle);
		float cosDeltaAngle = phd_cos(deltaAngle);

		// Calculate relative orientation adhering to normal.
		return EulerAngles(
			-slopeAngle * cosDeltaAngle,
			orient,
			slopeAngle * sinDeltaAngle);
	}

	Quaternion ConvertDirectionToQuat(const Vector3& dir)
	{
		constexpr auto SINGULARITY_THRESHOLD = 1.0f - EPSILON;

		static const auto REF_DIR = Vector3::UnitZ;

		// Vectors are nearly opposite; return orientation 180 degrees around arbitrary axis.
		float dot = REF_DIR.Dot(dir);
		if (dot < -SINGULARITY_THRESHOLD)
		{
			auto axis = Vector3::UnitX.Cross(REF_DIR);
			if (axis.LengthSquared() < EPSILON)
				axis = Vector3::UnitY.Cross(REF_DIR);
			axis.Normalize();

			auto axisAngle = AxisAngle(axis, FROM_RAD(PI));
			return axisAngle.ToQuaternion();
		}

		// Vectors are nearly identical; return identity quaternion.
		if (dot > SINGULARITY_THRESHOLD)
			return Quaternion::Identity;

		// Calculate axis-angle and return converted quaternion.
		auto axisAngle = AxisAngle(REF_DIR.Cross(dir), FROM_RAD(acos(dot)));
		return axisAngle.ToQuaternion();
	}

	Vector3 ConvertQuatToDirection(const Quaternion& quat)
	{
		return Vector3::Transform(Vector3::UnitZ, quat);
	}

	bool IsPointInFront(const Pose& pose, const Vector3& target)
	{
		return IsPointInFront(pose.Position.ToVector3(), target, pose.Orientation);
	}

	bool IsPointInFront(const Vector3& origin, const Vector3& target, const EulerAngles& orient)
	{
		if (origin == target)
			return true;

		float sinY = phd_sin(orient.y);
		float cosY = phd_cos(orient.y);

		// 2D heading direction vector: X = +sinY, Y = 0, Z = +cosY
		auto headingDir = Vector3(sinY, 0.0f, cosY);
		auto targetDir = target - origin;

		float dot = headingDir.Dot(targetDir);
		return (dot > 0.0f);
	}

	bool IsPointInFront(const Vector3& origin, const Vector3& target, const Vector3& refPoint)
	{
		if (origin == target)
			return true;

		auto refDir = refPoint - origin;

		// 2D heading direction vector to 3D reference direction vector: X = +refDir.x, Y = 0, Z = +refDir.z
		auto headingDir = Vector3(refDir.x, 0.0f, refDir.z);
		auto targetDir = target - origin;

		float dot = headingDir.Dot(targetDir);
		return (dot > 0.0f);
	}

	bool IsPointOnLeft(const Pose& pose, const Vector3& target)
	{
		return IsPointOnLeft(pose.Position.ToVector3(), target, pose.Orientation);
	}

	bool IsPointOnLeft(const Vector3& origin, const Vector3& target, const EulerAngles& orient)
	{
		if (origin == target)
			return true;

		float sinY = phd_sin(orient.y);
		float cosY = phd_cos(orient.y);

		// 2D normal vector to 2D heading direction vector: X = -cosY, Y = 0, Z = +sinY
		auto headingNormal = Vector3(-cosY, 0.0f, sinY);
		auto targetDir = target - origin;

		float dot = headingNormal.Dot(targetDir);
		return (dot > 0.0f);
	}

	bool IsPointOnLeft(const Vector3& origin, const Vector3& target, const Vector3& refPoint)
	{
		if (origin == target)
			return true;

		auto refDir = refPoint - origin;

		// 2D normal vector to 3D reference direction vector: X = +refDir.z, Y = 0, Z = -refDir.x
		auto headingNormal = Vector3(refDir.z, 0.0f, -refDir.x);
		auto targetDir = target - origin;

		float dot = headingNormal.Dot(targetDir);
		return (dot > 0.0f);
	}

	bool IsPointInBox(const Vector3& point, const BoundingBox& box)
	{
		// Calculate box-relative point.
		auto relPoint = box.Center - point;

		// Test if point intersects box.
		if (relPoint.x >= -box.Extents.x && relPoint.x <= box.Extents.x &&
			relPoint.y >= -box.Extents.y && relPoint.y <= box.Extents.y &&
			relPoint.z >= -box.Extents.z && relPoint.z <= box.Extents.z)
		{
			return true;
		}

		return false;
	}

	bool IsPointInBox(const Vector3& point, const BoundingOrientedBox& box)
	{
		// Calculate box-relative point.
		auto invRotMatrix = Matrix::CreateFromQuaternion(box.Orientation).Invert();
		auto relPoint = Vector3::Transform(box.Center - point, invRotMatrix);

		// Test if point intersects box.
		if (relPoint.x >= -box.Extents.x && relPoint.x <= box.Extents.x &&
			relPoint.y >= -box.Extents.y && relPoint.y <= box.Extents.y &&
			relPoint.z >= -box.Extents.z && relPoint.z <= box.Extents.z)
		{
			return true;
		}

		return false;
	}

	bool IsPointInSphere(const Vector3& point, const BoundingSphere& sphere)
	{
		float distSqr = Vector3::DistanceSquared(point, sphere.Center);
		float radiusSqr = SQUARE(sphere.Radius);

		return (distSqr <= radiusSqr);
	}

	bool CircleIntersects(const Vector3& circle0, const Vector3& circle1)
	{
		return (sqrt(SQUARE(circle1.x - circle0.x) + SQUARE(circle1.y - circle0.y)) <= (circle0.z + circle1.z));
	}
}
