#pragma once

// References:
// https://github.com/erincatto/box2d/blob/28adacf82377d4113f2ed00586141463244b9d10/src/dynamic_tree.c
// https://www.gdcvault.com/play/1025909/Math-for-Game-Developers-Dynamic

// NOTE: _leafIdMap is a hash map for convenience. If performance suffers with too many Move() and Remove() calls, a method with faster access can be implemented.
// However, it requires maintaining an odd index variable outside the BVH instance, so a hash map is preferred for the benefit of cleaner code. -- Sezz 2024.11.05

namespace TEN::Structures
{
	enum class BvhBuildStrategy
	{
		Fast,	  // O(n): Fast build, okay quality. Top-down approach with median split.
		Balanced, // O(n * m): Efficient build, good quality. Top-down approach with constrained surface area heuristic.
		Accurate  // O(n²): Slow build, optimal quality. Top-down approach with exhaustive surface area heuristic.
	};

	// Dynamic bounding volume hierarchy using AABBs.
	typedef class BoundingVolumeHierarchy
	{
	private:
		struct Node
		{
			int			ObjectId = NO_VALUE; // NOTE: Only stored by leaf.
			BoundingBox Aabb	 = BoundingBox();

			int Height		 = 0;
			int ParentId	 = NO_VALUE;
			int LeftChildId	 = NO_VALUE;
			int RightChildId = NO_VALUE;

			bool IsLeaf() const;
		};

		// Fields

		std::vector<Node>			 _nodes		  = {};
		std::stack<int>				 _freeNodeIds = {};
		std::unordered_map<int, int> _leafIdMap	  = {}; // Key = object ID, value = leaf ID.
		int							 _rootId	  = NO_VALUE;

	public:
		// Constructors

		BoundingVolumeHierarchy() = default;
		BoundingVolumeHierarchy(const std::vector<int>& objectIds, const std::vector<BoundingBox>& aabbs, BvhBuildStrategy strategy = BvhBuildStrategy::Balanced);

		// Getters

		unsigned int GetSize() const;

		std::vector<int> GetBoundedObjectIds() const;
		std::vector<int> GetBoundedObjectIds(const Ray& ray, float dist) const;
		std::vector<int> GetBoundedObjectIds(const BoundingBox& aabb) const;
		std::vector<int> GetBoundedObjectIds(const BoundingOrientedBox& obb) const;
		std::vector<int> GetBoundedObjectIds(const BoundingSphere& sphere) const;

		// Inquirers

		bool IsEmpty() const;

		// Utilities

		void Insert(int objectId, const BoundingBox& aabb, float boundary = 0.0f);
		void Move(int objectId, const BoundingBox& aabb, float boundary = 0.0f);
		void Remove(int objectId);

		// Debug

		void DrawDebug() const;

	private:
		// Collision helpers

		std::vector<int> GetBoundedObjectIds(const std::function<bool(const Node& node)>& testCollRoutine) const;

		// Dynamic helpers

		int GetNewNodeId();
		int GetBestSiblingLeafId(int leafId) const;

		void InsertLeaf(int leafId);
		void RemoveLeaf(int leafId);
		void RefitNode(int nodeId);
		void RemoveNode(int nodeId);
		int	 BalanceNode(int nodeId);

		// Static helpers

		void Build(const std::vector<int>& objectIds, const std::vector<BoundingBox>& aabbs, BvhBuildStrategy strategy);
		int	 Build(const std::vector<int>& objectIds, const std::vector<BoundingBox>& aabbs, int start, int end, BvhBuildStrategy strategy);

		// Debug helpers

		void Validate() const;
		void Validate(int nodeId) const;
	} Bvh;
}
