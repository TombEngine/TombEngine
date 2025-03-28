#include "framework.h"
#include "Physics/Objects/CollisionMesh.h"

#include "Math/Math.h"
#include "Physics/Objects/CollisionMeshDesc.h"
#include "Specific/Structures/BoundingVolumeHierarchy.h"
#include "Specific/trutils.h"

using namespace TEN::Math;
using namespace TEN::Structures;
using namespace TEN::Utils;

namespace TEN::Physics
{
	LocalCollisionTriangle::LocalCollisionTriangle(int vertexId0, int vertexId1, int vertexId2)
	{
		_vertexIds[0] = vertexId0;
		_vertexIds[1] = vertexId1;
		_vertexIds[2] = vertexId2;
	}

	const Vector3& LocalCollisionTriangle::GetVertex0(const std::vector<Vector3>& vertices) const
	{
		return vertices[_vertexIds[0]];
	}

	const Vector3& LocalCollisionTriangle::GetVertex1(const std::vector<Vector3>& vertices) const
	{
		return vertices[_vertexIds[1]];
	}

	const Vector3& LocalCollisionTriangle::GetVertex2(const std::vector<Vector3>& vertices) const
	{
		return vertices[_vertexIds[2]];
	}

	Vector3 LocalCollisionTriangle::GetNormal(const std::vector<Vector3>& vertices) const
	{
		// Get vertices.
		const auto& vertex0 = GetVertex0(vertices);
		const auto& vertex1 = GetVertex1(vertices);
		const auto& vertex2 = GetVertex2(vertices);

		// Calculate edges.
		auto edge0 = vertex1 - vertex0;
		auto edge1 = vertex2 - vertex0;

		// Calculate edge normal.
		auto normal = edge0.Cross(edge1);
		normal.Normalize();
		return normal;
	}

	BoundingBox LocalCollisionTriangle::GetAabb(const std::vector<Vector3>& vertices) const
	{
		// Get vertices.
		const auto& vertex0 = GetVertex0(vertices);
		const auto& vertex1 = GetVertex1(vertices);
		const auto& vertex2 = GetVertex2(vertices);

		return Geometry::GetBoundingBox({ vertex0, vertex1, vertex2 });
	}

	bool LocalCollisionTriangle::Intersects(const Ray& ray, float& dist, const std::vector<Vector3>& vertices) const
	{
		// Get normal.
		const auto& normal = GetNormal(vertices);

		// Test if ray is facing triangle.
		if (ray.direction.Dot(normal) > EPSILON)
			return false;

		// Get vertices.
		const auto& vertex0 = GetVertex0(vertices);
		const auto& vertex1 = GetVertex1(vertices);
		const auto& vertex2 = GetVertex2(vertices);

		// Calculate edges.
		auto edge0 = vertex1 - vertex0;
		auto edge1 = vertex2 - vertex0;

		// Calculate determinant.
		auto dirCrossEdge1 = ray.direction.Cross(edge1);
		float det = edge0.Dot(dirCrossEdge1);

		// Test if ray and triangle are parallel.
		if (abs(det) < EPSILON)
			return false;

		float invDet = 1.0f / det;

		// Calculate barycentric coordinates.
		float baryCoordVert0 = (ray.position - vertex0).Dot(dirCrossEdge1) * invDet;
		if (baryCoordVert0 < 0.0f || baryCoordVert0 > 1.0f)
			return false;

		auto rayToVert0CrossEdge0 = (ray.position - vertex0).Cross(edge0);
		float baryCoordVert1 = ray.direction.Dot(rayToVert0CrossEdge0) * invDet;
		if (baryCoordVert1 < 0.0f || (baryCoordVert0 + baryCoordVert1) > 1.0f)
			return false;

		// Calculate distance along ray to intersection point.
		float intersectDist = edge1.Dot(rayToVert0CrossEdge0) * invDet;
		if (intersectDist < 0.0f)
			return false;

		dist = intersectDist;
		return true;
	}

	void LocalCollisionTriangle::DrawDebug(const Matrix& transformMatrix, const Matrix& rotMatrix, const std::vector<Vector3>& vertices) const
	{
		constexpr auto TRI_SURF_COLOR	 = Color(1.0f, 1.0f, 0.0f, 0.1f);
		constexpr auto TRI_OUTLINE_COLOR = Color(1.0f, 1.0f, 1.0f, 0.25f);
		constexpr auto NORMAL_LENGTH	 = BLOCK(0.2f);
		constexpr auto NORMAL_COLOR		 = Color(1.0f, 1.0f, 1.0f);

		// Get vertices.
		auto vertex0 = Vector3::Transform(GetVertex0(vertices), transformMatrix);
		auto vertex1 = Vector3::Transform(GetVertex1(vertices), transformMatrix);
		auto vertex2 = Vector3::Transform(GetVertex2(vertices), transformMatrix);

		// Get normal.
		auto normal = Vector3::Transform(GetNormal(vertices), rotMatrix);

		// Draw triangle surface.
		DrawDebugTriangle(vertex0, vertex1, vertex2, TRI_SURF_COLOR);

		// Draw triangle outline.
		DrawDebugLine(vertex0, vertex1, TRI_OUTLINE_COLOR);
		DrawDebugLine(vertex1, vertex2, TRI_OUTLINE_COLOR);
		DrawDebugLine(vertex2, vertex0, TRI_OUTLINE_COLOR);

		// Draw triangle normal.
		auto center = (vertex0 + vertex1 + vertex2) / VERTEX_COUNT;
		DrawDebugLine(center, Geometry::TranslatePoint(center, normal, NORMAL_LENGTH), NORMAL_COLOR);
	}

	CollisionMesh::CollisionMesh(const Vector3& pos, const Quaternion& orient, const CollisionMeshDesc& desc)
	{
		_position = pos;
		_orientation = orient;
		_vertices = desc.GetVertices();

		// Add triangles.
		for (int i = 0; i < desc.GetIds().size(); i += LocalCollisionTriangle::VERTEX_COUNT)
			_triangles.push_back(LocalCollisionTriangle(desc.GetIds()[i], desc.GetIds()[i + 1], desc.GetIds()[i + 2]));

		// Collect triangle IDs and AABBs.
		auto triIds = std::vector<int>(_triangles.size());
		auto triAabbs = std::vector<BoundingBox>(_triangles.size());
		for (int i = 0; i < _triangles.size(); i++)
		{
			const auto& tri = _triangles[i];

			triIds.push_back(i);
			triAabbs.push_back(tri.GetAabb(_vertices));
		}

		// Create triangle tree.
		_triangleTree = Bvh(triIds, triAabbs);
	}

	std::optional<CollisionMeshRayCollisionData> CollisionMesh::GetCollision(const Ray& ray, float dist) const
	{
		// Get matrices.
		auto rotMatrix = GetRotationMatrix();
		auto transformMatrix = rotMatrix * GetTranslationMatrix();

		// Calculate local ray.
		auto localRay = Ray(Vector3::Transform(ray.position, transformMatrix.Invert()), Vector3::Transform(ray.direction, rotMatrix.Invert()));

		// Get bounded triangle IDs.
		auto triIds = _triangleTree.GetBoundedObjectIds(localRay, dist);
		if (triIds.empty())
			return std::nullopt;

		const LocalCollisionTriangle* closestTri = nullptr;
		float closestDist = dist;

		// Find closest triangle.
		for (int triId : triIds)
		{
			const auto& tri = _triangles[triId];

			float intersectDist = 0.0f;
			if (tri.Intersects(localRay, intersectDist, _vertices) && intersectDist <= closestDist)
			{
				closestTri = &tri;
				closestDist = intersectDist;
			}
		}

		// No triangle collision; return early.
		if (closestTri == nullptr)
			return std::nullopt;

		// Create and return ray-mesh collision.
		auto meshColl = CollisionMeshRayCollisionData{};
		meshColl.Triangle.Vertices =
		{
			Vector3::Transform(closestTri->GetVertex0(_vertices), transformMatrix),
			Vector3::Transform(closestTri->GetVertex1(_vertices), transformMatrix),
			Vector3::Transform(closestTri->GetVertex2(_vertices), transformMatrix)
		};
		meshColl.Triangle.Normal = Vector3::Transform(closestTri->GetNormal(_vertices), rotMatrix);
		meshColl.Distance = closestDist;
		return meshColl;
	}

	void CollisionMesh::SetPosition(const Vector3& pos)
	{
		_position = pos;
	}

	void CollisionMesh::SetOrientation(const Quaternion& orient)
	{
		_orientation = orient;
	}

	void CollisionMesh::DrawDebug() const
	{
		auto rotMatrix = GetRotationMatrix();
		auto transformMatrix = rotMatrix * GetTranslationMatrix();

		for (const auto& tri : _triangles)
			tri.DrawDebug(transformMatrix, rotMatrix, _vertices);
	}

	Matrix CollisionMesh::GetTranslationMatrix() const
	{
		return Matrix::CreateTranslation(_position);
	}

	Matrix CollisionMesh::GetRotationMatrix() const
	{
		return Matrix::CreateFromQuaternion(_orientation);
	}
}
