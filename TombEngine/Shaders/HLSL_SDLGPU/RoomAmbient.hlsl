// @SDLGPU_RESOURCE_MAP
// VS_UBO: 0=0 8=1 5=2 6=3
// PS_UBO: 0=0 5=1 6=2
// PS_UBO_MERGE: 3=8,4,12
// PS_TEX: 0=0 3=1
// PS_TEX_ARRAY: 1
// @END_RESOURCE_MAP
#pragma pack_matrix(row_major)

#ifdef VERTEX_SHADER
// VS CBs: Camera(0), Sky(8), Room(5), AnimTex(6) -> contiguous b0-b3
#define REG_CB_CAMERA b0
#define REG_CB_SKY b1
#define REG_CB_ROOM b2
#define REG_CB_ANIMATED_TEXTURE b3
// VS unused CBs -> after VS-used
#define REG_CB_BLENDING b4
#define REG_CB_SHADOW_LIGHT b5
// VS unused textures: use defaults from includes (unique registers, no type conflicts)
#else
// PS CBs: Camera(0), Room(5), AnimTex(6), MergedSSB(Sky+ShadowLight+Blending) -> b0-b3
#define REG_CB_CAMERA b0
#define REG_CB_ROOM b1
#define REG_CB_ANIMATED_TEXTURE b2
#define REG_CB_MERGED_SSB b3
// PS textures -> contiguous t0-t1
#define REG_TEX_SHADOWMAP t1
#define REG_SMP_SHADOWMAP s1
#endif

#include "./CBMergedSSB.hlsli"
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