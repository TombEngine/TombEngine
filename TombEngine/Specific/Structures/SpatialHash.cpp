#include "framework.h"
#include "Specific/Structures/SpatialHash.h"

#include "Math/Math.h"

using namespace TEN::Math;

namespace TEN::Structures
{
	SpatialHash::Cell::Cell(const BoundingBox& aabb)
	{
		Aabb = aabb;
	}

	SpatialHash::SpatialHash(float cellSize)
	{
		_cellSize = cellSize;
		_cellAabbExtents = Vector3(cellSize / 2);
	}

	unsigned int SpatialHash::GetSize() const
	{
		return (unsigned int)_cellMap.size();
	}

	std::set<int> SpatialHash::GetBoundedObjectIds() const
	{
		auto objectIds = std::set<int>{};

		// Collect object IDs from all cells.
		for (const auto& [key, cell] : _cellMap)
			objectIds.insert(cell.ObjectIds.begin(), cell.ObjectIds.end());

		return objectIds;
	}

	std::set<int> SpatialHash::GetBoundedObjectIds(const Vector3& pos) const
	{
		// No cells; return early.
		if (_cellMap.empty())
			return {};

		// Get position's cell key.
		auto key = GetCellKey(pos);

		// Check if cell exists.
		auto it = _cellMap.find(key);
		if (it == _cellMap.end())
			return {};

		// Return object IDs from cell.
		const auto& [keyPos, cell] = *it;
		return cell.ObjectIds;
	}

	std::set<int> SpatialHash::GetBoundedObjectIds(const Ray& ray, float dist) const
	{
		auto objectIds = std::set<int>{};

		// No cells; return early.
		if (_cellMap.empty())
			return objectIds;

		// Collect object IDs from cells intersecting ray.
		auto keys = GetCellKeys(ray, dist);
		for (const auto& key : keys)
		{
			// Check if cell exists.
			auto it = _cellMap.find(key);
			if (it == _cellMap.end())
				continue;

			// Collect object IDs from cell.
			const auto& [keyPos, cell] = *it;
			objectIds.insert(cell.ObjectIds.begin(), cell.ObjectIds.end());
		}

		return objectIds;
	}

	std::set<int> SpatialHash::GetBoundedObjectIds(const BoundingBox& aabb) const
	{
		auto objectIds = std::set<int>{};

		// No cells; return early.
		if (_cellMap.empty())
			return objectIds;

		// Collect object IDs from cells intersecting AABB.
		auto keys = GetCellKeys(aabb);
		for (const auto& key : keys)
		{
			// Check if cell exists.
			auto it = _cellMap.find(key);
			if (it == _cellMap.end())
				continue;

			// Collect object IDs from cell.
			const auto& [keyPos, cell] = *it;
			objectIds.insert(cell.ObjectIds.begin(), cell.ObjectIds.end());
		}

		return objectIds;
	}

	std::set<int> SpatialHash::GetBoundedObjectIds(const BoundingOrientedBox& obb) const
	{
		auto objectIds = std::set<int>{};

		// No cells; return early.
		if (_cellMap.empty())
			return objectIds;

		// Collect object IDs from cells intersecting OBB.
		auto keys = GetCellKeys(obb);
		for (const auto& key : keys)
		{
			// Check if cell exists.
			auto it = _cellMap.find(key);
			if (it == _cellMap.end())
				continue;

			// Collect object IDs from cell.
			const auto& [keyPos, cell] = *it;
			objectIds.insert(cell.ObjectIds.begin(), cell.ObjectIds.end());
		}

		return objectIds;
	}

	std::set<int> SpatialHash::GetBoundedObjectIds(const BoundingSphere& sphere) const
	{
		auto objectIds = std::set<int>{};

		// No cells; return early.
		if (_cellMap.empty())
			return objectIds;

		// Collect object IDs from cells intersecting sphere.
		auto keys = GetCellKeys(sphere);
		for (const auto& key : keys)
		{
			// Check if cell exists.
			auto it = _cellMap.find(key);
			if (it == _cellMap.end())
				continue;

			// Collect object IDs from cell.
			const auto& [keyPos, cell] = *it;
			objectIds.insert(cell.ObjectIds.begin(), cell.ObjectIds.end());
		}

		return objectIds;
	}

	bool SpatialHash::IsEmpty() const
	{
		return _cellMap.empty();
	}

	void SpatialHash::Insert(int objectId, const BoundingBox& aabb)
	{
		// Insert object ID into cells intersecting AABB.
		auto keys = GetCellKeys(aabb);
		Insert(objectId, keys);
	}

	void SpatialHash::Insert(int objectId, const BoundingOrientedBox& obb)
	{
		// Insert object ID into cells intersecting OBB.
		auto keys = GetCellKeys(obb);
		Insert(objectId, keys);
	}

	void SpatialHash::Insert(int objectId, const BoundingSphere& sphere)
	{
		// Insert object ID into cells intersecting sphere.
		auto keys = GetCellKeys(sphere);
		Insert(objectId, keys);
	}

	void SpatialHash::Move(int objectId, const BoundingBox& aabb, const BoundingBox& prevAabb)
	{
		Remove(objectId, prevAabb);
		Insert(objectId, aabb);
	}

	void SpatialHash::Move(int objectId, const BoundingOrientedBox& obb, BoundingOrientedBox& prevObb)
	{
		Remove(objectId, prevObb);
		Insert(objectId, obb);
	}

	void SpatialHash::Move(int objectId, const BoundingSphere& sphere, const BoundingSphere& prevSphere)
	{
		Remove(objectId, prevSphere);
		Insert(objectId, sphere);
	}

	void SpatialHash::Remove(int objectId, const BoundingBox& prevAabb)
	{
		// Remove object ID from cells intersecting previous AABB.
		auto keys = GetCellKeys(prevAabb);
		Remove(objectId, keys);
	}

	void SpatialHash::Remove(int objectId, const BoundingOrientedBox& prevObb)
	{
		// Remove object ID from cells intersecting previous OBB.
		auto keys = GetCellKeys(prevObb);
		Remove(objectId, keys);
	}

	void SpatialHash::Remove(int objectId, const BoundingSphere& prevSphere)
	{
		// Remove object ID from cells intersecting previous sphere.
		auto keys = GetCellKeys(prevSphere);
		Remove(objectId, keys);
	}

	void SpatialHash::DrawDebug() const
	{
		constexpr auto BOX_COLOR = Color(1.0f, 1.0f, 1.0f, 0.5f);

		PrintDebugMessage("SPATIAL HASH DEBUG");

		PrintDebugMessage("Cells: %d", _cellMap.size());
		for (const auto& [key, cell] : _cellMap)
			DrawDebugBox(cell.Aabb, BOX_COLOR);
	}

	Vector3i SpatialHash::GetCellKey(const Vector3& pos) const
	{
		// Calculate and return key.
		return Vector3i(
			RoundToStep(pos.x, _cellSize) + (_cellSize / 2),
			RoundToStep(pos.y, _cellSize) + (_cellSize / 2),
			RoundToStep(pos.z, _cellSize) + (_cellSize / 2));
	}

	std::vector<Vector3i> SpatialHash::GetCellKeys(const Ray& ray, float dist) const
	{
		// Reserve minimum key vector.
		auto keys = std::vector<Vector3i>{};
		keys.reserve(int(dist / _cellSize) + 1);

		// Calculate cell position.
		auto pos = Vector3(
			floor(ray.position.x / _cellSize) * _cellSize,
			floor(ray.position.y / _cellSize) * _cellSize,
			floor(ray.position.z / _cellSize) * _cellSize);

		// Calculate cell position step.
		auto posStep = Vector3(
			(ray.direction.x > 0) ? _cellSize : -_cellSize,
			(ray.direction.y > 0) ? _cellSize : -_cellSize,
			(ray.direction.z > 0) ? _cellSize : -_cellSize);

		// Calculate next intersection.
		auto nextIntersect = Vector3(
			((pos.x + ((posStep.x > 0) ? _cellSize : 0)) - ray.position.x) / ray.direction.x,
			((pos.y + ((posStep.y > 0) ? _cellSize : 0)) - ray.position.y) / ray.direction.y,
			((pos.z + ((posStep.z > 0) ? _cellSize : 0)) - ray.position.z) / ray.direction.z);

		// Calculate ray step.
		auto rayStep = Vector3(
			_cellSize / abs(ray.direction.x),
			_cellSize / abs(ray.direction.y),
			_cellSize / abs(ray.direction.z));

		// Traverse cells and collect keys.
		float currentDist = 0.0f;
		while (currentDist <= dist)
		{
			auto key = GetCellKey(pos);
			keys.push_back(key);

			// Determine which axis to step along.
			if (nextIntersect.x < nextIntersect.y)
			{
				if (nextIntersect.x < nextIntersect.z)
				{
					pos.x += posStep.x;
					currentDist = nextIntersect.x;
					nextIntersect.x += rayStep.x;
				}
				else
				{
					pos.z += posStep.z;
					currentDist = nextIntersect.z;
					nextIntersect.z += rayStep.z;
				}
			}
			else
			{
				if (nextIntersect.y < nextIntersect.z)
				{
					pos.y += posStep.y;
					currentDist = nextIntersect.y;
					nextIntersect.y += rayStep.y;
				}
				else
				{
					pos.z += posStep.z;
					currentDist = nextIntersect.z;
					nextIntersect.z += rayStep.z;
				}
			}
		}

		return keys;
	}

	std::vector<Vector3i> SpatialHash::GetCellKeys(const BoundingBox& aabb) const
	{
		// Calculate cell bounds.
		auto minCell = Vector3(
			FloorToStep(aabb.Center.x - aabb.Extents.x, _cellSize),
			FloorToStep(aabb.Center.y - aabb.Extents.y, _cellSize),
			FloorToStep(aabb.Center.z - aabb.Extents.z, _cellSize));
		auto maxCell = Vector3(
			FloorToStep(aabb.Center.x + aabb.Extents.x, _cellSize),
			FloorToStep(aabb.Center.y + aabb.Extents.y, _cellSize),
			FloorToStep(aabb.Center.z + aabb.Extents.z, _cellSize));

		// Reserve key vector.
		auto keys = std::vector<Vector3i>{};
		keys.reserve(
			(((maxCell.x - minCell.x) / _cellSize) + 1) *
			(((maxCell.y - minCell.y) / _cellSize) + 1) *
			(((maxCell.z - minCell.z) / _cellSize) + 1));

		// Collect keys of cells intersecting AABB.
		for (float x = minCell.x; x <= maxCell.x; x += _cellSize)
		{
			for (float y = minCell.y; y <= maxCell.y; y += _cellSize)
			{
				for (float z = minCell.z; z <= maxCell.z; z += _cellSize)
				{
					auto pos = Vector3(x, y, z);
					keys.push_back(GetCellKey(pos));
				}
			}
		}

		return keys;
	}

	std::vector<Vector3i> SpatialHash::GetCellKeys(const BoundingOrientedBox& obb) const
	{
		auto aabb = Geometry::GetBoundingBox(obb);

		// Collect keys of cells intersecting OBB.
		auto keys = GetCellKeys(aabb);
		for (auto it = keys.begin(); it != keys.end();)
		{
			const auto& key = *it;

			// Remove keys of cells not intersecting OBB.
			auto cellAabb = BoundingBox(key.ToVector3(), _cellAabbExtents);
			if (!obb.Intersects(cellAabb))
			{
				it = keys.erase(it);
				continue;
			}

			it++;
		}

		return keys;
	}

	std::vector<Vector3i> SpatialHash::GetCellKeys(const BoundingSphere& sphere) const
	{
		auto aabb = BoundingBox(sphere.Center, Vector3(sphere.Radius));

		// Collect keys of cells intersecting sphere.
		auto keys = GetCellKeys(aabb);
		for (auto it = keys.begin(); it != keys.end();)
		{
			const auto& key = *it;

			// Remove keys of cells not intersecting OBB.
			auto cellAabb = BoundingBox(key.ToVector3(), _cellAabbExtents);
			if (!sphere.Intersects(cellAabb))
			{
				it = keys.erase(it);
				continue;
			}

			it++;
		}

		return keys;
	}

	void SpatialHash::Insert(int objectId, const std::vector<Vector3i>& keys)
	{
		// Insert object ID into cells at keys.
		for (auto& key : keys)
		{
			// Get existing cell or insert new cell.
			auto [it, isInserted] = _cellMap.try_emplace(key, Cell(BoundingBox(key.ToVector3(), _cellAabbExtents)));

			// Insert object ID into cell.
			auto& [keyPos, cell] = *it;
			cell.ObjectIds.insert(objectId);
		}
	}

	void SpatialHash::Remove(int objectId, const std::vector<Vector3i>& keys)
	{
		// Remove object ID from cells at keys.
		for (const auto& key : keys)
		{
			// Check if cell exists.
			auto it = _cellMap.find(key);
			if (it == _cellMap.end())
				continue;

			// Remove object ID from cell.
			auto& [keyPos, cell] = *it;
			cell.ObjectIds.erase(objectId);

			// Remove cell if empty.
			if (cell.ObjectIds.empty())
				_cellMap.erase(it);
		}
	}
}
