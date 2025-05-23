#pragma once

#include "Math/Math.h"

using namespace TEN::Math;

namespace TEN::Structures
{
	class SpatialHash
	{
	private:
		struct Cell
		{
			std::set<int> ObjectIds = {};
			BoundingBox	  Aabb		= BoundingBox();

			Cell(const BoundingBox& aabb);
		};

		// Fields

		std::unordered_map<Vector3i, Cell> _cellMap = {}; // Key = position, value = cell.

		float	_cellSize		 = 0.0f;
		Vector3 _cellAabbExtents = Vector3::Zero;

	public:
		// Constructors

		SpatialHash(float cellSize);

		// Getters

		unsigned int GetSize() const;

		std::set<int> GetBoundedObjectIds() const;
		std::set<int> GetBoundedObjectIds(const Vector3& pos) const;
		std::set<int> GetBoundedObjectIds(const Ray& ray, float dist) const;
		std::set<int> GetBoundedObjectIds(const BoundingBox& aabb) const;
		std::set<int> GetBoundedObjectIds(const BoundingOrientedBox& obb) const;
		std::set<int> GetBoundedObjectIds(const BoundingSphere& sphere) const;

		// Inquirers

		bool IsEmpty() const;

		// Utilities

		void Insert(int objectId, const BoundingBox& aabb);
		void Insert(int objectId, const BoundingOrientedBox& obb);
		void Insert(int objectId, const BoundingSphere& sphere);
		void Move(int objectId, const BoundingBox& aabb, const BoundingBox& prevAabb);
		void Move(int objectId, const BoundingOrientedBox& obb, BoundingOrientedBox& prevObb);
		void Move(int objectId, const BoundingSphere& sphere, const BoundingSphere& prevSphere);
		void Remove(int objectId, const BoundingBox& prevAabb);
		void Remove(int objectId, const BoundingOrientedBox& prevObb);
		void Remove(int objectId, const BoundingSphere& prevSphere);

		// Debug

		void DrawDebug() const;

	private:
		// Getter helpers

		Vector3i			  GetCellKey(const Vector3& pos) const;
		std::vector<Vector3i> GetCellKeys(const Ray& ray, float dist) const;
		std::vector<Vector3i> GetCellKeys(const BoundingBox& aabb) const;
		std::vector<Vector3i> GetCellKeys(const BoundingOrientedBox& obb) const;
		std::vector<Vector3i> GetCellKeys(const BoundingSphere& sphere) const;

		// Utility helpers

		void Insert(int objectId, const std::vector<Vector3i>& keys);
		void Remove(int objectId, const std::vector<Vector3i>& keys);
	};
}
