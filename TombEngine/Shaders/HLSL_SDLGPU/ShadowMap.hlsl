// @SDLGPU_RESOURCE_MAP
// VS_UBO: 0=0 1=1
// PS_UBO:
// PS_TEX:
// @END_RESOURCE_MAP
#pragma pack_matrix(row_major)

#ifdef VERTEX_SHADER
// VS CBs: Camera(0), Item(1) -> contiguous b0-b1
#define REG_CB_CAMERA b0
#define REG_CB_ITEM b1
// VS unused CBs -> after VS-used
#define REG_CB_BLENDING b2
#else
// PS CBs: none (all optimized out)
#define REG_CB_CAMERA b0
#define REG_CB_ITEM b1
#define REG_CB_BLENDING b2
#endif

#include "./CBCamera.hlsli"
#include "./CBItem.hlsli"
#include "./Blending.hlsli"
#include "./Math.hlsli"
#include "./VertexInput.hlsli"

struct PixelShaderInput
{
	float4 Position: SV_POSITION;
	float4 PositionCopy : TEXCOORD1;
	float Depth: TEXCOORD2;
};

Texture2D Texture : register(t0);
SamplerState Sampler : register(s0);

PixelShaderInput VS(VertexShaderInput input)
{
	PixelShaderInput output;
	
	// Blend and apply world matrix
	float4x4 blended = Skinned ? BlendBoneMatrices(input, Bones, (Skinned == 2)) : Bones[input.BoneIndex[0]];
	float4x4 world = mul(blended, World);

	output.Position = mul(mul(float4(input.Position, 1.0f), world), ViewProjection);
	output.Depth = output.Position.z / output.Position.w;
	output.PositionCopy = output.Position;

	return output;
}

float4 PS(PixelShaderInput input) : SV_TARGET
{
	return float4(input.PositionCopy.z / input.PositionCopy.w, 0, 0, 0);
}