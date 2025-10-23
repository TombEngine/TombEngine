#include "./CBCamera.hlsli"
#include "./CBItem.hlsli"
#include "./CBInstancedStatics.hlsli"
#include "./CBRoom.hlsli"
#include "./Materials.hlsli"
#include "./VertexInput.hlsli"
#include "./VertexEffects.hlsli"
#include "./AnimatedTextures.hlsli"
#include "./Blending.hlsli"
#include "./Math.hlsli"
#include "./Materials.hlsli"

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

PixelShaderInput VSRooms(VertexShaderInput input)
{
	PixelShaderInput output;

	// Setting effect weight on TE side prevents portal vertices from moving.
	// Here we just read weight and decide if we should apply refraction or movement effect.
    float weight = DecodeWeight(input.Effects);

	// Calculate vertex effects
	float wibble = Wibble(input.Effects, DecodeHash(input.AnimationFrameOffsetIndexHash));
	float3 pos = Move(input.Position, input.Effects * weight, wibble);

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
    output.Normal = input.Normal.xyz;
	output.Tangent = input.Tangent.xyz;
    output.Binormal = cross(input.Normal.xyz, input.Tangent.xyz);
    output.PositionCopy = screenPos;
    output.UV = GetUVPossiblyAnimated(input.UV, DecodeIndexInPoly(input.Effects), DecodeAnimationFrameOffset(input.AnimationFrameOffsetIndexHash));
	
	return output;
}

PixelShaderInput VSItems(VertexShaderInput input)
{
	PixelShaderInput output;
	
	// Blend and apply world matrix
	float4x4 blended = Skinned ? BlendBoneMatrices(input, Bones, (Skinned == 2)) : Bones[input.BoneIndex[0]];
	float4x4 world = mul(blended, World);

	// Calculate vertex effects
	float wibble = Wibble(input.Effects, DecodeHash(input.AnimationFrameOffsetIndexHash));
	float3 pos = Move(input.Position, input.Effects, wibble);

	output.Position = mul(mul(float4(pos, 1.0f), world), ViewProjection);
    output.PositionCopy = output.Position;
    output.UV = GetUVPossiblyAnimated(input.UV, DecodeIndexInPoly(input.Effects), DecodeAnimationFrameOffset(input.AnimationFrameOffsetIndexHash));
    output.Normal = normalize(mul(input.Normal.xyz, (float3x3) world).xyz);
    output.Tangent = normalize(mul(input.Tangent.xyz, (float3x3) world).xyz);
    output.Binormal = SafeNormalize(mul(cross(input.Normal.xyz, input.Tangent.xyz), (float3x3) world).xyz);
	
	return output;
}

PixelShaderInput VSInstancedStatics(VertexShaderInput input, uint InstanceID : SV_InstanceID)
{
	PixelShaderInput output;

	// Calculate vertex effects
    float wibble = Wibble(input.Effects, DecodeHash(input.AnimationFrameOffsetIndexHash));
	float3 pos = Move(input.Position, input.Effects, wibble);

	float4 worldPosition = (mul(float4(pos, 1.0f), StaticMeshes[InstanceID].World));

	output.Position = mul(worldPosition, ViewProjection);
    output.PositionCopy = output.Position;
    output.UV = GetUVPossiblyAnimated(input.UV, DecodeIndexInPoly(input.Effects), DecodeAnimationFrameOffset(input.AnimationFrameOffsetIndexHash));
    output.Normal = normalize(mul(input.Normal.xyz, (float3x3) StaticMeshes[InstanceID].World).xyz);
    output.Tangent = normalize(mul(input.Tangent.xyz, (float3x3) StaticMeshes[InstanceID].World).xyz);
    output.Binormal = SafeNormalize(mul(cross(input.Normal.xyz, input.Tangent.xyz), (float3x3) StaticMeshes[InstanceID].World).xyz);
	
	return output;
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
    float specular = OcclusionRoughnessSpecularTexture.Sample(OcclusionRoughnessSpecularSampler, input.UV).z;
	
	float3x3 TBN = float3x3(input.Tangent, input.Binormal, input.Normal);
	float3 normal = DecodeNormalMap(NormalTexture.Sample(NormalTextureSampler, input.UV));
	normal = EncodeNormal(normalize(mul(mul(normal, TBN), (float3x3)View)));

	output.Normals.xyz = normal;
    output.Normals.w = MaterialType / 64.0f;
	output.Depth = color.w > 0.0f ? input.PositionCopy.z / input.PositionCopy.w : 0.0f;
    output.Emissive.xyz = emissive.xyz;
    output.Emissive.w = specular;
	
	return output;
}