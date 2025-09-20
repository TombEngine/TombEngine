#pragma once

namespace TEN::Physics
{
	class CollisionMeshDesc
	{
	private:
		// Constants
		
		static constexpr auto VERTEX_COUNT = 3;

		// Aliases

		using TriangleVertexIds	  = std::array<int, VERTEX_COUNT>;
		using EdgeVertexIdPair	  = std::pair<int, int>;
		using CoplanarTriangleMap = std::unordered_map<Plane, std::vector<TriangleVertexIds>>; // Key = plane, value = triangle vertex IDs.
		using EdgeCountMap		  = std::unordered_map<EdgeVertexIdPair, int, PairHash>;	   // Key = vertex ID pair defining edge, value = edge count.
		using VertexAdjacencyMap  = std::unordered_map<int, std::unordered_set<int>>;		   // Key = vertex ID, value = adjacent vertex IDs.

		// Fields

		std::vector<Vector3> _vertices	= {};
		std::vector<int>	 _vertexIds = {}; // Vertex IDs in batches of 3 defining triangles.

		std::unordered_map<Vector3, int> _vertexMap = {}; // Key = vertex, value = vertex ID.

	public:
		// Constructors

		CollisionMeshDesc() = default;

		// Getters

		unsigned int				GetTriangleCount() const;
		const std::vector<Vector3>& GetVertices() const;
		const std::vector<int>&		GetIds() const;

		// Utilities

		void InsertTriangle(const Vector3& vertex0, const Vector3& vertex1, const Vector3& vertex2);
		void Optimize();

	private:
		// Helpers

		std::vector<std::vector<int>> GetPolygons(const std::vector<TriangleVertexIds>& tris) const;
		std::vector<std::vector<int>> GetRawPolygonLoops(const EdgeCountMap& edgeCountMap) const;
		CoplanarTriangleMap			  GetCoplanarTriangleMap() const;
		EdgeCountMap				  GetEdgeCountMap(const std::vector<TriangleVertexIds>& tris) const;
		VertexAdjacencyMap			  GetVertexAdjacencyMap(const std::vector<TriangleVertexIds>& tris, const EdgeCountMap& edgeCountMap,
															const std::vector<CollisionMeshDesc::EdgeVertexIdPair>& boundaryEdges) const;

		std::vector<EdgeVertexIdPair> GetBoundaryEdges(const EdgeCountMap& edgeCountMap) const;
		std::vector<EdgeVertexIdPair> GetInnerEdges(const EdgeCountMap& edgeCountMap) const;
		std::vector<EdgeVertexIdPair> GetConnectingEdges(const EdgeCountMap& edgeCountMap) const;
		
		void TriangulatePolygon(std::vector<int>& optimizedVertexIds, const std::vector<int>& polygon, const Vector3& normal) const;
		void SimplifyPolygon(std::vector<int>& polygon) const;
	};
}
