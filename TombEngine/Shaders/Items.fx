#include "./Math.hlsli"
#include "./CBCamera.hlsli"
#include "./CBItem.hlsli"
#include "./ShaderLight.hlsli"
#include "./VertexEffects.hlsli"
#include "./VertexInput.hlsli"
#include "./Blending.hlsli"
#include "./AnimatedTextures.hlsli"
#include "./Shadows.hlsli"

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
	unsigned int Bone : BONE;
};

struct PixelShaderOutput
{
	float4 Color: SV_TARGET0;
};

Texture2D Texture : register(t0);
SamplerState Sampler : register(s0);

Texture2D NormalTexture : register(t1);
SamplerState NormalTextureSampler : register(s1);

Texture2D AmbientMapFrontTexture : register(t7);
SamplerState AmbientMapFrontSampler : register(s7);

Texture2D AmbientMapBackTexture : register(t8);
SamplerState AmbientMapBackSampler : register(s8);

Texture2D SSAOTexture : register(t9);
SamplerState SSAOSampler : register(s9);

PixelShaderInput VS(VertexShaderInput input)
{
	PixelShaderInput output;

	// Blend and apply world matrix
	float4x4 blended = Skinned ? BlendBoneMatrices(input, Bones, (Skinned == 2)) : Bones[input.BoneIndex[0]];
	float4x4 world = mul(blended, World);

	// Calculate vertex effects
	float wibble = Wibble(input.Effects.xyz, input.Hash);
	float3 pos = Move(input.Position, input.Effects.xyz, wibble);
	float3 col = Glow(input.Color.xyz, input.Effects.xyz, wibble);
	float3 worldPosition = mul(float4(pos, 1.0f), world).xyz;

	output.Position = mul(float4(worldPosition, 1.0f), ViewProjection);
	output.UV = input.UV;
	output.Color = float4(col, input.Color.w);
	output.Color *= Color;
	output.PositionCopy = output.Position;
	output.Sheen = input.Effects.w;
	output.Bone = input.BoneIndex[0];
	output.WorldPosition = worldPosition;

	output.Normal = normalize(mul(input.Normal, (float3x3)world).xyz);
	output.Tangent = normalize(mul(input.Tangent, (float3x3)world).xyz);
	output.Binormal = normalize(mul(input.Binormal, (float3x3)world).xyz);

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

	if (Type == 1)
		input.UV = CalculateUVRotate(input.UV, 0);

	float4 tex = Texture.Sample(Sampler, input.UV);	
    DoAlphaTest(tex);

	float3x3 TBN = float3x3(input.Tangent, input.Binormal, input.Normal);
	float3 normal = UnpackNormalMap(NormalTexture.Sample(NormalTextureSampler, input.UV));
	normal = normalize(mul(normal, TBN));

	float3 positionInParaboloidSpace = mul(float4(input.WorldPosition, 1.0f), DualParaboloidView);
	float L = length(positionInParaboloidSpace);
	positionInParaboloidSpace /= L;

	float3 ambientLight = AmbientLight.xyz;

	/*if (positionInParaboloidSpace.z >= 0.0f)
	{
		float2 paraboloidUV;
		paraboloidUV.x = (positionInParaboloidSpace.x / (1.0f + positionInParaboloidSpace.z)) * 0.5f + 0.5f;
		paraboloidUV.y = 1.0f - ((positionInParaboloidSpace.y / (1.0f + positionInParaboloidSpace.z)) * 0.5f + 0.5f);

		ambientLight = AmbientMapFrontTexture.Sample(AmbientMapFrontSampler, paraboloidUV).xyz;
	}
	else
	{	
		float2 paraboloidUV;
		paraboloidUV.x = (positionInParaboloidSpace.x / (1.0f - positionInParaboloidSpace.z)) * 0.5f + 0.5f;
		paraboloidUV.y = 1.0f - ((positionInParaboloidSpace.y / (1.0f - positionInParaboloidSpace.z)) * 0.5f + 0.5f);

		ambientLight = AmbientMapBackTexture.Sample(AmbientMapBackSampler, paraboloidUV).xyz;
	}*/

	float occlusion = 1.0f;
	if (AmbientOcclusion == 1)
	{
		float2 samplePosition;
		samplePosition = input.PositionCopy.xy / input.PositionCopy.w;               // perspective divide
		samplePosition = samplePosition * 0.5f + 0.5f; // transform to range 0.0 - 1.0  
		samplePosition.y = 1.0f - samplePosition.y;
		occlusion = pow(SSAOTexture.Sample(SSAOSampler, samplePosition).x, AmbientOcclusionExponent);
		
		if (BlendMode == BLENDMODE_ALPHABLEND)
			occlusion = lerp(occlusion, 1.0f, tex.w);
	}

	float3 color = (BoneLightModes[input.Bone / 4][input.Bone % 4] == 0) ?
		CombineLights(
			ambientLight,
			input.Color.xyz,
			tex.xyz, 
			input.WorldPosition,
			normal, 
			input.Sheen,
			ItemLights, 
			NumItemLights,
			input.FogBulbs.w) :
		StaticLight(input.Color.xyz, tex.xyz, input.FogBulbs.w);

	float shadowable = step(0.5f, float((NumItemLights & SHADOWABLE_MASK) == SHADOWABLE_MASK));
	float3 shadow = DoShadow(input.WorldPosition, normal, color, -0.5f);
	shadow = DoBlobShadows(input.WorldPosition, shadow);
	color = lerp(color, shadow, shadowable);

	output.Color = saturate(float4(color * occlusion, tex.w));
	output.Color = DoFogBulbsForPixel(output.Color, float4(input.FogBulbs.xyz, 1.0f));
	output.Color = DoDistanceFogForPixel(output.Color, FogColor, input.DistanceFog);
	output.Color.w *= input.Color.w;

	return output;
}
