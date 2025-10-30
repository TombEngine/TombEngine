#include "./CBCamera.hlsli"
#include "./CBRoom.hlsli"
#include "./CBSky.hlsli"
#include "./VertexInput.hlsli"
#include "./VertexEffects.hlsli"
#include "./Blending.hlsli"
#include "./Math.hlsli"
#include "./AnimatedTextures.hlsli"
#include "./Shadows.hlsli"
#include "./ShaderLight.hlsli"

struct PixelShaderInput
{
	float4 Position: SV_POSITION;
	float2 UV: TEXCOORD0;
	float4 Color: COLOR;
	float ClipDepth : TEXCOORD1;
};

Texture2D Texture : register(t0);
SamplerState Sampler : register(s0);

// DPDepth-vertex-shader
PixelShaderInput VS(VertexShaderInput input)
{
	PixelShaderInput output;

	// Transform vertex to DP-space
	output.Position = mul(float4(input.Position, 1.0f), DualParaboloidView);
	output.Position /= output.Position.w;

	// For the back-map z has to be inverted
	output.Position.z *= Hemisphere;

	float L = length(output.Position.xyz);

	output.Position /= L;

	output.ClipDepth = output.Position.z;

	output.Position.x /= output.Position.z + 1.0f;
	output.Position.y /= output.Position.z + 1.0f;

	// Set z for z-buffering and neutralize w
	output.Position.z = (L - NearPlane) / (FarPlane - NearPlane);
	output.Position.w = 1.0f;
    output.UV = GetUVPossiblyAnimated(input.UV, DecodeIndexInPoly(input.Effects), DecodeAnimationFrameOffset(input.AnimationFrameOffsetIndexHash));
	output.Color = input.Color;

	return output;
}

PixelShaderInput VSSky(VertexShaderInput input)
{
	PixelShaderInput output;

	// Transform vertex to DP-space
	output.Position = mul(mul(float4(input.Position, 1.0f), World), DualParaboloidView);
	output.Position /= output.Position.w;

	// For the back-map z has to be inverted
	output.Position.z *= Hemisphere;

	float L = length(output.Position.xyz);

	output.Position /= L;

	output.ClipDepth = output.Position.z;

	output.Position.x /= output.Position.z + 1.0f;
	output.Position.y /= output.Position.z + 1.0f;

	// Set z for z-buffering and neutralize w
	output.Position.z = (L - NearPlane) / (FarPlane - NearPlane);
	output.Position.w = 1.0f;

    output.UV = GetUVPossiblyAnimated(input.UV, DecodeIndexInPoly(input.Effects), DecodeAnimationFrameOffset(input.AnimationFrameOffsetIndexHash));
	output.Color = Color;

	return output;
}

float4 PS(PixelShaderInput input) : SV_TARGET0
{
    if (Animated && Type == 1)
        input.UV = CalculateUVRotate(input.UV, 0);
	
    float4 output = Texture.Sample(Sampler, input.UV);

    clip(input.ClipDepth);

    DoAlphaTest(output);

    output.xyz *= input.Color.xyz;

    return output;
}


float4 PSSky(PixelShaderInput input) : SV_TARGET
{
    if (Animated && Type == 1)
        input.UV = CalculateUVRotate(input.UV, 0);
	
    float4 output = Texture.Sample(Sampler, input.UV);
	
    clip(input.ClipDepth);

    DoAlphaTest(output);

    float3 light = saturate(Color.xyz);
    output.xyz *= light;
    output.w *= Color.w;

    return output;
}