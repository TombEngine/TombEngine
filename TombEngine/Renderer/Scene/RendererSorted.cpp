#include "framework.h"
#include "Renderer/Renderer.h"

#include <algorithm>
#include <unordered_map>

#include "Game/effects/hair.h"
#include "Objects/game_object_ids.h"
#include "Renderer/Scene/RenderView.h"
#include "Renderer/Structures/RendererSortableObject.h"
#include "Specific/configuration.h"
#include "Specific/level.h"

using namespace TEN::Effects::Hair;
using namespace TEN::Renderer::Structures;

// Transparent face ordering: CollectSortedBucket and CollectSortedSprite add sortable objects and per-polygon
// entries to the view, and SortTransparentFaces builds compact sort keys (draw group cluster rank, then distance)
// that the scene renderer replays to draw the sorted faces back to front.

namespace TEN::Renderer
{
	// Depth step below which two object groups count as equally distant and fall back
	// to the blend mode priority for a stable order.
	constexpr float GROUP_DEPTH_STEP = 128.0f;

	// Same-texture sprites group together only within this depth band, so a local
	// cloud batches as one unit without dragging distant sprites out of depth order.
	constexpr float SPRITE_GROUP_DEPTH_BAND = BLOCK(4);

	// Identity of a draw group collected by SortTransparentFaces: polygons with the same
	// key batch together in the sorted pass.
	struct GroupKey
	{
		const void*			Primary	  = nullptr;
		const void*			Secondary = nullptr;
		unsigned long long	Extra	  = 0;

		bool operator ==(const GroupKey& key) const
		{
			return (Primary == key.Primary && Secondary == key.Secondary && Extra == key.Extra);
		}
	};

	struct GroupKeyHash
	{
		size_t operator ()(const GroupKey& key) const
		{
			unsigned long long hash = (unsigned long long)(uintptr_t)key.Primary;
			hash ^= (unsigned long long)(uintptr_t)key.Secondary * 0x9E3779B97F4A7C15;
			hash ^= key.Extra * 0xC2B2AE3D27D4EB4F;
			hash ^= hash >> 29;
			return (size_t)hash;
		}
	};

	// Clusters of overlapping objects interleave their polygons in exact depth order. A cluster
	// that would need more draw group switches than this falls back to group order.
	constexpr int MAX_CLUSTER_GROUP_SWITCHES = 64;

	// Per-group data accumulated by SortTransparentFaces to establish the draw order. A cluster
	// is identified by the index of one of its groups; ungrouped groups form their own cluster.
	struct SortedGroup
	{
		bool  IsRoom		= false;
		bool  CanInterleave = false;
		int	  RoomSlot		= 0;
		float DistanceSum	= 0.0f;
		int	  PolygonCount	= 0;
		int	  BlendPriority = 0;
		int	  Cluster		= 0;

		int		MinDistance	 = INT_MAX;
		int		MaxDistance	 = 0;
		Vector4 ScreenBounds = Vector4(FLT_MAX, FLT_MAX, -FLT_MAX, -FLT_MAX);
	};

	static BlendMode GetSortedBlendMode(const RendererSortableObject& object)
	{
		return ((object.ObjectType == RendererObjectType::Sprite) ? object.Sprite->BlendMode : object.BlendMode);
			}

	// Identity of the draw group an object belongs to. Distance only matters for sprites, which
	// group by texture within a depth band.
	static GroupKey GetSortedGroupKey(const RendererSortableObject& object, int distance)
		{
			auto key = GroupKey{};

			switch (object.ObjectType)
			{
			case RendererObjectType::Room:
				key.Primary = object.Bucket;
				break;

			case RendererObjectType::Moveable:
			case RendererObjectType::HairPrimary:
			case RendererObjectType::HairSecondary:
				key.Primary = object.Item;
				key.Secondary = object.Bucket;
				key.Extra = (unsigned long long)object.ObjectType;
				break;

			case RendererObjectType::Static:
				key.Primary = object.Static;
				key.Secondary = object.Bucket;
				break;

			case RendererObjectType::Effect:
				key.Primary = object.Effect;
				key.Secondary = object.Bucket;
				break;

			case RendererObjectType::MoveableAsStatic:
				// No stable instance pointer exists for swarm objects: the world translation
				// tells instances of the same mesh apart (e.g. two bats sharing buckets).
				key.Primary = object.Bucket;
				key.Extra =
					((unsigned long long)(std::lround(object.World._41) & 0x1FFFFF)) |
					((unsigned long long)(std::lround(object.World._42) & 0x1FFFFF) << 21) |
					((unsigned long long)(std::lround(object.World._43) & 0x1FFFFF) << 42);
				break;

			case RendererObjectType::Sprite:
				key.Primary = object.Sprite->Sprite;
				key.Extra =
				(unsigned long long)(unsigned int)(int)(distance / SPRITE_GROUP_DEPTH_BAND) |
				((unsigned long long)object.Sprite->BlendMode << 32) |
					((unsigned long long)object.Sprite->Type << 40) |
					((unsigned long long)object.Sprite->renderType << 48) |
					((unsigned long long)(object.Sprite->SoftParticle ? 1 : 0) << 56);
				break;

			default:
			key.Primary = &object;
				key.Extra = (unsigned long long)object.ObjectType;
				break;
			}

		return key;
	}

	// Stable LSD radix sort of 64-bit keys in 8-bit digits. Digits shared by all entries are
	// skipped, so small group ranks and distances only pay for the passes their bit width needs.
	static void RadixSortKeys(std::vector<RendererSortKey>& keys, std::vector<RendererSortKey>& scratch)
	{
		constexpr auto DIGIT_COUNT	= 8;
		constexpr auto BUCKET_COUNT = 256;

		int count = (int)keys.size();
		if (count < 2)
			return;

		scratch.resize(count);

		int histograms[DIGIT_COUNT][BUCKET_COUNT] = {};
		for (const auto& entry : keys)
		{
			for (int digit = 0; digit < DIGIT_COUNT; digit++)
				histograms[digit][(entry.Key >> (digit * 8)) & 0xFF]++;
		}

		auto* source = keys.data();
		auto* dest = scratch.data();

		for (int digit = 0; digit < DIGIT_COUNT; digit++)
		{
			int shift = digit * 8;
			auto* histogram = histograms[digit];

			if (histogram[(source[0].Key >> shift) & 0xFF] == count)
				continue;

			int offset = 0;
			for (int bucket = 0; bucket < BUCKET_COUNT; bucket++)
			{
				int bucketSize = histogram[bucket];
				histogram[bucket] = offset;
				offset += bucketSize;
			}

			for (int i = 0; i < count; i++)
				dest[histogram[(source[i].Key >> shift) & 0xFF]++] = source[i];

			std::swap(source, dest);
		}

		if (source != keys.data())
			std::memcpy(keys.data(), source, sizeof(RendererSortKey) * count);
	}

	// Rooms and sprites keep their group order and never interleave with other groups.
	static bool CanInterleaveObject(const RendererSortableObject& object)
	{
		return (object.ObjectType != RendererObjectType::Room && object.ObjectType != RendererObjectType::Sprite);
	}

	static void MergeSortedBounds(SortedGroup& group, const RendererSortableObject& object)
	{
		group.MinDistance = std::min(group.MinDistance, object.MinDistance);
		group.MaxDistance = std::max(group.MaxDistance, object.MaxDistance);
		group.ScreenBounds.x = std::min(group.ScreenBounds.x, object.ScreenBounds.x);
		group.ScreenBounds.y = std::min(group.ScreenBounds.y, object.ScreenBounds.y);
		group.ScreenBounds.z = std::max(group.ScreenBounds.z, object.ScreenBounds.z);
		group.ScreenBounds.w = std::max(group.ScreenBounds.w, object.ScreenBounds.w);
	}

	// Union-find root lookup with path halving.
	static int FindSortedCluster(std::vector<int>& parents, int index)
	{
		while (parents[index] != index)
		{
			parents[index] = parents[parents[index]];
			index = parents[index];
		}

		return index;
	}

	static bool ScreenBoundsOverlap(const Vector4& bounds0, const Vector4& bounds1)
	{
		return (bounds0.x <= bounds1.z && bounds1.x <= bounds0.z &&
				bounds0.y <= bounds1.w && bounds1.y <= bounds0.w);
	}

	// Grows a normalized screen rectangle by a projected world point. Points behind the camera
	// have no meaningful projection and conservatively cover the whole screen.
	static void ExpandScreenBounds(Vector4& bounds, const Matrix& viewProjection, float x, float y, float z)
	{
		float clipX = x * viewProjection._11 + y * viewProjection._21 + z * viewProjection._31 + viewProjection._41;
		float clipY = x * viewProjection._12 + y * viewProjection._22 + z * viewProjection._32 + viewProjection._42;
		float clipW = x * viewProjection._14 + y * viewProjection._24 + z * viewProjection._34 + viewProjection._44;

		float minX = -1.0f;
		float minY = -1.0f;
		float maxX = 1.0f;
		float maxY = 1.0f;

		if (clipW > EPSILON)
		{
			minX = maxX = clipX / clipW;
			minY = maxY = clipY / clipW;
		}

		bounds.x = std::min(bounds.x, minX);
		bounds.y = std::min(bounds.y, minY);
		bounds.z = std::max(bounds.z, maxX);
		bounds.w = std::max(bounds.w, maxY);
	}

	static void ExpandScreenBoundsWithBox(Vector4& bounds, const Matrix& viewProjection, const Matrix& world, const Vector3& boxMin, const Vector3& boxMax)
	{
		for (int corner = 0; corner < 8; corner++)
		{
			auto point = Vector3((corner & 1) ? boxMax.x : boxMin.x, (corner & 2) ? boxMax.y : boxMin.y, (corner & 4) ? boxMax.z : boxMin.z);
			point = Vector3::Transform(point, world);
			ExpandScreenBounds(bounds, viewProjection, point.x, point.y, point.z);
		}
	}

	// Joins object groups that overlap both in depth and on screen: only there can polygon order
	// across objects change the image. Candidates are swept by near depth, so each group is only
	// tested against the groups that start before it ends.
	static void BuildSortedClusters(std::vector<SortedGroup>& groups)
	{
		int groupCount = (int)groups.size();

		auto parents = std::vector<int>(groupCount);
		auto candidates = std::vector<int>{};
		for (int i = 0; i < groupCount; i++)
		{
			parents[i] = i;
			if (groups[i].CanInterleave)
				candidates.push_back(i);
		}

		std::sort(
			candidates.begin(), candidates.end(),
			[&groups](int index0, int index1) { return (groups[index0].MinDistance < groups[index1].MinDistance); });

		for (int i = 0; i < candidates.size(); i++)
		{
			const auto& group = groups[candidates[i]];

			for (int j = i + 1; j < candidates.size(); j++)
			{
				const auto& other = groups[candidates[j]];
				if (other.MinDistance > group.MaxDistance)
					break;

				if (!ScreenBoundsOverlap(group.ScreenBounds, other.ScreenBounds))
					continue;

				parents[FindSortedCluster(parents, candidates[j])] = FindSortedCluster(parents, candidates[i]);
			}
		}

		for (int i = 0; i < groupCount; i++)
			groups[i].Cluster = FindSortedCluster(parents, i);
	}

	// Returns the draw order position of every cluster, indexed by cluster id. Clusters are ordered
	// with the rules previously applied to single groups, so an isolated group keeps its old place.
	static std::vector<int> RankSortedClusters(const std::vector<SortedGroup>& groups)
	{
		int groupCount = (int)groups.size();

		auto clusters = std::vector<SortedGroup>(groupCount);
		auto isCluster = std::vector<bool>(groupCount, false);
		for (const auto& group : groups)
		{
			auto& cluster = clusters[group.Cluster];
			if (!isCluster[group.Cluster])
			{
				cluster = group;
				cluster.DistanceSum = 0.0f;
				cluster.PolygonCount = 0;
				isCluster[group.Cluster] = true;
			}

			cluster.DistanceSum += group.DistanceSum;
			cluster.PolygonCount += group.PolygonCount;
			cluster.BlendPriority = std::min(cluster.BlendPriority, group.BlendPriority);
		}

		auto order = std::vector<int>{};
		for (int i = 0; i < groupCount; i++)
		{
			if (isCluster[i])
				order.push_back(i);
		}

		std::sort(
			order.begin(), order.end(),
			[&clusters](int index0, int index1)
			{
				const auto& cluster0 = clusters[index0];
				const auto& cluster1 = clusters[index1];

				// Rooms draw first, back to front by portal traversal order.
				if (cluster0.IsRoom != cluster1.IsRoom)
					return cluster0.IsRoom;

				float distance0 = cluster0.DistanceSum / cluster0.PolygonCount;
				float distance1 = cluster1.DistanceSum / cluster1.PolygonCount;

				if (cluster0.IsRoom)
				{
					if (cluster0.RoomSlot != cluster1.RoomSlot)
						return (cluster0.RoomSlot < cluster1.RoomSlot);

					return (distance0 > distance1);
				}

				// Object clusters draw back to front by mean polygon distance.
				int depth0 = (int)(distance0 / GROUP_DEPTH_STEP);
				int depth1 = (int)(distance1 / GROUP_DEPTH_STEP);
				if (depth0 != depth1)
					return (depth0 > depth1);

				if (cluster0.BlendPriority != cluster1.BlendPriority)
					return (cluster0.BlendPriority < cluster1.BlendPriority);

				return (index0 < index1);
			});

		auto ranks = std::vector<int>(groupCount, 0);
		for (int i = 0; i < order.size(); i++)
			ranks[order[i]] = i;

		return ranks;
	}

	// Keys order clusters first and distance (far to near) second, so polygons of a single-group
	// cluster stay contiguous while polygons of overlapping objects interleave by depth.
	static void BuildSortKeys(RenderView& view, const std::vector<SortedGroup>& groups, const std::vector<int>& clusterRanks)
	{
		const auto* objects = view.TransparentObjectsToDraw.data();
		int polygonCount = (int)view.TransparentPolygonsToDraw.size();
		view.TransparentSortKeys.resize(polygonCount);

		for (int i = 0; i < polygonCount; i++)
		{
			const auto& polygon = view.TransparentPolygonsToDraw[i];
			int clusterRank = clusterRanks[groups[objects[polygon.ObjectIndex].GroupIndex].Cluster];

			auto invertedDistance = (unsigned int)(INT_MAX - std::max(polygon.Distance, 0));
			view.TransparentSortKeys[i].Key = ((unsigned long long)clusterRank << 32) | invertedDistance;
			view.TransparentSortKeys[i].Index = i;
		}
	}

	// Dissolves clusters whose interleaved order would switch draw groups too often, returning
	// their groups to group order. Returns true when keys must be rebuilt.
	static bool SplitBusyClusters(const RenderView& view, std::vector<SortedGroup>& groups)
	{
		const auto* objects = view.TransparentObjectsToDraw.data();
		const auto* polygons = view.TransparentPolygonsToDraw.data();

		auto switches = std::vector<int>(groups.size(), 0);
		int lastGroup = NO_VALUE;

		for (const auto& key : view.TransparentSortKeys)
		{
			int group = objects[polygons[key.Index].ObjectIndex].GroupIndex;
			if (lastGroup != NO_VALUE && group != lastGroup && groups[group].Cluster == groups[lastGroup].Cluster)
				switches[groups[group].Cluster]++;

			lastGroup = group;
		}

		bool isSplit = false;
		for (int i = 0; i < groups.size(); i++)
		{
			if (switches[groups[i].Cluster] <= MAX_CLUSTER_GROUP_SWITCHES)
				continue;

			groups[i].Cluster = i;
			isSplit = true;
		}

		return isSplit;
	}

	// Orders the collected transparent polygons: polygons are grouped by draw group, overlapping object groups are
	// clustered, clusters are ranked, and the resulting keys are radix sorted into view.TransparentSortKeys.
	void Renderer::SortTransparentFaces(RenderView& view)
	{
		view.TransparentSortKeys.clear();

		if (view.TransparentPolygonsToDraw.empty())
			return;

		// Fixed priority between blend modes of groups at the same depth (issue #1793),
		// so their relative order is stable and predictable instead of arbitrary.
		auto getBlendPriority = [](BlendMode blendMode)
		{
			switch (blendMode)
			{
			case BlendMode::Exclude:	 return 0;
			case BlendMode::Subtractive: return 1;
			case BlendMode::Lighten:	 return 2;
			case BlendMode::Screen:		 return 3;
			case BlendMode::AlphaBlend:	 return 4;
			default:					 return 5;
			}
		};

		// Position of every room in the back-to-front draw order (portal traversal reversed).
		auto roomSlots = std::unordered_map<const RendererRoom*, int>{};
		roomSlots.reserve(view.RoomsToDraw.size());
		for (int i = 0; i < view.RoomsToDraw.size(); i++)
			roomSlots[view.RoomsToDraw[i]] = (int)view.RoomsToDraw.size() - 1 - i;

		auto groupIndices = std::unordered_map<GroupKey, int, GroupKeyHash>{};
		auto groups = std::vector<SortedGroup>{};

		// Polygons of one object are contiguous, so the group is resolved once per object and the
		// per-polygon work is reduced to accumulating its distance.
		int lastObjectIndex = NO_VALUE;
		int groupIndex = 0;

		for (const auto& polygon : view.TransparentPolygonsToDraw)
		{
			if (polygon.ObjectIndex != lastObjectIndex)
			{
				auto& object = view.TransparentObjectsToDraw[polygon.ObjectIndex];
				auto key = GetSortedGroupKey(object, polygon.Distance);

			auto [it, isNewGroup] = groupIndices.try_emplace(key, (int)groups.size());
			if (isNewGroup)
			{
				auto& group = groups.emplace_back();
				group.IsRoom = (object.ObjectType == RendererObjectType::Room);
					group.CanInterleave = CanInterleaveObject(object);
					group.BlendPriority = getBlendPriority(GetSortedBlendMode(object));

				if (group.IsRoom)
				{
					auto slot = roomSlots.find(object.Room);
					group.RoomSlot = (slot != roomSlots.end()) ? slot->second : (int)view.RoomsToDraw.size();
				}
			}

				groupIndex = it->second;
				lastObjectIndex = polygon.ObjectIndex;

				object.GroupIndex = groupIndex;
				MergeSortedBounds(groups[groupIndex], object);
			}

			auto& group = groups[groupIndex];
			group.DistanceSum += polygon.Distance;
			group.PolygonCount++;
		}

		BuildSortedClusters(groups);

		// Only compact keys are sorted; objects and polygons never move. A second pass is needed
		// only when a cluster had to be dissolved for switching draw groups too often.
		BuildSortKeys(view, groups, RankSortedClusters(groups));
		RadixSortKeys(view.TransparentSortKeys, _sortKeysScratch);

		if (SplitBusyClusters(view, groups))
		{
			BuildSortKeys(view, groups, RankSortedClusters(groups));
			RadixSortKeys(view.TransparentSortKeys, _sortKeysScratch);
		}

		// Debug statistics: groups that share their cluster with other groups.
		auto clusterSizes = std::vector<int>(groups.size(), 0);
		for (const auto& group : groups)
			clusterSizes[group.Cluster]++;

		for (const auto& group : groups)
		{
			if (clusterSizes[group.Cluster] > 1)
				_numInterleavedSortedGroups++;
		}
	}

	// Adds a bucket as a sortable object with one entry per polygon, storing each polygon's camera distance and the
	// depth range and screen bounds of the object for clustering. A null world matrix means room geometry.
	void Renderer::CollectSortedBucket(RenderView& view, const RendererSortableObject& object, const Matrix* world, const Vector3& cameraPosition)
	{
		if (object.Bucket->Polygons.empty())
			return;

		int objectIndex = (int)view.TransparentObjectsToDraw.size();
		view.TransparentObjectsToDraw.push_back(object);

		auto& polygons = object.Bucket->Polygons;
		int polygonCount = (int)polygons.size();

		int firstEntry = (int)view.TransparentPolygonsToDraw.size();
		view.TransparentPolygonsToDraw.resize(firstEntry + polygonCount);
		auto* entries = &view.TransparentPolygonsToDraw[firstEntry];

		// Room geometry is already in world space. Transforms are expanded by hand (row vector times
		// matrix, as Vector3::Transform) to keep the per-polygon cost low in debug builds as well.
		auto matrix = (world != nullptr) ? *world : Matrix::Identity;
		const auto& viewProjection = view.Camera.ViewProjection;
		auto* sourcePolygons = polygons.data();

		int minDistance = INT_MAX;
		int maxDistance = 0;
		auto screenBounds = Vector4(FLT_MAX, FLT_MAX, -FLT_MAX, -FLT_MAX);

		// Screen bounds come from the bucket box, as polygon centres miss the extent of large polygons.
		bool hasBox = (object.Bucket->BoundsMin.x <= object.Bucket->BoundsMax.x);

		for (int i = 0; i < polygonCount; i++)
		{
			const auto& centre = sourcePolygons[i].Centre;
			float worldX = centre.x * matrix._11 + centre.y * matrix._21 + centre.z * matrix._31 + matrix._41;
			float worldY = centre.x * matrix._12 + centre.y * matrix._22 + centre.z * matrix._32 + matrix._42;
			float worldZ = centre.x * matrix._13 + centre.y * matrix._23 + centre.z * matrix._33 + matrix._43;

			float deltaX = worldX - cameraPosition.x;
			float deltaY = worldY - cameraPosition.y;
			float deltaZ = worldZ - cameraPosition.z;
			int distance = (int)sqrt(deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ);

			entries[i].Polygon = &sourcePolygons[i];
			entries[i].Distance = distance;
			entries[i].ObjectIndex = objectIndex;

			minDistance = std::min(minDistance, distance);
			maxDistance = std::max(maxDistance, distance);

			if (!hasBox)
				ExpandScreenBounds(screenBounds, viewProjection, worldX, worldY, worldZ);
		}

		if (hasBox)
			ExpandScreenBoundsWithBox(screenBounds, viewProjection, matrix, object.Bucket->BoundsMin, object.Bucket->BoundsMax);

		auto& storedObject = view.TransparentObjectsToDraw[objectIndex];
		storedObject.MinDistance = minDistance;
		storedObject.MaxDistance = maxDistance;
		storedObject.ScreenBounds = screenBounds;
	}

	// Adds a sprite as a sortable object with a single entry at the given distance.
	void Renderer::CollectSortedSprite(RenderView& view, const RendererSortableObject& object, int distance)
	{
		int objectIndex = (int)view.TransparentObjectsToDraw.size();
		view.TransparentObjectsToDraw.push_back(object);

		auto& entry = view.TransparentPolygonsToDraw.emplace_back();
		entry.Distance = distance;
		entry.ObjectIndex = objectIndex;
	}

}
