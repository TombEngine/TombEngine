#include "./CBCamera.hlsli"
#include "./Blending.hlsli"
#include "./VertexInput.hlsli"
#include "./Math.hlsli"
#include "./ShaderLight.hlsli"
#include "./CBSky.hlsli"
#include "./AnimatedTextures.hlsli"
#include "./VertexEffects.hlsli"

struct PixelShaderInput
{
	float4 Position: SV_POSITION;
	float3 Normal: NORMAL;
	float2 UV: TEXCOORD;
	float4 Color: COLOR;
	float4 FogBulbs : TEXCOORD3;
	float WorldY : TEXCOORD1;  // World-space Y — globally consistent across mesh
};

Texture2D Texture : register(t0);
SamplerState Sampler : register(s0);

PixelShaderInput VS(VertexShaderInput input)
{
	PixelShaderInput output;

	float4 worldPosition = mul(float4(input.Position, 1.0f), World);

	output.Position = mul(worldPosition, ViewProjection);
    output.Normal = input.Normal.xyz;
	output.Color = input.Color;
    output.UV = GetUVPossiblyAnimated(input.UV, DecodeIndexInPoly(input.Effects), DecodeAnimationFrameOffset(input.AnimationFrameOffsetIndexHash));
	output.FogBulbs = ApplyFogBulbs == 1 ? DoFogBulbsForSky(worldPosition) : 0;
	output.WorldY = worldPosition.y;  // World-space Y for gradient (TEN Y-down)

	return output;
}

float4 PS(PixelShaderInput input) : SV_TARGET
{
    if (Animated && Type == 1)
        input.UV = CalculateUVRotate(input.UV, 0);
	
	float4 output = Texture.Sample(Sampler, input.UV);

	DoAlphaTest(output);

	float3 light = saturate(Color.xyz - float3(input.FogBulbs.w, input.FogBulbs.w, input.FogBulbs.w) * 1.4f);
	output.xyz *= light;
	output.xyz += saturate(input.FogBulbs.xyz);
	output.w *= Color.w;

	// Top-to-bottom alpha gradient on horizon mesh (Altocumulus-driven).
	// WorldY (camera-relative) is remapped using the actual mesh Y bounds computed in C++.
	// MeshWorldYMin stores the TOPMOST camera-relative Y of the mesh.
	// t = 0 at mesh top (transparent), t = 1 at mesh bottom (opaque).
	// Slider 0 = no effect (full opaque), Slider 1 = full gradient.
	if (HorizonGradientFade > 0.0f && MeshWorldYRange > 0.0f)
	{
		// In TEN Y-down, larger Y is lower on screen/world.
		// Since MeshWorldYMin is the TOP of the mesh and MeshWorldYRange is (bottom - top),
		// relY naturally maps from [top, bottom] -> [0, 1].
		float relY = input.WorldY - CamPositionWS.y;
		float t = saturate((relY - MeshWorldYMin) / MeshWorldYRange);
		// Slider controls how far downward the fade reaches:
		// small values = only a short top fade, medium/high values = gradient reaches down faster.
		float fadeExtent = max(HorizonGradientFade * 0.75f, 0.001f);
		float remappedT = saturate(t / fadeExtent);
		// Bias the curve so the upper region stays transparent longer and more peaks disappear.
		float gradientAlpha = smoothstep(0.0f, 1.0f, pow(remappedT, 2.2f));
		// Make the effect ramp up faster so around 0.5 the upper peaks are already gone.
		float gradientStrength = saturate(HorizonGradientFade * 2.0f);
		output.w *= lerp(1.0, gradientAlpha, gradientStrength);
	}

	// Bottom-to-top alpha gradient on horizon mesh (Altocumulus-driven).
	// Mirrors the top-to-bottom gradient but fades the mesh from bottom (transparent) upward.
	// t = 0 at mesh bottom (transparent), t = 1 at mesh top (opaque).
	if (HorizonGradientRise > 0.0f && MeshWorldYRange > 0.0f)
	{
		float relY = input.WorldY - CamPositionWS.y;
		// Invert t so bottom of mesh = 0 (transparent), top = 1 (opaque).
		float t = saturate(1.0f - (relY - MeshWorldYMin) / MeshWorldYRange);
		float fadeExtent = max(HorizonGradientRise * 0.75f, 0.001f);
		float remappedT = saturate(t / fadeExtent);
		float gradientAlpha = smoothstep(0.0f, 1.0f, pow(remappedT, 2.2f));
		float gradientStrength = saturate(HorizonGradientRise * 2.0f);
		output.w *= lerp(1.0, gradientAlpha, gradientStrength);
	}

	return output;
}