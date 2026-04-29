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
		Matrix BonesMatrices[BONE_COUNT_MAX];
		//--
		Vector4 Color;
		//--
		Vector4 AmbientLight;
		//--
		int BoneLightModes[BONE_COUNT_MAX];
		//--
		ShaderLight Lights[MAX_LIGHTS_PER_ITEM];
		//--
		int NumLights;
		int Skinned;
		int CItemBuffer_Padding0;
		int CItemBuffer_Padding1;
	};
}