#ifndef OPENGL_BACKEND
#pragma pack_matrix(row_major)
#endif

// GBuffer PS: Camera(b0), Material(b1), AnimTex(b2), Blending(b3)
#define REG_CB_MATERIAL b1
#include "./CBCamera.hlsli"
#include "./Materials.hlsli"
#include "./VertexInput.hlsli"
#include "./VertexEffects.hlsli"
#include "./AnimatedTextures.hlsli"
#include "./Blending.hlsli"
#include "./Math.hlsli"

struct PixelShaderInput
{
	float4 Position: SV_POSITION;
	float3 Normal: NORMAL;
	float3 Tangent: TANGENT;
	float3 Binormal: BINORMAL;
	float2 UV: TEXCOORD0;
	float4 PositionCopy : TEXCOORD1;
	float  DistanceFog : FOG;
};

Texture2D Texture : register(t0);
SamplerState Sampler : register(s0);

Texture2D NormalTexture : register(t1);
SamplerState NormalTextureSampler : register(s1);

struct PixelShaderOutput
{
	float4 Normals: SV_TARGET0;
	float Depth: SV_TARGET1;
    float4 Emissive : SV_Target2;
};

float3 DecodeNormalMap(float4 n)
{
	n = n * 2.0f - 1.0f;
	n.z = saturate(1.0f - dot(n.xy, n.xy));
	return n.xyz;
}

float3 EncodeNormal(float3 n)
{
	n = (n + 1.0f) * 0.5f;
	return n.xyz;
}

PixelShaderOutput PS(PixelShaderInput input)
{
	PixelShaderOutput output;

    if (Animated && Type == 1)
        if (IsWaterfall == 1)
            input.UV = CalculateUVRotateForLegacyWaterfalls(input.UV, 0);
        else
            input.UV = CalculateUVRotate(input.UV, 0);

	float4 color = Texture.Sample(Sampler, input.UV);

	DoAlphaTest(color);

    float4 emissive = EmissiveTexture.Sample(EmissiveSampler, input.UV);
    float specular = ORSHTexture.Sample(ORSHSampler, input.UV).z;

	float3x3 TBN = float3x3(input.Tangent, input.Binormal, input.Normal);
	float3 normal = DecodeNormalMap(NormalTexture.Sample(NormalTextureSampler, input.UV));
	normal = EncodeNormal(normalize(mul(mul(normal, TBN), (float3x3)View)));

	output.Normals.xyz = normal;
	output.Depth = color.w > 0.0f ? input.PositionCopy.z / input.PositionCopy.w : 0.0f;
    output.Emissive.xyz = DoDistanceFogForPixel(emissive, 0.0f, pow(input.DistanceFog, 2)).xyz;
    output.Emissive.w = specular;

	return output;
}
