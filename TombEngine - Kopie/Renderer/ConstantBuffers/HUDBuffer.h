#pragma once
#include <SimpleMath.h>

namespace TEN::Renderer::ConstantBuffers
{
	using namespace DirectX::SimpleMath;

	struct alignas(16) CHUDBuffer
	{
		Matrix View;
		//--
		Matrix Projection;
	};
}
