#pragma once
#include "Renderer/ConstantBuffers/ShaderLight.h"
#include "Renderer/RendererEnums.h"

namespace TEN::Renderer::ConstantBuffers
{
	struct alignas(16) CLightBuffer
	{
		ShaderLight Lights[MAX_LIGHTS_PER_ITEM];
		//--
		int NumLights;
	};
}