#include "./CBCamera.hlsli"
#include "./Blending.hlsli"
#include "./Math.hlsli"
#include "./SpriteEffects.hlsli"
#include "./ShaderLight.hlsli"
#include "./VertexInput.hlsli"

// NOTE: Shader is used for all 3D and alpha blended sprites because transformed vertices are already sent to GPU instead of instances.

#define FADE_FACTOR .789f

cbuffer SpriteBuffer : register(b9)
{
	float IsSoftParticle;
	int RenderType;
};

struct PixelShaderInput
{
	float4 Position: SV_POSITION;
	float3 Normal: NORMAL;
	float2 UV: TEXCOORD1;
	float4 Color: COLOR;
	float4 PositionCopy: TEXCOORD2;
	float4 FogBulbs : TEXCOORD3;
	float DistanceFog : FOG;
};

Texture2D Texture : register(t0);
SamplerState Sampler : register(s0);

Texture2D DepthTexture : register(t6);
SamplerState DepthSampler : register(s6);

PixelShaderInput VS(VertexShaderInput input)
{
	PixelShaderInput output;

	float4 worldPosition = float4(input.Position, 1.0f);

	output.Position = mul(worldPosition, ViewProjection);
	output.PositionCopy = output.Position;
	output.Normal = input.Normal;
	output.Color = input.Color;
	output.UV = input.UV;

	output.FogBulbs = DoFogBulbsForVertex(worldPosition);
	output.DistanceFog = DoDistanceFogForVertex(worldPosition);

	return output;
}

float4 PS(PixelShaderInput input) : SV_TARGET
{
	float4 output = Texture.Sample(Sampler, input.UV) * input.Color;

	if (IsSoftParticle == 1)
	{
		float particleDepth = input.PositionCopy.z / input.PositionCopy.w;
		input.PositionCopy.xy /= input.PositionCopy.w;

		float2 texCoord = 0.5f * (float2(input.PositionCopy.x, -input.PositionCopy.y) + 1.0f);
		float sceneDepth = DepthTexture.Sample(DepthSampler, texCoord).x;

		sceneDepth = LinearizeDepth(sceneDepth, NearPlane, FarPlane);
		particleDepth = LinearizeDepth(particleDepth, NearPlane, FarPlane);

		if (particleDepth - sceneDepth > 0.01f)
			discard;

		float fade = (sceneDepth - particleDepth) * 1024.0f;
		output.w = min(output.w, fade);
	}

	if (RenderType == 1)
	{
		output = DoLaserBarrierEffect(input.Position, output, input.UV, FADE_FACTOR, Frame);
	}

	if (RenderType == 2)
	{
		output = DoLaserBeamEffect(input.Position, output, input.UV, FADE_FACTOR, Frame);
	}

	output.xyz *= 1.0f - Luma(input.FogBulbs.xyz);
	output.xyz = saturate(output.xyz);

	output = DoDistanceFogForPixel(output, float4(0.0f, 0.0f, 0.0f, 0.0f), input.DistanceFog);

	return output;
}
