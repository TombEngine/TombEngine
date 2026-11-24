#pragma once

namespace TEN::Renderer
{
	// Six plane view frustum for CPU culling, rebuilt each frame from the view and projection matrices of a RenderView.
	// Plane order: left, right, bottom, top, back, front (see Update).
	class Frustum
	{
	public:

		Frustum() = default;

		void Update(const Matrix& view, const Matrix& projection);
		bool SphereInFrustum(const Vector3& position, float radius) const;
		bool AABBInFrustum(const Vector3& min, const Vector3& max) const;

	private:

		void NormalizePlane(int side);
		// Plane equations (a, b, c, d) with unit normals pointing inside the frustum.
		std::array<std::array<float, 4>, 6> frustum = {};
	};
}