#pragma once

#include <SimpleMath.h>
#include "Renderer/RendererEnums.h"
#include "Renderer/ConstantBuffers/ShaderLight.h"

namespace TEN::Renderer::ConstantBuffers
{
	using namespace DirectX::SimpleMath;

	// Shared per-object payload uploaded inside both the per-item CB and the per-instance
	// array CB. Field order and alignment match the HLSL Object struct one-to-one — keep
	// them in lockstep when adding fields.
	struct alignas(16) ObjectData
	{
		Matrix      World;
		//--
		Vector4     Color;
		//--
		Vector4     AmbientLight;
		//--
		ShaderLight Lights[MAX_LIGHTS_PER_ITEM];
		//--
		int         NumLights;
		int         LightMode;
		int         ApplyFogBulbs;
		int         ObjectData_Padding0;
	};
}
