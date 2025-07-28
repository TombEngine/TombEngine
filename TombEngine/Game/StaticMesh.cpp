#include "framework.h"
#include "Game/StaticMesh.h"

#include "Game/Setup.h"

//namespace TEN
//{
	GameBoundingBox StaticMesh::GetVisibilityAabb() const
	{
		auto aabb = Statics[Slot].visibilityBox;
		ScaleAabb(aabb);
		return aabb;
	}

	GameBoundingBox StaticMesh::GetCollisionAabb() const
	{
		auto aabb = Statics[Slot].collisionBox;
		ScaleAabb(aabb);
		return aabb;
	}

	void StaticMesh::ScaleAabb(GameBoundingBox& aabb) const
	{
		// Calculate scaled parameters.
		auto center = aabb.GetCenter();
		auto extents = aabb.GetExtents();
		auto scaledExtents = extents * Pose.Scale;
		auto scaledOffset = (center * Pose.Scale) - center;

		// Scale AABB.
		aabb.X1 = (center.x - scaledExtents.x) + scaledOffset.x;
		aabb.X2 = (center.x + scaledExtents.x) + scaledOffset.x;
		aabb.Y1 = (center.y - scaledExtents.y) + scaledOffset.y;
		aabb.Y2 = (center.y + scaledExtents.y) + scaledOffset.y;
		aabb.Z1 = (center.z - scaledExtents.z) + scaledOffset.z;
		aabb.Z2 = (center.z + scaledExtents.z) + scaledOffset.z;
	}

	BoundingOrientedBox StaticMesh::GetObb() const
	{
		return GetBoundsAccurate(*this, false).ToBoundingOrientedBox(Pose);
	}

	BoundingOrientedBox StaticMesh::GetVisibilityObb() const
	{
		return GetBoundsAccurate(*this, true).ToBoundingOrientedBox(Pose);
	}

	GameBoundingBox& GetBoundsAccurate(const StaticMesh& staticObj, bool getVisibilityAabb)
	{
		static auto aabb = GameBoundingBox();

		aabb = getVisibilityAabb ? staticObj.GetVisibilityAabb() : staticObj.GetCollisionAabb();
		return aabb;
	}
//}
