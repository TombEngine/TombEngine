#pragma once
#include <SimpleMath.h>

namespace TEN::Renderer::ConstantBuffers
{
	using namespace DirectX::SimpleMath;

	static constexpr int NUM_CSM_CASCADES = 3;

	struct alignas(16) CSunLightBuffer
	{
		// Sun light properties.
		Vector3 Direction;
		float Intensity;
		//--
		Vector3 Color;
		int Enabled;
		//--
		Matrix CascadeViewProjections[NUM_CSM_CASCADES];
		//--
		// Packed as float4 to avoid HLSL array padding (float[3] pads each element to 16 bytes).
		// x, y, z = cascade splits; w = shadow map pixel size.
		Vector4 CascadeSplitsAndPixelSize;
		//--
		float DepthBias;
		float SlopeBias;
		float NormalBias;
		float ShadowIntensity;
	};
}
