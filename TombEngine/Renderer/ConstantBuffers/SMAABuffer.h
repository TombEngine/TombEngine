#pragma once
#include <SimpleMath.h>

namespace TEN::Renderer::ConstantBuffers
{
	using namespace DirectX::SimpleMath;

	struct alignas(16) CSMAABuffer
	{
		// Only required for temporal modes (SMAA T2x).
		Vector4 SubsampleIndices = Vector4::Zero;
		//--
		
		// Required for blending results of previous subsample with output render target.
		// Used in SMAA S2x and 4x, for other modes just use 1.0 (no blending).
		float BlendFactor = 0.0f;

		// Can be ignored; purpose is to support interactive custom parameter tweaking.
		float Threshold = 0.0f;
		float MaxSearchSteps = 0.0f;
		float MaxSearchStepsDiag = 0.0f;
		//--
		float CornerRounding = 0.0f;
		Vector3 CSMAABuffer_Padding0;
	};
}