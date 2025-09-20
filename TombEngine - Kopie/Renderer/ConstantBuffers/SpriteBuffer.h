#pragma once
#include <d3d11.h>
#include <SimpleMath.h>

namespace TEN::Renderer::ConstantBuffers
{
	using namespace DirectX::SimpleMath;

	struct alignas(16) CSpriteBuffer
	{
		float IsSoftParticle;
		int RenderType;
	};
}
