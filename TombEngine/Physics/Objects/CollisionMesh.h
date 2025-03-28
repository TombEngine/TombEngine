#pragma once

#include "Specific/Structures/BoundingVolumeHierarchy.h"

using namespace TEN::Structures;

namespace TEN::Physics
{
	class CollisionMeshDesc;

	class LocalCollisionTriangle
	{
	public:
		static constexpr auto VERTEX_COUNT = 3;

	private:
		// Fields

		std::array<int, VERTEX_COUNT> _vertexIds = {};

	public:
		// Constructors

		LocalCollisionTriangle(int vertexId0, int vertexId1, int vertexId2);

		// Getters

		const Vector3& GetVertex0(const std::vector<Vector3>& vertices) const;
		const Vector3& GetVertex1(const std::vector<Vector3>& vertices) const;
		const Vector3& GetVertex2(const std::vector<Vector3>& vertices) const;
		Vector3		   GetNormal(const std::vector<Vector3>& vertices) const;
		BoundingBox	   GetAabb(const std::vector<Vector3>& vertices) const;

		// Inquirers

		bool Intersects(const Ray& ray, float& dist, const std::vector<Vector3>& vertices) const;

		// Debug

		void DrawDebug(const Matrix& transformMatrix, const Matrix& rotMatrix, const std::vector<Vector3>& vertices) const;
	};

	struct CollisionTriangleData
	{
		std::array<Vector3, LocalCollisionTriangle::VERTEX_COUNT> Vertices = {};
		Vector3													  Normal   = Vector3::Zero;
	};

	struct CollisionMeshRayCollisionData
	{
		CollisionTriangleData Triangle = {};
		float				  Distance = 0.0f;
	};

	class CollisionMesh
	{
	private:
		// Fields

		Vector3								_position	 = Vector3::Zero;
		Quaternion							_orientation = Quaternion::Identity;
		std::vector<Vector3>				_vertices	 = {};
		std::vector<LocalCollisionTriangle> _triangles	 = {};

		Bvh _triangleTree = Bvh();

	public:
		// Constructors

		CollisionMesh() = default;
		CollisionMesh(const Vector3& pos, const Quaternion& orient, const CollisionMeshDesc& desc);

		// Getters

		std::optional<CollisionMeshRayCollisionData> GetCollision(const Ray& ray, float dist) const;

		// Setters

		void SetPosition(const Vector3& pos);
		void SetOrientation(const Quaternion& orient);

		// Debug

		void DrawDebug() const;

	private:
		// Helpers

		Matrix GetTranslationMatrix() const;
		Matrix GetRotationMatrix() const;
	};
}
