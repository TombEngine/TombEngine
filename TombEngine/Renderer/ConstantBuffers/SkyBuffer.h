#pragma once
#include "Renderer/ConstantBuffers/ShaderLight.h"
#include "Renderer/RendererEnums.h"

namespace TEN::Renderer::ConstantBuffers
{
	using namespace TEN::Math::Library;

	struct alignas(16) CSkyBuffer
	{
		Matrix World;
		//--
		Vector4 Color;
		//--
		Vector4 Ambient;
		//--
		int ApplyFogBulbs;
		Vector3 CSkyBuffer_Padding0;
	};
}