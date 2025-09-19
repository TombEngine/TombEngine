#pragma once
#include <SimpleMath.h>

namespace TEN::Renderer::ConstantBuffers
{
	using namespace DirectX::SimpleMath;

	struct alignas(16) CMaterialBuffer
	{
		Vector4 MaterialParameters0;
		//--
		Vector4 MaterialParameters1;
		//--
		Vector4 MaterialParameters2;
		//--
		Vector4 MaterialParameters3;
		//--
		unsigned int MaterialType;
	};
}