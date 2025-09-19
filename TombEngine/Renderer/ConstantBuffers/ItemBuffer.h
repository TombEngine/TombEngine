#pragma once
#include <SimpleMath.h>
#include "Renderer/RendererEnums.h"
#include "Renderer/ConstantBuffers/ShaderLight.h"

namespace TEN::Renderer::ConstantBuffers
{
	using namespace DirectX::SimpleMath;

	struct alignas(16) CItemBuffer
	{
		Matrix World;
		//--
		Matrix BonesMatrices[MAX_BONES];
		//--
		Vector4 Color;
		//--
		Vector4 AmbientLight;
		//--
		int BoneLightModes[MAX_BONES];
		//--
		ShaderLight Lights[MAX_LIGHTS_PER_ITEM];
		//--
		int NumLights;
		int Skinned;
	};
}