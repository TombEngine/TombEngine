#include "./CBCamera.hlsli"
#include "./CBObjects.hlsli"
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

	// Shadow caster is always a single moveable in Objects[0]; pick the world transform via
	// the unified Skinned flag (0=Static, 1=Rigid, 2=Full, 3=Classic).
	float4x4 world;
	if (Skinned == 0)
	{
		world = Objects[0].World;
	}
	else if (Skinned == 1)
	{
		world = mul(Bones[input.BoneIndex[0]], Objects[0].World);
	}
	else
	{
		float4x4 blended = BlendBoneMatrices(input, Bones, Skinned == 3);
		world = mul(blended, Objects[0].World);
	}

	output.Position = mul(mul(float4(input.Position, 1.0f), world), ViewProjection);
	output.Depth = output.Position.z / output.Position.w;
	output.PositionCopy = output.Position;

	return output;
}

float4 PS(PixelShaderInput input) : SV_TARGET
{
	return float4(input.PositionCopy.z / input.PositionCopy.w, 0, 0, 0);
}