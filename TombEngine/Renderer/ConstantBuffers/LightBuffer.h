#pragma once
#include "Renderer/ConstantBuffers/MaterialBuffer.h"
#include "Renderer/ConstantBuffers/ShadowLightBuffer.h"
#include "Renderer/ConstantBuffers/BlendingBuffer.h"
#include "Renderer/ConstantBuffers/SkyBuffer.h"

namespace TEN::Renderer::ConstantBuffers
{
	// Merged constant buffer: Material + ShadowLight + Blending
	// Matches shader-side CBCLightBuffer at register(b3)
	struct alignas(16) CLightBuffer
	{
		CMaterialBuffer Material;
		CShadowLightBuffer Shadow;
		CBlendingBuffer Blending;
	};

	// Merged constant buffer: Sky + ShadowLight + Blending
	// Matches shader-side CBCLightBufferSky at register(b3)
	struct alignas(16) CLightBufferSky
	{
		CSkyBuffer Sky;
		CShadowLightBuffer Shadow;
		CBlendingBuffer Blending;
	};
}
