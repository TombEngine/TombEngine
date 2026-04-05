#pragma once
#include <SimpleMath.h>
#include "Renderer/ConstantBuffers/ShaderLight.h"
#include "Renderer/RendererEnums.h"

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
		float HorizonGradientFade;  // [0,1] top-to-bottom alpha gradient on horizon mesh
		float MeshWorldYMin;        // world-space Y of mesh bottom edge (camera-relative)
		float MeshWorldYRange;      // world-space Y extent: top - bottom (positive value)
		//--
		float HorizonGradientRise;  // [0,1] bottom-to-top alpha gradient on horizon mesh
		float _pad0, _pad1, _pad2;
	};
}