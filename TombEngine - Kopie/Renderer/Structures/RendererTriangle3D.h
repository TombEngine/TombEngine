#pragma once
#include <array>
#include <SimpleMath.h>

namespace TEN::Renderer::Structures
{
	using namespace DirectX::SimpleMath;

	struct RendererTriangle3D
	{
	private:
		static constexpr auto VERTEX_COUNT = 3;

	public:
		std::array<Vector3, VERTEX_COUNT> Vertices = {};
		Vector4 Color = Vector3::Zero;
	};
}
