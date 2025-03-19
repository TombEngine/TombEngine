#include "framework.h"
#include "Math/Objects/GameBoundingBox.h"

#include "Game/Animation/Animation.h"
#include "Game/items.h"
#include "Game/Setup.h"
#include "Math/Objects/EulerAngles.h"
#include "Math/Objects/Pose.h"
#include "Objects/game_object_ids.h"

using namespace TEN::Animation;

//namespace TEN::Math
//{
	const GameBoundingBox GameBoundingBox::Zero = GameBoundingBox(0, 0, 0, 0, 0, 0);

	GameBoundingBox::GameBoundingBox(float x1, float x2, float y1, float y2, float z1, float z2)
	{
		X1 = (int)round(x1);
		X2 = (int)round(x2);
		Y1 = (int)round(y1);
		Y2 = (int)round(y2);
		Z1 = (int)round(z1);
		Z2 = (int)round(z2);
	}

	GameBoundingBox::GameBoundingBox(const BoundingBox& aabb)
	{
		X1 = aabb.Center.x - aabb.Extents.x;
		X2 = aabb.Center.x + aabb.Extents.x;
		Y1 = aabb.Center.y - aabb.Extents.y;
		Y2 = aabb.Center.y + aabb.Extents.y;
		Z1 = aabb.Center.z - aabb.Extents.z;
		Z2 = aabb.Center.z + aabb.Extents.z;
	}

	GameBoundingBox::GameBoundingBox(GAME_OBJECT_ID objectID, int animNumber, int frameNumber)
	{
		*this = GetKeyframe(objectID, animNumber, frameNumber).BoundingBox;
	}

	// TODO: Use reference, not pointer.
	GameBoundingBox::GameBoundingBox(const ItemInfo* item)
	{
		auto frameData = GetFrameInterpData(*item);
		if (frameData.Alpha == 0.0f)
		{
			*this = frameData.Keyframe0.BoundingBox;
		}
		else
		{
			*this = frameData.Keyframe0.BoundingBox + (((frameData.Keyframe1.BoundingBox - frameData.Keyframe0.BoundingBox) * frameData.Alpha));
		}
	}

	int GameBoundingBox::GetWidth() const
	{
		return abs(X2 - X1);
	}

	int GameBoundingBox::GetHeight() const
	{
		return abs(Y2 - Y1);
	}

	int GameBoundingBox::GetDepth() const
	{
		return abs(Z2 - Z1);
	}

	Vector3 GameBoundingBox::GetCenter() const
	{
		return ((Vector3(X1, Y1, Z1) + Vector3(X2, Y2, Z2)) / 2);
	}

	Vector3 GameBoundingBox::GetExtents() const
	{
		return ((Vector3(X2, Y2, Z2) - Vector3(X1, Y1, Z1)) / 2);
	}

	void GameBoundingBox::Rotate(const EulerAngles& rot)
	{
		// Get box min and max values.
		auto boxMax = Vector3(X2, Y2, Z2);
		auto boxMin = Vector3(X1, Y1, Z1);

		// Rotate min and max values.
		auto rotMatrix = rot.ToRotationMatrix();
		boxMax = Vector3::Transform(boxMax, rotMatrix);
		boxMin = Vector3::Transform(boxMin, rotMatrix);

		X1 = (int)round(boxMin.x);
		X2 = (int)round(boxMax.x);
		Y1 = (int)round(boxMin.y);
		Y2 = (int)round(boxMax.y);
		Z1 = (int)round(boxMin.z);
		Z2 = (int)round(boxMax.z);
	}

	BoundingSphere GameBoundingBox::ToLocalBoundingSphere() const
	{
		return BoundingSphere(GetCenter(), GetExtents().Length());
	}

	BoundingBox GameBoundingBox::ToConservativeBoundingBox(const Pose& pose) const
	{
		// Calculate conservative radius in XZ plane for Y-axis rotation.
		auto extents = GetExtents();
		float xzRadius = sqrt(extents.x * extents.x + extents.z * extents.z);

		// Extents remain unchanged along Y-axis.
		auto conservativeExtents = Vector3(xzRadius, extents.y, xzRadius);

		// Manual 2D rotation for Y-axis only.
		auto center = GetCenter();
		float cos = phd_cos(pose.Orientation.y);
		float sin = phd_sin(pose.Orientation.y);

		// Rotate centerOffset in XZ plane (ignore Y rotation for height).
		float rotatedX =  (cos * center.x) + (sin * center.z);
		float rotatedZ = -(sin * center.x) + (cos * center.z);

		// Box center is now position plus rotated offset.
		auto rotatedCenter = pose.Position.ToVector3() + Vector3(rotatedX, center.y, rotatedZ);

		// Build and return conservative AABB.
		return BoundingBox(rotatedCenter, conservativeExtents);
	}

	BoundingOrientedBox GameBoundingBox::ToBoundingOrientedBox(const Pose& pose) const
	{
		return ToBoundingOrientedBox(pose.Position.ToVector3(), pose.Orientation.ToQuaternion());
	}

	BoundingOrientedBox GameBoundingBox::ToBoundingOrientedBox(const Vector3& pos, const Quaternion& orient) const
	{
		auto box = BoundingOrientedBox();
		BoundingOrientedBox(GetCenter(), GetExtents(), Vector4::UnitY).Transform(box, 1.0f, orient, pos);
		return box;
	}

	GameBoundingBox GameBoundingBox::operator +(const GameBoundingBox& bounds) const
	{
		return GameBoundingBox(
			X1 + bounds.X1, X2 + bounds.X2,
			Y1 + bounds.Y1, Y2 + bounds.Y2,
			Z1 + bounds.Z1, Z2 + bounds.Z2);
	}

	GameBoundingBox GameBoundingBox::operator +(const Pose& pose) const
	{
		return GameBoundingBox(
			X1 + pose.Position.x, X2 + pose.Position.x,
			Y1 + pose.Position.y, Y2 + pose.Position.y,
			Z1 + pose.Position.z, Z2 + pose.Position.z);
	}

	GameBoundingBox GameBoundingBox::operator -(const GameBoundingBox& bounds) const
	{
		return GameBoundingBox(
			X1 - bounds.X1, X2 - bounds.X2,
			Y1 - bounds.Y1, Y2 - bounds.Y2,
			Z1 - bounds.Z1, Z2 - bounds.Z2);
	}

	GameBoundingBox GameBoundingBox::operator -(const Pose& pose) const
	{
		return GameBoundingBox(
			X1 - pose.Position.x, X2 - pose.Position.x,
			Y1 - pose.Position.y, Y2 - pose.Position.y,
			Z1 - pose.Position.z, Z2 - pose.Position.z);
	}

	GameBoundingBox GameBoundingBox::operator *(float scalar) const
	{
		return GameBoundingBox(
			X1 * scalar, X2 * scalar,
			Y1 * scalar, Y2 * scalar,
			Z1 * scalar, Z2 * scalar);
	}

	GameBoundingBox GameBoundingBox::operator /(float scalar) const
	{
		return GameBoundingBox(
			X1 / scalar, X2 / scalar,
			Y1 / scalar, Y2 / scalar,
			Z1 / scalar, Z2 / scalar);
	}
//}
