#include "./Math.hlsli"
#include "./CBCamera.hlsli"
#include "./ShaderLight.hlsli"
#include "./VertexEffects.hlsli"
#include "./VertexInput.hlsli"
#include "./Blending.hlsli"
#include "./Shadows.hlsli"

#define INSTANCED_STATIC_MESH_BUCKET_SIZE 100

struct InstancedStaticMesh
{
	float4x4 World;
	float4 Color;
	float4 AmbientLight;
	ShaderLight InstancedStaticLights[MAX_LIGHTS_PER_ITEM];
	uint4 LightInfo;
};

cbuffer InstancedStaticMeshBuffer : register(b3)
{
	InstancedStaticMesh StaticMeshes[INSTANCED_STATIC_MESH_BUCKET_SIZE];
};

struct PixelShaderInput
{
	float4 Position: SV_POSITION;
	float3 WorldPosition: POSITION0;
	float3 Normal: NORMAL;
	float2 UV: TEXCOORD0;
	float4 Color: COLOR;
	float Sheen : SHEEN;
	float4 PositionCopy : TEXCOORD1;
	float4 FogBulbs : TEXCOORD2;
	float DistanceFog : FOG;
	float3 Tangent: TANGENT;
	float3 Binormal: BINORMAL;
	uint InstanceID : SV_InstanceID;
};

struct PixelShaderOutput
{
	float4 Color: SV_TARGET0;
};

Texture2D Texture : register(t0);
SamplerState Sampler : register(s0);

Texture2D NormalTexture : register(t1);
SamplerState NormalTextureSampler : register(s1);

Texture2D SSAOTexture : register(t9);
SamplerState SSAOSampler : register(s9);

PixelShaderInput VS(VertexShaderInput input, uint InstanceID : SV_InstanceID)
{
	PixelShaderInput output;

	float wibble = Wibble(input.Effects.xyz, input.Hash);
	float3 pos = Move(input.Position, input.Effects.xyz, wibble);
	float3 col = Glow(input.Color.xyz, input.Effects.xyz, wibble);

	float4 worldPosition = (mul(float4(pos, 1.0f), StaticMeshes[InstanceID].World));

	output.Position = mul(worldPosition, ViewProjection);
	output.UV = input.UV;
	output.WorldPosition = worldPosition;
	output.Color = float4(col, input.Color.w);
	output.Color *= StaticMeshes[InstanceID].Color;
	output.PositionCopy = output.Position;
	output.Sheen = input.Effects.w;
	output.InstanceID = InstanceID;

	output.Normal = normalize(mul(input.Normal, (float3x3)StaticMeshes[InstanceID].World).xyz);
	output.Tangent = normalize(mul(input.Tangent, (float3x3)StaticMeshes[InstanceID].World).xyz);
	output.Binormal = normalize(mul(input.Binormal, (float3x3)StaticMeshes[InstanceID].World).xyz);

	output.FogBulbs = DoFogBulbsForVertex(worldPosition);
	output.DistanceFog = DoDistanceFogForVertex(worldPosition);

	return output;
}

float3 UnpackNormalMap(float4 n)
{
	n = n * 2.0f - 1.0f;
	n.z = saturate(1.0f - dot(n.xy, n.xy));
	return n.xyz;
}

PixelShaderOutput PS(PixelShaderInput input)
{
	PixelShaderOutput output;

	float4 tex = Texture.Sample(Sampler, input.UV);
	DoAlphaTest(tex);

	uint mode = StaticMeshes[input.InstanceID].LightInfo.y;
	uint numLights = StaticMeshes[input.InstanceID].LightInfo.x;

	float3x3 TBN = float3x3(input.Tangent, input.Binormal, input.Normal);
	float3 normal = UnpackNormalMap(NormalTexture.Sample(NormalTextureSampler, input.UV));
	normal = normalize(mul(normal, TBN));

	float occlusion = 1.0f;
	if (AmbientOcclusion == 1)
	{
		float2 samplePosition;
		samplePosition = input.PositionCopy.xy / input.PositionCopy.w;
		samplePosition = samplePosition * 0.5f + 0.5f;
		samplePosition.y = 1.0f - samplePosition.y;
		occlusion = pow(SSAOTexture.Sample(SSAOSampler, samplePosition).x, AmbientOcclusionExponent);
		
		if (BlendMode == BLENDMODE_ALPHABLEND)
			occlusion = lerp(occlusion, 1.0f, tex.w);
	}

	float3 color = (mode == 0) ?
		CombineLights(
			StaticMeshes[input.InstanceID].AmbientLight.xyz,
			input.Color.xyz,
			tex.xyz, 
			input.WorldPosition, 
			normal, 
			input.Sheen,
			StaticMeshes[input.InstanceID].InstancedStaticLights,
			numLights,
			input.FogBulbs.w) :
		StaticLight(input.Color.xyz, tex.xyz, input.FogBulbs.w);

	color = DoShadow(input.WorldPosition, normal, color, -0.5f);
	color = DoBlobShadows(input.WorldPosition, color);

	output.Color = float4(color * occlusion, tex.w);
	output.Color = DoFogBulbsForPixel(output.Color, float4(input.FogBulbs.xyz, 1.0f));
	output.Color = DoDistanceFogForPixel(output.Color, FogColor, input.DistanceFog);
	output.Color.w *= input.Color.w;

	return output;
}
