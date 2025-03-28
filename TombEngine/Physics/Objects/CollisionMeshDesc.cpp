#include "framework.h"
#include "Physics/Objects/CollisionMeshDesc.h"

#include "Math/Math.h"
#include "Specific/trutils.h"

using namespace TEN::Math;
using namespace TEN::Utils;

namespace TEN::Physics
{
	unsigned int CollisionMeshDesc::GetTriangleCount() const
	{
		return ((unsigned int)_vertexIds.size() / VERTEX_COUNT);
	}

	const std::vector<Vector3>& CollisionMeshDesc::GetVertices() const
	{
		return _vertices;
	}

	const std::vector<int>& CollisionMeshDesc::GetIds() const
	{
		return _vertexIds;
	}

	void CollisionMeshDesc::InsertTriangle(const Vector3& vertex0, const Vector3& vertex1, const Vector3& vertex2)
	{
		auto getVertexId = [&](const Vector3& vertex)
		{
			// Get existing vertex ID.
			auto it = _vertexMap.find(vertex);
			if (it != _vertexMap.end())
			{
				const auto& [vertex, vertexId] = *it;
				return vertexId;
			}

			// Add and cache new vertex.
			int vertexId = (int)_vertices.size();
			_vertices.push_back(vertex);
			_vertexMap[vertex] = vertexId;

			// Get new vertex ID.
			return vertexId;
		};

		// Allocate vertices.
		int vertexId0 = getVertexId(vertex0);
		int vertexId1 = getVertexId(vertex1);
		int vertexId2 = getVertexId(vertex2);

		// Add vertex IDs.
		_vertexIds.push_back(vertexId0);
		_vertexIds.push_back(vertexId1);
		_vertexIds.push_back(vertexId2);
	}

	void CollisionMeshDesc::Optimize()
	{
		// TODO: Optimise later. -- Sezz 2025.03.25
		return;

		// 1) Get coplanar triangle map.
		auto coplanarTriMap = GetCoplanarTriangleMap();

		// 2) Process coplanar triangles into optimized vertex IDs.
		auto optimizedVertexIds = std::vector<int>{};
		for (const auto& [plane, tris] : coplanarTriMap)
		{
			// Get optimal polygons from coplanar triangles.
			auto polygons = GetPolygons(tris);

			// Triangulate polygons.
			for (const auto& polygon : polygons)
				TriangulatePolygon(optimizedVertexIds, polygon, plane.Normal());
		}

		auto optimizedVertices = std::vector<Vector3>{};
		optimizedVertices.reserve(optimizedVertexIds.size());
		
		auto reoptimizedVertexIds = std::vector<int>{};
		reoptimizedVertexIds.reserve(optimizedVertexIds.size());

		// 3) Finalize optimized vertices and vertex IDs.
		for (int vertexId : optimizedVertexIds)
		{
			const auto& vertex = _vertices[vertexId];

			optimizedVertices.push_back(vertex);
			reoptimizedVertexIds.push_back((int)optimizedVertices.size() - 1);
		}

		// 4) Store optimized vertices and vertex IDs.
		_vertices = std::move(optimizedVertices);
		_vertexIds = std::move(reoptimizedVertexIds);
	}

	std::vector<std::vector<int>> CollisionMeshDesc::GetPolygons(const std::vector<TriangleVertexIds>& tris) const
	{
		// 1) Get key mesh collections.
		auto edgeCountMap = GetEdgeCountMap(tris);
		auto boundaryEdges = GetBoundaryEdges(edgeCountMap);
		auto vertexAdjacencyMap = GetVertexAdjacencyMap(tris, edgeCountMap, boundaryEdges); // TODO: Not used yet.

		auto polygons = std::vector<std::vector<int>>{};

		// 2) Get raw polygon loops.
		auto rawLoops = GetRawPolygonLoops(edgeCountMap);
		for (auto& rawLoop : rawLoops)
		{
			// Simplify and add polygon.
			SimplifyPolygon(rawLoop);
			if (rawLoop.size() >= VERTEX_COUNT)
				polygons.push_back(std::move(rawLoop));
		}

		return polygons;

		// TODO: Following code needs another pass.
		/*
		// 3) Collect monotone and irregular polygons.
		for (auto& rawLoop : rawLoops)
		{
			// Check if raw loop has no adjacent loops.
			bool hasAdjacency = false;

			for (const auto& vertexId : rawLoop)
			{
				// Check adjacency in vertex adjacency map.
				if (vertexAdjacencyMap.find(vertexId) != vertexAdjacencyMap.end())
				{
					hasAdjacency = true;
					break;
				}
			}

			// If no adjacent loops, simplify and add standalone polygon.
			if (!hasAdjacency)
			{
				SimplifyPolygon(rawLoop);
				if (rawLoop.size() >= VERTEX_COUNT)
					polygons.push_back(std::move(rawLoop));
			}
		}

		// 4) Collect holed polygons converted into irregular polygons.
		auto usedLoops = std::unordered_set<int>{};
		for (auto& rawLoop : rawLoops)
		{
			if (rawLoop.empty())
				continue;

			// Skip processed loops.
			if (usedLoops.find(rawLoop.front()) != usedLoops.end())
				continue;

			// Process adjacent loops and bridge them.
			auto combinedLoop = rawLoop;
			for (auto& adjLoop : rawLoops)
			{
				// If rawLoop has adjacency with adjLoop.
				bool isConnected = false;
				for (const auto& vertex : rawLoop)
				{
					if (vertexAdjacencyMap.find(vertex) != vertexAdjacencyMap.end())
					{
						isConnected = true;
						break;
					}
				}

				if (isConnected)
				{
					// Find closest vertices between two loops.
					int closestVertexId0 = NO_VALUE;
					int closestVertexId1 = NO_VALUE;
					float distSqrMin = INFINITY;

					for (const int& vertexId0 : rawLoop)
					{
						const auto& vertex0 = _vertices[vertexId0];

						for (const int& vertexId1 : adjLoop)
						{
							const auto& vertex1 = _vertices[vertexId1];

							// Track closest pair or vertices.
							// TODO: Revise.
							float distSqr = Vector3::DistanceSquared(vertex0, vertex1);
							if (distSqr < distSqrMin)
							{
								closestVertexId0 = vertexId0;
								closestVertexId1 = vertexId1;
								distSqrMin = distSqr;
							}
						}
					}

					// Reorder and combine loops based on found closest vertices.
					// Ensure closestVertex0 is in first loop and closestVertex1 is in second.
					auto it0 = std::find(combinedLoop.begin(), combinedLoop.end(), closestVertexId0);
					auto it1 = std::find(adjLoop.begin(), adjLoop.end(), closestVertexId1);

					// Insert adjLoop into combinedLoop at correct position.
					// Insert everything after closestVertex0 from combinedLoop and prepend closestVertex1.
					if (it0 != combinedLoop.end() && it1 != adjLoop.end())
					{
						// If vertex0 is found in combinedLoop and vertex1 is found in adjLoop.
						combinedLoop.insert(it0 + 1, adjLoop.begin(), adjLoop.end());
					}

					// Mark loop as used.
					usedLoops.insert(rawLoop.front());

					break;
				}
			}

			// Simplify and add combined polygon.
			SimplifyPolygon(combinedLoop);
			if (combinedLoop.size() >= VERTEX_COUNT)
				polygons.push_back(std::move(combinedLoop));
		}

		// 5) Return optimal polygons.
		return polygons;*/
	}

	std::vector<std::vector<int>> CollisionMeshDesc::GetRawPolygonLoops(const EdgeCountMap& edgeCountMap) const
	{
		// Get boundfary edges.
		auto boundaryEdges = GetBoundaryEdges(edgeCountMap);

		// Run through boundary edges and collect isolated raw loops.
		auto rawLoops = std::vector<std::vector<int>>{};
		while (!boundaryEdges.empty())
		{
			auto rawLoop = std::vector<int>{};

			const auto& firstEdgeIt = boundaryEdges.begin();
			const auto& [firstVertexId0, firstVertexId1] = *firstEdgeIt;

			// Add first boundary edge.
			rawLoop.push_back(firstVertexId0);
			rawLoop.push_back(firstVertexId1);
			boundaryEdges.erase(firstEdgeIt);

			// Run through remaining boundary edges.
			while (!boundaryEdges.empty())
			{
				// TODO: Optimise this. O(n) search here is too inefficient.
				// Find matching edge by checking last loop vertex.
				bool isFound = false;
				for (auto it = boundaryEdges.begin(); it != boundaryEdges.end(); it++)
				{
					const auto& [vertexId0, vertexId1] = *it;

					// If current vertex is equal to one in loop, add next vertex.
					if (vertexId0 == rawLoop.back())
					{
						// Add vertex ID to loop and remove used edge.
						rawLoop.push_back(vertexId1);
						boundaryEdges.erase(it);

						isFound = true;
						break;
					}
					else if (vertexId1 == rawLoop.back())
					{
						// Add vertex ID to loop and remove used edge.
						rawLoop.push_back(vertexId0);
						boundaryEdges.erase(it);

						isFound = true;
						break;
					}
				}

				// No matching edge found; finalize raw loop.
				if (!isFound)
					break;
			}

			// Collect valid raw loop.
			if (rawLoop.size() >= VERTEX_COUNT)
				rawLoops.push_back(std::move(rawLoop));
		}

		// Return raw loops.
		return rawLoops;
	}

	CollisionMeshDesc::CoplanarTriangleMap CollisionMeshDesc::GetCoplanarTriangleMap() const
	{
		constexpr auto NORMAL_EPSILON	 = 0.0001f;
		constexpr auto PLANE_HEIGHT_STEP = BLOCK(1 / 64.0f);

		// Collect coplanar triangle groups.
		auto coplanarTriMap = std::unordered_map<Plane, std::vector<TriangleVertexIds>>{}; // Key = plane, value = triangles.
		for (int i = 0; i < _vertexIds.size(); i += VERTEX_COUNT)
		{
			// Get vertices.
			const auto& vertex0 = _vertices[_vertexIds[i]];
			const auto& vertex1 = _vertices[_vertexIds[i + 1]];
			const auto& vertex2 = _vertices[_vertexIds[i + 2]];

			// Calculate edges.
			auto edge0 = vertex1 - vertex0;
			auto edge1 = vertex2 - vertex0;

			// Calculate plane normal.
			auto normal = edge0.Cross(edge1);
			normal.Normalize();
			normal = RoundNormal(normal, NORMAL_EPSILON);

			// TODO: Make rounding optional.
			// Calculate plane distance.
			float dist = RoundToStep(normal.Dot(vertex0), PLANE_HEIGHT_STEP);

			// Collect triangles by plane.
			auto plane = Plane(normal, dist);
			coplanarTriMap[plane].push_back({ _vertexIds[i], _vertexIds[i + 1], _vertexIds[i + 2] });
		}

		return coplanarTriMap;
	}

	CollisionMeshDesc::EdgeCountMap CollisionMeshDesc::GetEdgeCountMap(const std::vector<TriangleVertexIds>& tris) const
	{
		// Run through triangles.
		auto edgeCountMap = EdgeCountMap{};
		for (const auto& tri : tris)
		{
			// Run through vertices.
			for (int i = 0; i < VERTEX_COUNT; i++)
			{
				// Get edge vertices.
				int vertexId0 = tri[i];
				int vertexId1 = tri[(i + 1) % VERTEX_COUNT];

				// Order vertex ID pairs in sequence.
				if (vertexId0 > vertexId1)
					std::swap(vertexId0, vertexId1);

				// Count shared edges.
				auto edgePair = EdgeVertexIdPair(vertexId0, vertexId1);
				edgeCountMap[edgePair]++;
			}
		}

		return edgeCountMap;
	}

	struct EdgeHash
	{
		template <typename T>
		size_t operator ()(const std::pair<T, T>& edge) const
		{
			// Combine the hashes of the two integers in the pair
			size_t h1 = std::hash<T>{}(edge.first);
			size_t h2 = std::hash<T>{}(edge.second);
			return h1 ^ (h2 << 1); // Combine hashes using bitwise operations
		}
	};

	CollisionMeshDesc::VertexAdjacencyMap CollisionMeshDesc::GetVertexAdjacencyMap(const std::vector<TriangleVertexIds>& tris, const EdgeCountMap& edgeCountMap,
																				   const std::vector<EdgeVertexIdPair>& boundaryEdges) const
	{
		// Run through triangles.
		auto vertexAdjacencyMap = VertexAdjacencyMap{};
		for (const auto& tri : tris)
		{
			// Run through vertices.
			for (int i = 0; i < VERTEX_COUNT; i++)
			{
				// Get edge vertices.
				int vertexId0 = tri[i];
				int vertexId1 = tri[(i + 1) % VERTEX_COUNT];

				// Track vertex adjacency.
				vertexAdjacencyMap[vertexId0].insert(vertexId1);
				vertexAdjacencyMap[vertexId1].insert(vertexId0);
			}
		}

		return vertexAdjacencyMap;
	}

	std::vector<CollisionMeshDesc::EdgeVertexIdPair> CollisionMeshDesc::GetBoundaryEdges(const EdgeCountMap& edgeCountMap) const
	{
		// Extract boundary edges.
		auto boundaryEdges = std::vector<EdgeVertexIdPair>{};
		for (const auto& [edge, count] : edgeCountMap)
		{
			// Filter out non-boundary edge.
			if (count != 1)
				continue;

			boundaryEdges.push_back(edge);
		}

		return boundaryEdges;
	}
	
	std::vector<CollisionMeshDesc::EdgeVertexIdPair> CollisionMeshDesc::GetInnerEdges(const EdgeCountMap& edgeCountMap) const
	{
		// Extract innter edges.
		auto innerEdges = std::vector<EdgeVertexIdPair>{};
		for (const auto& [edge, count] : edgeCountMap)
		{
			// Filter out boundary edge.
			if (count == 1)
				continue;

			innerEdges.push_back(edge);
		}

		return innerEdges;
	}
	
	std::vector<CollisionMeshDesc::EdgeVertexIdPair> CollisionMeshDesc::GetConnectingEdges(const EdgeCountMap& edgeCountMap) const
	{
		// TODO: Extract connecting edges.
		auto connectingEdges = std::vector<EdgeVertexIdPair>{};

		return connectingEdges;
	}

	void CollisionMeshDesc::SimplifyPolygon(std::vector<int>& polygon) const
	{
		// Remove redundant collinear vertices from polygon.
		int i = 0;
		while (polygon.size() > VERTEX_COUNT)
		{
			// Get vertex IDs.
			int vertexId0 = polygon[i];
			int vertexId1 = polygon[(i + 1) % polygon.size()];
			int vertexId2 = polygon[(i + 2) % polygon.size()];

			// Get vertices.
			const auto& vertex0 = _vertices[vertexId0];
			const auto& vertex1 = _vertices[vertexId1];
			const auto& vertex2 = _vertices[vertexId2];

			// Calculate edges.
			auto edge0 = vertex1 - vertex0;
			auto edge1 = vertex2 - vertex1;

			// Edges are collinear; remove vertex 1 and remain at current vertex.
			auto edgeCross = edge0.Cross(edge1);
			if (edgeCross.LengthSquared() < EPSILON)
			{
				Erase(polygon, (i + 1) % polygon.size());
				if (i >= polygon.size())
					i = (int)polygon.size() - 1;
			}
			// Edges aren't collinear; move to next vertex.
			else
			{
				i++;
				if (i >= polygon.size())
					break;
			}
		}
	}

	void CollisionMeshDesc::TriangulatePolygon(std::vector<int>& optimizedVertexIds, const std::vector<int>& polygon, const Vector3& normal) const
	{
		// Invalid polygon; return early.
		if (polygon.size() < VERTEX_COUNT)
			return;

		// Triangulate monotone or irregular polygon using ear clipping method.
		auto vertexIds = polygon;
		int i = 0;
		while (vertexIds.size() > 2)
		{
			// Get vertex IDs.
			int vertexId0 = vertexIds[i];
			int vertexId1 = vertexIds[(i + 1) % vertexIds.size()];
			int vertexId2 = vertexIds[(i + 2) % vertexIds.size()];

			// Ensure correct winding order.
			auto faceNormal = (_vertices[vertexId0] - _vertices[vertexId1]).Cross(_vertices[vertexId2] - _vertices[vertexId1]);
			if (faceNormal.Dot(normal) > 0.0f)
				std::swap(vertexId0, vertexId2);

			// Get vertices.
			const auto& vertex0 = _vertices[vertexId0];
			const auto& vertex1 = _vertices[vertexId1];
			const auto& vertex2 = _vertices[vertexId2];

			// Calculate edges.
			auto edge0 = vertex1 - vertex0;
			auto edge1 = vertex2 - vertex1;

			// Calculate cross product of edges.
			auto edgeCross = edge0.Cross(edge1);

			/*TENLog("edge0: " + std::to_string(edge0.x) + ", " + std::to_string(edge0.y) + ", " + std::to_string(edge0.z));
			TENLog("edge1: " + std::to_string(edge1.x) + ", " + std::to_string(edge1.y) + ", " + std::to_string(edge1.z));
			TENLog("edgeCross: " + std::to_string(edgeCross.x) + ", " + std::to_string(edgeCross.y) + ", " + std::to_string(edgeCross.z));
			TENLog("edgeCrossDotNormal: " + std::to_string(edgeCross.Dot(normal)));
			TENLog("Normal: " + std::to_string(normal.x) + ", " + std::to_string(normal.y) + ", " + std::to_string(normal.z));
			TENLog("NormalLength: " + std::to_string(normal.Length()));*/

			// TODO: Correct this.
			// Angle between edges is convex (< 180 degrees).
			if (edgeCross.Dot(normal) >= 0.0f)
			{
				// Collect optimized vertex IDs.
				optimizedVertexIds.push_back(vertexId0);
				optimizedVertexIds.push_back(vertexId1);
				optimizedVertexIds.push_back(vertexId2);

				// Remove vertex 1.
				vertexIds.erase(vertexIds.begin() + ((i + 1) % vertexIds.size()));
				if (i == vertexIds.size())
					i--;
			}
			// Angle between edges is reflex (>= 180 degrees).
			else
			{
				// Skip current vertex.
				i = (i + 1) % vertexIds.size();
			}
		}
	}
}
