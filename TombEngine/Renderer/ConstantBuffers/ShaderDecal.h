#pragma once
#include <SimpleMath.h>

namespace TEN::Renderer::ConstantBuffers
{
	using namespace DirectX::SimpleMath;

	struct alignas(16) ShaderDecal
	{
		Vector3 Position;
		int Pattern;
		float Radius;
		float Opacity;
	};
}