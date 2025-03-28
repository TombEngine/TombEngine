#include "framework.h"
#include "Specific/Structures/BoundingVolumeHierarchy.h"

#include "Math/Math.h"

using namespace TEN::Math;

namespace TEN::Structures
{
	bool BoundingVolumeHierarchy::Node::IsLeaf() const
	{
		return (LeftChildId == NO_VALUE && RightChildId == NO_VALUE);
	}

	BoundingVolumeHierarchy::BoundingVolumeHierarchy(const std::vector<int>& objectIds, const std::vector<BoundingBox>& aabbs, BvhBuildStrategy strategy)
	{
		TENAssert(objectIds.size() == aabbs.size(), "BVH: Object ID and AABB counts unequal in static constructor.");
		if (objectIds.empty() && aabbs.empty())
			return;

		Build(objectIds, aabbs, strategy);
	}

	unsigned int BoundingVolumeHierarchy::GetSize() const
	{
		return (unsigned int)_leafIdMap.size();
	}

	std::vector<int> BoundingVolumeHierarchy::GetBoundedObjectIds() const
	{
		auto objectIds = std::vector<int>{};
		if (_leafIdMap.empty())
			return objectIds;

		// Collect all object IDs.
		objectIds.reserve(_leafIdMap.size());
		for (const auto& [keyObjectId, leafId] : _leafIdMap)
			objectIds.push_back(keyObjectId);

		return objectIds;
	}

	std::vector<int> BoundingVolumeHierarchy::GetBoundedObjectIds(const Ray& ray, float dist) const
	{
		auto testColl = [&](const Node& node)
		{
			float intersectDist = 0.0f;
			return (node.Aabb.Intersects(ray.position, ray.direction, intersectDist) && intersectDist <= dist);
		};

		return GetBoundedObjectIds(testColl);
	}

	std::vector<int> BoundingVolumeHierarchy::GetBoundedObjectIds(const BoundingBox& aabb) const
	{
		auto testColl = [&](const Node& node)
		{
			return node.Aabb.Intersects(aabb);
		};

		return GetBoundedObjectIds(testColl);
	}

	std::vector<int> BoundingVolumeHierarchy::GetBoundedObjectIds(const BoundingOrientedBox& obb) const
	{
		auto testColl = [&](const Node& node)
		{
			return node.Aabb.Intersects(obb);
		};

		return GetBoundedObjectIds(testColl);
	}

	std::vector<int> BoundingVolumeHierarchy::GetBoundedObjectIds(const BoundingSphere& sphere) const
	{
		auto testColl = [&](const Node& node)
		{
			return node.Aabb.Intersects(sphere);
		};

		return GetBoundedObjectIds(testColl);
	}

	bool BoundingVolumeHierarchy::IsEmpty() const
	{
		return _leafIdMap.empty();
	}

	void BoundingVolumeHierarchy::Insert(int objectId, const BoundingBox& aabb, float boundary)
	{
		// FAILSAFE: Find leaf containing object ID.
		auto it = _leafIdMap.find(objectId);
		if (it != _leafIdMap.end())
		{
			TENLog("BVH: Attempted to insert leaf with existing object ID " + std::to_string(objectId) + ".", LogLevel::Warning, LogConfig::All, true);
			return;
		}

		// Allocate new leaf.
		int leafId = GetNewNodeId();
		auto& leaf = _nodes[leafId];

		// Set initial parameters.
		leaf.ObjectId = objectId;
		leaf.Aabb = BoundingBox(aabb.Center, aabb.Extents + Vector3(boundary));
		leaf.Height = 0;

		// Insert new leaf.
		InsertLeaf(leafId);
	}

	void BoundingVolumeHierarchy::Move(int objectId, const BoundingBox& aabb, float boundary)
	{
		// Find leaf containing object ID.
		auto it = _leafIdMap.find(objectId);
		if (it == _leafIdMap.end())
		{
			TENLog("BVH: Attempted to move missing leaf with object ID " + std::to_string(objectId) + ".", LogLevel::Warning, LogConfig::All, true);
			return;
		}

		// Get leaf.
		const auto& [keyObjectId, leafId] = *it;
		auto& leaf = _nodes[leafId];

		// Test if object AABB is inside node AABB.
		if (leaf.Aabb.Contains(aabb) == ContainmentType::CONTAINS)
		{
			auto deltaExtents = leaf.Aabb.Extents - aabb.Extents;
			float threshold = boundary * 2;

			// Test if object AABB is significantly smaller than node AABB.
			if (deltaExtents.x < threshold &&
				deltaExtents.y < threshold &&
				deltaExtents.z < threshold)
			{
				return;
			}
		}

		// Reinsert leaf.
		RemoveLeaf(leafId);
		Insert(objectId, aabb, boundary);
	}

	void BoundingVolumeHierarchy::Remove(int objectId)
	{
		// Find leaf containing object ID.
		auto it = _leafIdMap.find(objectId);
		if (it == _leafIdMap.end())
		{
			TENLog("BVH: Attempted to remove missing leaf with object ID " + std::to_string(objectId) + ".", LogLevel::Warning, LogConfig::All, true);
			return;
		}

		// Remove leaf.
		const auto& [keyObjectId, leafId] = *it;
		RemoveLeaf(leafId);
	}

	void BoundingVolumeHierarchy::DrawDebug() const
	{
		constexpr auto BOX_COLOR = Color(1.0f, 1.0f, 1.0f, 0.5f);

		//PrintDebugMessage("BOUNDING TREE DEBUG");

		if (!_nodes.empty())
		{
			//PrintDebugMessage("Nodes: %d", (int)_nodes.size());
			//PrintDebugMessage("Root height: %d", _nodes[_rootId].Height);
		}

		for (const auto& node : _nodes)
			DrawDebugBox(node.Aabb, BOX_COLOR);
	}

	std::vector<int> BoundingVolumeHierarchy::GetBoundedObjectIds(const std::function<bool(const Node& node)>& testCollRoutine) const
	{
		auto objectIds = std::vector<int>{};
		if (_nodes.empty())
			return objectIds;

		// Traverse tree.
		auto nodeIds = std::stack<int>{};
		nodeIds.push(_rootId);
		while (!nodeIds.empty())
		{
			int nodeId = nodeIds.top();
			nodeIds.pop();

			// Invalid node; continue.
			if (nodeId == NO_VALUE)
				continue;

			const auto& node = _nodes[nodeId];

			// Test node collision.
			if (!testCollRoutine(node))
				continue;

			// Leaf node; collect object ID.
			if (node.IsLeaf())
			{
				objectIds.push_back(node.ObjectId);
			}
			// Inner node; push children onto stack for traversal.
			else
			{
				if (node.LeftChildId != NO_VALUE)
					nodeIds.push(node.LeftChildId);

				if (node.RightChildId != NO_VALUE)
					nodeIds.push(node.RightChildId);
			}
		}

		return objectIds;
	}

	int BoundingVolumeHierarchy::GetNewNodeId()
	{
		int nodeId = 0;

		// Allocate and get new empty node ID.
		if (_freeNodeIds.empty())
		{
			_nodes.emplace_back();
			nodeId = (int)_nodes.size() - 1;
		}
		// Get existing empty node ID.
		else
		{
			nodeId = _freeNodeIds.top();
			_freeNodeIds.pop();
		}

		return nodeId;
	}

	int BoundingVolumeHierarchy::GetBestSiblingLeafId(int leafId) const
	{
		const auto& leaf = _nodes[leafId];

		// Branch and bound for best sibling leaf.
		int siblingId = _rootId;
		while (!_nodes[siblingId].IsLeaf())
		{
			const auto& sibling = _nodes[siblingId];
			int leftChildId = sibling.LeftChildId;
			int rightChildId = sibling.RightChildId;

			float area = Geometry::GetBoundingBoxArea(sibling.Aabb);
			float inheritCost = Geometry::GetBoundingBoxArea(leaf.Aabb) * 2;

			// Calculate cost of creating new parent for sibling and new leaf.
			auto mergedAabb = BoundingBox();
			BoundingBox::CreateMerged(mergedAabb, sibling.Aabb, leaf.Aabb);
			float mergedArea = Geometry::GetBoundingBoxArea(mergedAabb);
			float cost = mergedArea * 2;

			// Calculate cost of descending into left child.
			float leftCost = INFINITY;
			if (leftChildId != NO_VALUE)
			{
				const auto& leftChild = _nodes[leftChildId];
				auto aabb = BoundingBox();
				BoundingBox::CreateMerged(aabb, leftChild.Aabb, leaf.Aabb);
				float newArea = Geometry::GetBoundingBoxArea(aabb);

				leftCost = leftChild.IsLeaf() ?
					newArea + inheritCost :
					(newArea - Geometry::GetBoundingBoxArea(leftChild.Aabb)) + inheritCost;
			}

			// Calculate cost of descending into right child.
			float rightCost = INFINITY;
			if (rightChildId != NO_VALUE)
			{
				const auto& rightChild = _nodes[rightChildId];
				auto aabb = BoundingBox();
				BoundingBox::CreateMerged(aabb, rightChild.Aabb, leaf.Aabb);
				float newArea = Geometry::GetBoundingBoxArea(aabb);

				rightCost = rightChild.IsLeaf() ?
					newArea + inheritCost :
					(newArea - Geometry::GetBoundingBoxArea(rightChild.Aabb)) + inheritCost;
			}

			// Test if descent is worthwhile according to minimum cost.
			if (cost < leftCost && cost < rightCost)
				break;

			// Descend.
			siblingId = (leftCost < rightCost) ? leftChildId : rightChildId;
			if (siblingId == NO_VALUE)
			{
				TENLog("BVH: Sibling leaf search failed.", LogLevel::Warning);
				break;
			}
		}

		return siblingId;
	}

	void BoundingVolumeHierarchy::InsertLeaf(int leafId)
	{
		// Create root if empty.
		if (_rootId == NO_VALUE)
		{
			auto& leaf = _nodes[leafId];

			_leafIdMap.insert({ leaf.ObjectId, leafId });
			_rootId = leafId;
			return;
		}

		// Allocate new parent.
		int parentId = GetNewNodeId();
		auto& parent = _nodes[parentId];

		// Get sibling leaf and new leaf.
		int siblingId = GetBestSiblingLeafId(leafId);
		auto& sibling = _nodes[siblingId];
		auto& leaf = _nodes[leafId];

		// Calculate merged AABB of sibling leaf and new leaf.
		auto aabb = BoundingBox();
		BoundingBox::CreateMerged(aabb, sibling.Aabb, leaf.Aabb);

		// Get previous parent.
		int prevParentId = sibling.ParentId;

		// Update nodes.
		parent.Aabb = aabb;
		parent.Height = sibling.Height + 1;
		parent.ParentId = prevParentId;
		parent.LeftChildId = siblingId;
		parent.RightChildId = leafId;
		sibling.ParentId = parentId;
		leaf.ParentId = parentId;

		if (prevParentId == NO_VALUE)
		{
			_rootId = parentId;
		}
		else
		{
			auto& prevParent = _nodes[prevParentId];

			// Update previous parent's child reference.
			if (prevParent.LeftChildId == siblingId)
			{
				prevParent.LeftChildId = parentId;
			}
			else
			{
				prevParent.RightChildId = parentId;
			}
		}

		// Refit.
		RefitNode(leafId);

		// Store object-leaf association.
		_leafIdMap.insert({ leaf.ObjectId, leafId });

		//Validate(leafId);
	}

	void BoundingVolumeHierarchy::RemoveLeaf(int leafId)
	{
		int nodeId = leafId;
		int parentId = _nodes[nodeId].ParentId;

		// Remove node.
		RemoveNode(nodeId);

		// Prune branch up to root.
		while (parentId != NO_VALUE)
		{
			auto& parent = _nodes[parentId];

			// Check if parent becomes new leaf.
			int siblingId = (parent.LeftChildId == nodeId) ? parent.RightChildId : parent.LeftChildId;
			auto& sibling = _nodes[siblingId];

			// Rearrange nodes local to removal.
			if (parent.LeftChildId == nodeId || parent.RightChildId == nodeId)
			{
				// Replace parent with sibling.
				if (parent.ParentId != NO_VALUE)
				{
					auto& grandparent = _nodes[parent.ParentId];
					if (grandparent.LeftChildId == parentId)
					{
						grandparent.LeftChildId = siblingId;
					}
					else
					{
						grandparent.RightChildId = siblingId;
					}

					sibling.ParentId = parent.ParentId;
				}
				else
				{
					// No grandparent; sibling becomes root.
					_rootId = siblingId;
					sibling.ParentId = NO_VALUE;
				}

				// Refit sibling (new parent).
				RefitNode(siblingId);

				// Remove previous parent.
				RemoveNode(parentId);
				parentId = sibling.ParentId;
			}
			// Refit up hierarchy.
			else
			{
				// Refit parent.
				RefitNode(parentId);
				parentId = NO_VALUE;
			}
		}
	}

	void BoundingVolumeHierarchy::RefitNode(int nodeId)
	{
		const auto& node = _nodes[nodeId];

		// Retread tree branch to refit AABBs.
		int parentId = node.ParentId;
		while (parentId != NO_VALUE)
		{
			// Balance node and get new subtree root.
			int newParentId = BalanceNode(parentId);
			auto& parent = _nodes[newParentId];

			if (parent.LeftChildId != NO_VALUE && parent.RightChildId != NO_VALUE)
			{
				const auto& leftChild = _nodes[parent.LeftChildId];
				const auto& rightChild = _nodes[parent.RightChildId];

				BoundingBox::CreateMerged(parent.Aabb, leftChild.Aabb, rightChild.Aabb);
				parent.Height = std::max(leftChild.Height, rightChild.Height) + 1;
			}
			else if (parent.LeftChildId != NO_VALUE)
			{
				const auto& leftChild = _nodes[parent.LeftChildId];

				parent.Aabb = leftChild.Aabb;
				parent.Height = leftChild.Height + 1;
			}
			else if (parent.RightChildId != NO_VALUE)
			{
				const auto& rightChild = _nodes[parent.RightChildId];

				parent.Aabb = rightChild.Aabb;
				parent.Height = rightChild.Height + 1;
			}

			int prevParentId = parentId;
			parentId = parent.ParentId;
		}
	}

	void BoundingVolumeHierarchy::RemoveNode(int nodeId)
	{
		auto& node = _nodes[nodeId];

		// Remove leaf from map.
		if (node.IsLeaf())
			_leafIdMap.erase(node.ObjectId);

		// Clear node and mark free.
		node = {};
		_freeNodeIds.push(nodeId);

		// Shrink capacity if empty to avoid memory bloat.
		if (_nodes.size() == _freeNodeIds.size())
			*this = {};
	}

	// Performs left or right tree rotation if input node is imbalanced.
	// Returns new subtree root ID.
	int BoundingVolumeHierarchy::BalanceNode(int nodeId)
	{
		if (nodeId == NO_VALUE)
			return nodeId;

		auto& nodeA = _nodes[nodeId];
		if (nodeA.IsLeaf() || nodeA.Height < 2)
			return nodeId;

		int nodeIdB = nodeA.LeftChildId;
		int nodeIdC = nodeA.RightChildId;
		if (nodeIdB == NO_VALUE || nodeIdC == NO_VALUE)
			return nodeId;

		auto& nodeB = _nodes[nodeIdB];
		auto& nodeC = _nodes[nodeIdC];

		// Calculate balance.
		int balance = nodeC.Height - nodeB.Height;

		// Rotate C up.
		if (balance > 1)
		{
			int nodeIdF = nodeC.LeftChildId;
			int nodeIdG = nodeC.RightChildId;
			if (nodeIdF == NO_VALUE || nodeIdG == NO_VALUE)
				return nodeId;

			auto& nodeF = _nodes[nodeIdF];
			auto& nodeG = _nodes[nodeIdG];

			// Swap A and C.
			nodeC.ParentId = nodeA.ParentId;
			nodeC.LeftChildId = nodeId;
			nodeA.ParentId = nodeIdC;

			// Make A's previous parent point to C.
			if (nodeC.ParentId != NO_VALUE)
			{
				auto& parent = _nodes[nodeC.ParentId];
				if (parent.LeftChildId == nodeId)
				{
					parent.LeftChildId = nodeIdC;
				}
				else
				{
					parent.RightChildId = nodeIdC;
				}
			}
			else
			{
				_rootId = nodeIdC;
			}

			// Rotate.
			if (nodeF.Height > nodeG.Height)
			{
				BoundingBox::CreateMerged(nodeA.Aabb, nodeB.Aabb, nodeG.Aabb);
				BoundingBox::CreateMerged(nodeC.Aabb, nodeA.Aabb, nodeF.Aabb);
				nodeA.Height = std::max(nodeB.Height, nodeG.Height) + 1;
				nodeC.Height = std::max(nodeA.Height, nodeF.Height) + 1;

				nodeG.ParentId = nodeId;
				nodeC.RightChildId = nodeIdF;
				nodeA.RightChildId = nodeIdG;
			}
			else
			{
				BoundingBox::CreateMerged(nodeA.Aabb, nodeB.Aabb, nodeF.Aabb);
				BoundingBox::CreateMerged(nodeC.Aabb, nodeA.Aabb, nodeG.Aabb);
				nodeA.Height = std::max(nodeB.Height, nodeF.Height) + 1;
				nodeC.Height = std::max(nodeA.Height, nodeG.Height) + 1;

				nodeF.ParentId = nodeId;
				nodeC.RightChildId = nodeIdG;
				nodeA.RightChildId = nodeIdF;
			}

			return nodeIdC;
		}
		// Rotate B up.
		else if (balance < -1)
		{
			int nodeIdD = nodeB.LeftChildId;
			int nodeIdE = nodeB.RightChildId;
			if (nodeIdD == NO_VALUE || nodeIdE == NO_VALUE)
				return nodeId;

			auto& nodeD = _nodes[nodeIdD];
			auto& nodeE = _nodes[nodeIdE];

			// Swap A and B.
			nodeB.ParentId = nodeA.ParentId;
			nodeB.LeftChildId = nodeId;
			nodeA.ParentId = nodeIdB;

			// Make A's previous parent point to B.
			if (nodeB.ParentId != NO_VALUE)
			{
				auto& parent = _nodes[nodeB.ParentId];
				if (parent.LeftChildId == nodeId)
				{
					parent.LeftChildId = nodeIdB;
				}
				else
				{
					parent.RightChildId = nodeIdB;
				}
			}
			else
			{
				_rootId = nodeIdB;
			}

			// Rotate.
			if (nodeD.Height > nodeE.Height)
			{
				BoundingBox::CreateMerged(nodeA.Aabb, nodeC.Aabb, nodeE.Aabb);
				BoundingBox::CreateMerged(nodeB.Aabb, nodeA.Aabb, nodeD.Aabb);
				nodeA.Height = std::max(nodeC.Height, nodeE.Height) + 1;
				nodeB.Height = std::max(nodeA.Height, nodeD.Height) + 1;

				nodeB.RightChildId = nodeIdD;
				nodeA.LeftChildId = nodeIdE;
				nodeE.ParentId = nodeId;
			}
			else
			{
				BoundingBox::CreateMerged(nodeA.Aabb, nodeC.Aabb, nodeD.Aabb);
				BoundingBox::CreateMerged(nodeB.Aabb, nodeA.Aabb, nodeE.Aabb);
				nodeA.Height = std::max(nodeC.Height, nodeD.Height) + 1;
				nodeB.Height = std::max(nodeA.Height, nodeE.Height) + 1;

				nodeB.RightChildId = nodeIdE;
				nodeA.LeftChildId = nodeIdD;
				nodeD.ParentId = nodeId;
			}

			return nodeIdB;
		}

		return nodeId;
	}

	void BoundingVolumeHierarchy::Build(const std::vector<int>& objectIds, const std::vector<BoundingBox>& aabbs, BvhBuildStrategy strategy)
	{
		// Reserve enough memory for optimally balanced tree.
		_nodes.reserve((objectIds.size() * 2) - 1);

		// Build tree recursively.
		Build(objectIds, aabbs, 0, (int)objectIds.size(), strategy);
		_rootId = (int)_nodes.size() - 1;

		//Validate();
	}

	int BoundingVolumeHierarchy::Build(const std::vector<int>& objectIds, const std::vector<BoundingBox>& aabbs, int start, int end, BvhBuildStrategy strategy)
	{
		constexpr auto BALANCED_STRAT_SPLIT_RANGE_MAX = 10;

		// FAILSAFE.
		if (start >= end)
			return NO_VALUE;

		// Create new node.
		auto node = Node{};

		// Combine AABBs.
		node.Aabb = aabbs[start];
		for (int i = (start + 1); i < end; i++)
			BoundingBox::CreateMerged(node.Aabb, node.Aabb, aabbs[i]);

		// Leaf node.
		if ((end - start) == 1)
		{
			int leafId = (int)_nodes.size();

			node.ObjectId = objectIds[start];
			node.Height = 0;

			// Add new leaf.
			_nodes.push_back(node);
			_leafIdMap.insert({ node.ObjectId, leafId });
			return leafId;
		}
		// Inner node.
		else
		{
			auto getBestSplit = [&]()
			{
				int bestSplit = (start + end) / 2;

				// Fast strategy: median split.
				if (strategy == BvhBuildStrategy::Fast)
					return bestSplit;

				float bestCost = INFINITY;
				int range = (strategy == BvhBuildStrategy::Balanced) ? BALANCED_STRAT_SPLIT_RANGE_MAX : (end - start);

				// Balanced or accurate strategy: surface area heuristic.
				for (int split = std::max(start + 1, bestSplit - range); split < std::min(end, bestSplit + range); split++)
				{
					// Calculate AABB 0.
					auto aabb0 = aabbs[start];
					for (int i = (start + 1); i < split; i++)
						BoundingBox::CreateMerged(aabb0, aabb0, aabbs[i]);

					// Calculate AABB 1.
					auto aabb1 = aabbs[split];
					for (int i = split; i < end; i++)
						BoundingBox::CreateMerged(aabb1, aabb1, aabbs[i]);

					// Calculate cost.
					float surfaceArea0 = Geometry::GetBoundingBoxArea(aabb0);
					float surfaceArea1 = Geometry::GetBoundingBoxArea(aabb1);
					float cost = (surfaceArea0 * (split - start)) + (surfaceArea1 * (end - split));

					// Track best split.
					if (cost < bestCost)
					{
						bestSplit = split;
						bestCost = cost;
					}
				}

				return bestSplit;
			};

			int bestSplit = getBestSplit();

			// Create children recursively.
			node.LeftChildId = Build(objectIds, aabbs, start, bestSplit, strategy);
			node.RightChildId = Build(objectIds, aabbs, bestSplit, end, strategy);

			// Set parent ID for children.
			int nodeId = (int)_nodes.size();
			if (node.LeftChildId != NO_VALUE)
				_nodes[node.LeftChildId].ParentId = nodeId;
			if (node.RightChildId != NO_VALUE)
				_nodes[node.RightChildId].ParentId = nodeId;

			// Set height.
			node.Height = std::max(
				(node.LeftChildId != NO_VALUE) ? _nodes[node.LeftChildId].Height : 0, 
				(node.RightChildId != NO_VALUE) ? _nodes[node.RightChildId].Height : 0) + 1;

			// Add new inner node.
			_nodes.push_back(node);
			return nodeId;
		}
	}

	void BoundingVolumeHierarchy::Validate() const
	{
		Validate(_rootId);

		// Validate unique object IDs.
		auto objectIds = GetBoundedObjectIds();
		for (int refObjectId : objectIds)
		{
			unsigned int count = 0;
			for (int objectId : objectIds)
			{
				if (refObjectId == objectId)
					count++;
			}

			TENAssert(count == 1, "BVH: Duplicate object IDs contained.");
		}
	}

	void BoundingVolumeHierarchy::Validate(int nodeId) const
	{
		if (nodeId == NO_VALUE)
			return;

		// Get node.
		const auto& node = _nodes[nodeId];

		// Validate root.
		if (nodeId == _rootId)
			TENAssert(node.ParentId == NO_VALUE, "BVH: Root node cannot have parent.");

		// Validate leaf node.
		if (node.IsLeaf())
		{
			TENAssert(node.ObjectId != NO_VALUE, "BVH: Leaf node must contain object ID.");
			TENAssert(node.Height == 0, "BVH: Leaf node must have height of 0.");
		}
		// Validate inner node.
		else
		{
			TENAssert(node.ObjectId == NO_VALUE, "BVH: Inner node cannot contain object ID.");
			TENAssert(node.Height != 0, "BVH: Inner node cannot have height of 0.");
		}

		// Validate parent.
		if (nodeId != _rootId)
			TENAssert(node.ParentId != NO_VALUE, "BVH: Non-root node must have parent.");

		// Validate parent of children.
		if (node.LeftChildId != NO_VALUE)
		{
			const auto& leftChild = _nodes[node.LeftChildId];
			TENAssert(leftChild.ParentId == nodeId, "BVH: Left child has wrong parent.");
		}
		if (node.RightChildId != NO_VALUE)
		{
			const auto& rightChild = _nodes[node.RightChildId];
			TENAssert(rightChild.ParentId == nodeId, "BVH: Right child has wrong parent.");
		}

		// Validate height.
		if (nodeId != _rootId)
		{
			const auto& parent = _nodes[node.ParentId];
			TENAssert(node.Height < parent.Height, "BVH: Child height must be less than parent height.");
		}

		// Validate recursively.
		Validate(node.LeftChildId);
		Validate(node.RightChildId);
	}
}
