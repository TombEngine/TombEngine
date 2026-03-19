#ifndef OPENGL_BACKEND
#pragma pack_matrix(row_major)
#endif

#define REG_CB_MATERIAL b3
#include "./CBCamera.hlsli"
#include "./CBInstancedStatics.hlsli"
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

PixelShaderInput VS(VertexShaderInput input, uint InstanceID : SV_InstanceID)
{
	PixelShaderInput output;

    float wibble = Wibble(input.Effects, DecodeHash(input.AnimationFrameOffsetIndexHash));
	float3 pos = Move(input.Position, input.Effects, wibble);

	float4 worldPosition = (mul(float4(pos, 1.0f), StaticMeshes[InstanceID].World));

	output.Position = mul(worldPosition, ViewProjection);
    output.PositionCopy = output.Position;
    output.UV = GetUVPossiblyAnimated(input.UV, DecodeIndexInPoly(input.Effects), DecodeAnimationFrameOffset(input.AnimationFrameOffsetIndexHash));
    output.Normal = normalize(mul(input.Normal.xyz, (float3x3) StaticMeshes[InstanceID].World).xyz);
    output.Tangent = normalize(mul(input.Tangent.xyz, (float3x3) StaticMeshes[InstanceID].World).xyz);
    output.Binormal = SafeNormalize(mul(cross(input.Normal.xyz, input.Tangent.xyz), (float3x3) StaticMeshes[InstanceID].World).xyz);
    output.DistanceFog = DoDistanceFogForVertex(pos);

	return output;
}
