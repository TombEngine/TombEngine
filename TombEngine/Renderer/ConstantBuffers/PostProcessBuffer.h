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
		Vector2i ViewportSize;
		//--
		float EffectStrength;
		Vector3 Tint;
		//--
		Vector4 SSAOKernel[64];
		//--
		ShaderLensFlare LensFlares[MAX_LENS_FLARES_DRAW];
		//--
		int NumLensFlares; 
		float DownscaleFactor;
		Vector2 Padding3;
		//--
		Vector2 TexelSize;
		Vector2 BlurDirection;
		//--
		float BlurSigma;
		int BlurRadius;
		float GlowIntensity;
		int GlowSoftAdd;
	};
}
