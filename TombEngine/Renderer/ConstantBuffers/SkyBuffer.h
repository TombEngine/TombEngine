#pragma once
#include <SimpleMath.h>
#include "Renderer/Renderer.h"

namespace TEN::Renderer::ConstantBuffers
{
	using namespace DirectX::SimpleMath;

	struct alignas(16) CSkyBuffer
	{
		Matrix World;
		//--
		Vector4 Color;
		//--
		Vector4 Ambient;
		//--
		int ApplyFogBulbs;
	};
}