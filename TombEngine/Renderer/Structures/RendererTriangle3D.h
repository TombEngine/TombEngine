#pragma once
#include <array>

namespace TEN::Renderer::Structures
{
	using namespace TEN::Math::Library;

	struct RendererTriangle3D
	{
	private:
		static constexpr auto VERTEX_COUNT = 3;

	public:
		std::array<Vector3, VERTEX_COUNT> Vertices = {};
		Vector4 Color = Vector3::Zero;
	};
}
