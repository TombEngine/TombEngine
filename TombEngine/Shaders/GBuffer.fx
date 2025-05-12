#include "./CBCamera.hlsli"
#include "./CBItem.hlsli"
#include "./VertexInput.hlsli"
#include "./VertexEffects.hlsli"
#include "./AnimatedTextures.hlsli"
#include "./Blending.hlsli"
#include "./Math.hlsli"

#define INSTANCED_STATIC_MESH_BUCKET_SIZE 100

cbuffer RoomBuffer : register(b5)
{
	int Water;
};

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

cbuffer StaticMatrixBuffer : register(b8)
{
	float4x4 StaticWorld;
};

struct PixelShaderInput
{
	float4 Position: SV_POSITION;
	float3 Normal: NORMAL;
	float3 Tangent: TANGENT;
	float3 Binormal: BINORMAL;
	float2 UV: TEXCOORD0;
	float4 PositionCopy : TEXCOORD1;
};

Texture2D Texture : register(t0);
SamplerState Sampler : register(s0);

Texture2D NormalTexture : register(t1);
SamplerState NormalTextureSampler : register(s1);

struct PixelShaderOutput
{
	float4 Normals: SV_TARGET0;
	float Depth: SV_TARGET1;
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

PixelShaderInput VSRooms(VertexShaderInput input)
{
	PixelShaderInput output;

	// Setting effect weight on TE side prevents portal vertices from moving.
	// Here we just read weight and decide if we should apply refraction or movement effect.
	float weight = input.Effects.z;

	// Calculate vertex effects
	float wibble = Wibble(input.Effects.xyz, input.Hash);
	float3 pos = Move(input.Position, input.Effects.xyz * weight, wibble);

	// Refraction
	float4 screenPos = mul(float4(pos, 1.0f), ViewProjection);
	float2 clipPos = screenPos.xy / screenPos.w;

	if (CameraUnderwater != Water)
	{
		float factor = (Frame + clipPos.x * 320);
		float xOffset = (sin(factor * PI / 20.0f)) * (screenPos.z / 1024) * 4;
		float yOffset = (cos(factor * PI / 20.0f)) * (screenPos.z / 1024) * 4;
		screenPos.x += xOffset * weight;
		screenPos.y += yOffset * weight;
	}

	output.Position = screenPos;
	output.Normal = input.Normal;
	output.Tangent = input.Tangent;
	output.Binormal = input.Binormal;
	output.PositionCopy = screenPos;
	
#ifdef ANIMATED

	if (Type == 0)
		output.UV = GetFrame(input.PolyIndex, input.AnimationFrameOffset);
	else
		output.UV = input.UV; // TODO: true UVRotate in future?
#else
    output.UV = input.UV;
#endif

	return output;
}

PixelShaderInput VSItems(VertexShaderInput input)
{
	PixelShaderInput output;
	
	// Blend and apply world matrix
	float4x4 blended = Skinned ? BlendBoneMatrices(input, Bones, true) : Bones[input.BoneIndex[0]];
	float4x4 world = mul(blended, World);

	// Calculate vertex effects
	float wibble = Wibble(input.Effects.xyz, input.Hash);
	float3 pos = Move(input.Position, input.Effects.xyz, wibble);

	output.Position = mul(mul(float4(pos, 1.0f), world), ViewProjection);
	output.PositionCopy = output.Position;
	output.UV = input.UV;
	output.Normal = normalize(mul(input.Normal, (float3x3)world).xyz);
	output.Tangent = normalize(mul(input.Tangent, (float3x3)world).xyz);
	output.Binormal = normalize(mul(input.Binormal, (float3x3)world).xyz);

	return output;
}

PixelShaderInput VSStatics(VertexShaderInput input)
{
	PixelShaderInput output;

	float wibble = Wibble(input.Effects.xyz, input.Hash);
	float3 pos = Move(input.Position, input.Effects.xyz, wibble);

	float4 worldPosition = (mul(float4(pos, 1.0f), StaticWorld));

	output.Position = mul(worldPosition, ViewProjection);
	output.PositionCopy = output.Position;
	output.UV = input.UV;
	output.Normal = normalize(mul(input.Normal, (float3x3)StaticWorld).xyz);
	output.Tangent = normalize(mul(input.Tangent, (float3x3)StaticWorld).xyz);
	output.Binormal = normalize(mul(input.Binormal, (float3x3)StaticWorld).xyz);

	return output;
}

PixelShaderInput VSInstancedStatics(VertexShaderInput input, uint InstanceID : SV_InstanceID)
{
	PixelShaderInput output;

	float wibble = Wibble(input.Effects.xyz, input.Hash);
	float3 pos = Move(input.Position, input.Effects.xyz, wibble);

	float4 worldPosition = (mul(float4(pos, 1.0f), StaticMeshes[InstanceID].World));

	output.Position = mul(worldPosition, ViewProjection);
	output.PositionCopy = output.Position;
	output.UV = input.UV;
	output.Normal = normalize(mul(input.Normal, (float3x3)StaticMeshes[InstanceID].World).xyz);
	output.Tangent = normalize(mul(input.Tangent, (float3x3)StaticMeshes[InstanceID].World).xyz);
	output.Binormal = normalize(mul(input.Binormal, (float3x3)StaticMeshes[InstanceID].World).xyz);

	return output;
}

PixelShaderOutput PS(PixelShaderInput input)
{
	PixelShaderOutput output;

	float4 color = Texture.Sample(Sampler, input.UV);

	DoAlphaTest(color);

	float3x3 TBN = float3x3(input.Tangent, input.Binormal, input.Normal);
	float3 normal = DecodeNormalMap(NormalTexture.Sample(NormalTextureSampler, input.UV));
	normal = EncodeNormal(normalize(mul(mul(normal, TBN), (float3x3)View)));

	output.Normals.xyz = normal;
	output.Depth = color.w > 0.0f ? input.PositionCopy.z / input.PositionCopy.w : 0.0f;

	return output;
}