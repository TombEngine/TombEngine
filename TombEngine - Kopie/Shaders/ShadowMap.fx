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