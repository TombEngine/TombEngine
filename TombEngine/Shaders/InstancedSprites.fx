#include "./CBCamera.hlsli"
#include "./Blending.hlsli"
#include "./VertexInput.hlsli"
#include "./Math.hlsli"
#include "./ShaderLight.hlsli"

// NOTE: This shader is used for all opaque or not sorted transparent sprites, that can be instanced for a faster drawing

#define INSTANCED_SPRITES_BUCKET_SIZE 512

struct PixelShaderInput
{
	float4 Position: SV_POSITION;
	float2 UV: TEXCOORD1;
	float4 Color: COLOR;
	float4 PositionCopy: TEXCOORD2;
	float4 FogBulbs : TEXCOORD3;
	float DistanceFog : FOG;
	uint InstanceID : SV_InstanceID;
};

struct InstancedSprite
{
	float4x4 World;
	float4 UV[2];
	float4 Color;
	float IsBillboard;
	float IsSoftParticle;
};

cbuffer InstancedSpriteBuffer : register(b13)
{
	InstancedSprite Sprites[INSTANCED_SPRITES_BUCKET_SIZE];
};

Texture2D Texture : register(t0);
SamplerState Sampler : register(s0);

Texture2D DepthTexture : register(t6);
SamplerState DepthSampler : register(s6);

PixelShaderInput VS(VertexShaderInput input, uint InstanceID : SV_InstanceID)
{
	PixelShaderInput output;

	float4 worldPosition;

	if (Sprites[InstanceID].IsBillboard == 1)
	{
		worldPosition = mul(float4(input.Position, 1.0f), Sprites[InstanceID].World);
		output.Position = mul(mul(float4(input.Position, 1.0f), Sprites[InstanceID].World), ViewProjection);
	}
	else
	{
		worldPosition = float4(input.Position, 1.0f);
		output.Position = mul(float4(input.Position, 1.0f), ViewProjection);
	}

	output.PositionCopy = output.Position;
	output.Color = Sprites[InstanceID].Color;
	output.UV = float2(Sprites[InstanceID].UV[0][input.PolyIndex], Sprites[InstanceID].UV[1][input.PolyIndex]);
	output.InstanceID  = InstanceID;

	output.FogBulbs = DoFogBulbsForVertex(worldPosition);
	output.DistanceFog = DoDistanceFogForVertex(worldPosition);

	return output;
}

// TODO: From NVIDIA SDK, check if it can be useful instead of linear ramp
float Contrast(float Input, float ContrastPower)
{
#if 1
	//piecewise contrast function
	bool IsAboveHalf = Input > 0.5;
	float ToRaise = saturate(2 * (IsAboveHalf ? 1 - Input : Input));
	float Output = 0.5 * pow(ToRaise, ContrastPower);
	Output = IsAboveHalf ? 1 - Output : Output;
	return Output;
#else
	// another solution to create a kind of contrast function
	return 1.0 - exp2(-2 * pow(2.0 * saturate(Input), ContrastPower));
#endif
}

float4 PS(PixelShaderInput input) : SV_TARGET
{
	float4 output = Texture.Sample(Sampler, input.UV) * input.Color;

	if (Sprites[input.InstanceID].IsSoftParticle == 1)
	{
		float particleDepth = input.PositionCopy.z / input.PositionCopy.w;
		input.PositionCopy.xy /= input.PositionCopy.w;
		float2 texCoord = 0.5f * (float2(input.PositionCopy.x, -input.PositionCopy.y) + 1);
		float sceneDepth = DepthTexture.Sample(DepthSampler, texCoord).x;

		sceneDepth = LinearizeDepth(sceneDepth, NearPlane, FarPlane);
		particleDepth = LinearizeDepth(particleDepth, NearPlane, FarPlane);

		if (particleDepth - sceneDepth > 0.01f)
		{
			discard;
		}

		float fade = (sceneDepth - particleDepth) * 1024.0f;
		output.w = min(output.w, fade);
	}

	output.xyz *= 1.0f - Luma(input.FogBulbs.xyz);
	output.xyz = saturate(output.xyz);

	output = DoDistanceFogForPixel(output, float4(0.0f, 0.0f, 0.0f, 0.0f), input.DistanceFog);

	return output;
}