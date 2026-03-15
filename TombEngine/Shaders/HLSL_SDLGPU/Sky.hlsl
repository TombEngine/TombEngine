// @SDLGPU_RESOURCE_MAP
// VS_UBO: 0=0 8=1 6=2
// PS_UBO: 0=0 8=1 6=2 12=3
// PS_TEX: 0=0
// @END_RESOURCE_MAP
#pragma pack_matrix(row_major)

#ifdef VERTEX_SHADER
// VS CBs: Camera(0), Sky(8), AnimTex(6) -> contiguous b0-b2
#define REG_CB_CAMERA b0
#define REG_CB_SKY b1
#define REG_CB_ANIMATED_TEXTURE b2
// VS unused CBs -> after VS-used
#define REG_CB_BLENDING b3
#else
// PS CBs: Camera(0), Sky(8), AnimTex(6), Blending(12) -> contiguous b0-b3
#define REG_CB_CAMERA b0
#define REG_CB_SKY b1
#define REG_CB_ANIMATED_TEXTURE b2
#define REG_CB_BLENDING b3
#endif

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

	return output;
}