#include "Renderer/RendererEnums.h"

namespace TEN::Renderer::ConstantBuffers
{
	struct alignas(16) ShaderLensFlare
	{
		Vector3 Position;
		float Padding1;
		//--
		Vector3 Color;
		float Padding2;
	};

	struct alignas(16) CPostProcessBuffer
	{
		float CinematicBarsHeight;
		float ScreenFadeFactor;
		int ViewportWidth;
		int ViewportHeight;
		//--
		float EffectStrength;
		Vector3 Tint;
		//--
		Vector4 SSAOKernel[64];
		//--
		ShaderLensFlare LensFlares[MAX_LENS_FLARES_DRAW];
		//--
		int NumLensFlares;
	};
}
