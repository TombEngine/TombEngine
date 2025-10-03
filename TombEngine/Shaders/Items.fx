#include "./Math.hlsli"
#include "./CBCamera.hlsli"
#include "./CBItem.hlsli"
#include "./ShaderLight.hlsli"
#include "./VertexEffects.hlsli"
#include "./VertexInput.hlsli"
#include "./Blending.hlsli"
#include "./AnimatedTextures.hlsli"
#include "./Shadows.hlsli"
#include "./Materials.hlsli"

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
    float3 Binormal : BINORMAL;
    float3 FaceNormal : TEXCOORD3;
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
	float wibble = Wibble(input.Effects, DecodeHash(input.AnimationFrameOffsetIndexHash));
	float3 pos = Move(input.Position, input.Effects, wibble);
	float3 col = Glow(input.Color.xyz, input.Effects, wibble);
	float3 worldPosition = mul(float4(pos, 1.0f), world).xyz;

	output.Position = mul(float4(worldPosition, 1.0f), ViewProjection);
    output.UV = GetUVPossiblyAnimated(input.UV, DecodeIndexInPoly(input.Effects), DecodeAnimationFrameOffset(input.AnimationFrameOffsetIndexHash));
	output.Color = float4(col, input.Color.w);
	output.Color *= Color;
	output.PositionCopy = output.Position;
    output.Sheen = DecodeSheen(input.Effects);
	output.Bone = input.BoneIndex[0];
	output.WorldPosition = worldPosition;

    output.Normal = normalize(mul(input.Normal.xyz, (float3x3) world).xyz);
    output.Tangent = normalize(mul(input.Tangent.xyz, (float3x3) world).xyz);
    output.Binormal = SafeNormalize(mul(cross(input.Normal.xyz, input.Tangent.xyz), (float3x3) world).xyz);
    output.FaceNormal = normalize(mul(input.FaceNormal.xyz, (float3x3) world).xyz);
   
	output.FogBulbs = DoFogBulbsForVertex(worldPosition);
	output.DistanceFog = DoDistanceFogForVertex(worldPosition);

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
	
	float4 tex = Texture.Sample(Sampler, input.UV);	
	
    DoAlphaTest(tex);

    float4 occlusionRoughnessSpecular = OcclusionRoughnessSpecularTexture.Sample(OcclusionRoughnessSpecularSampler, input.UV);
    float ambientOcclusion = occlusionRoughnessSpecular.x;
    float roughness = occlusionRoughnessSpecular.y;
    float specular = occlusionRoughnessSpecular.z;
	
    float3 emissive = EmissiveTexture.Sample(EmissiveSampler, input.UV).xyz;
	
	float3x3 TBN = float3x3(input.Tangent, input.Binormal, input.Normal);
	float3 normal = UnpackNormalMap(NormalTexture.Sample(NormalTextureSampler, input.UV));
	normal = normalize(mul(normal, TBN));
	
    // Material effects
    output.Color.xyz = CalculateReflections(input.WorldPosition, output.Color.xyz, input.FaceNormal, normal, specular);

    float2 samplePosition = GetSamplePosition(input.PositionCopy);
	
	float occlusion = 1.0f;
    if (AmbientOcclusion == 1 && BlendModeSupportsSSAO())
	{
		occlusion = pow(SSAOTexture.Sample(SSAOSampler, samplePosition).x, AmbientOcclusionExponent);
		
		if (BlendMode == BLENDMODE_ALPHABLEND)
			occlusion = lerp(occlusion, 1.0f, tex.w);
	}
	
    occlusion *= ambientOcclusion;

	float3 color = (BoneLightModes[input.Bone / 4][input.Bone % 4] == 0) ?
		CombineLights(
			AmbientLight.xyz,
			input.Color.xyz,
			tex.xyz, 
			input.WorldPosition,
			normal, 
			input.Sheen,
			ItemLights, 
			NumItemLights,
			input.FogBulbs.w,
			emissive, 
			specular,
			roughness) :
		StaticLight(input.Color.xyz, tex.xyz, input.FogBulbs.w, emissive);

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
