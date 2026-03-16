// @SDLGPU_RESOURCE_MAP
// VS_UBO: 0=0 1=1 6=2
// PS_UBO: 0=0 1=1 6=2
// PS_UBO_MERGE: 3=2,4,12
// PS_TEX: 0=0 1=1 7=2 8=3 3=4 9=5 10=6 11=7 12=8 13=9
// PS_TEX_ARRAY: 4 9
// @END_RESOURCE_MAP
#pragma pack_matrix(row_major)

#ifdef VERTEX_SHADER
// VS CBs: Camera(0), Item(1), AnimTex(6) -> contiguous b0-b2
#define REG_CB_CAMERA b0
#define REG_CB_ITEM b1
#define REG_CB_ANIMATED_TEXTURE b2
// VS unused CBs -> after VS-used
#define REG_CB_MATERIAL b3
#define REG_CB_SHADOW_LIGHT b4
#define REG_CB_BLENDING b5
// VS unused textures: must define all register macros used by global declarations
#define REG_TEX_AMBIENT_FRONT t2
#define REG_SMP_AMBIENT_FRONT s2
#define REG_TEX_AMBIENT_BACK t3
#define REG_SMP_AMBIENT_BACK s3
#else
// PS CBs: Camera(0), Item(1), AnimTex(6), MergedMSB(Material+ShadowLight+Blending) -> b0-b3
#define REG_CB_CAMERA b0
#define REG_CB_ITEM b1
#define REG_CB_ANIMATED_TEXTURE b2
#define REG_CB_MERGED_MSB b3
// PS textures -> contiguous t0-t9
#define REG_TEX_AMBIENT_FRONT t2
#define REG_SMP_AMBIENT_FRONT s2
#define REG_TEX_AMBIENT_BACK t3
#define REG_SMP_AMBIENT_BACK s3
#define REG_TEX_SHADOWMAP t4
#define REG_SMP_SHADOWMAP s4
#define REG_TEX_SSAO t5
#define REG_SMP_SSAO s5
#define REG_TEX_ORSH t6
#define REG_SMP_ORSH s6
#define REG_TEX_EMISSIVE t7
#define REG_SMP_EMISSIVE s7
#define REG_TEX_LEGACY_REFLECTIONS t8
#define REG_SMP_LEGACY_REFLECTIONS s8
#define REG_TEX_SKYBOX_REFLECTIONS t9
#define REG_SMP_SKYBOX_REFLECTIONS s9
#endif

#include "./CBMergedMSB.hlsli"
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

Texture2D AmbientMapFrontTexture : register(REG_TEX_AMBIENT_FRONT);
SamplerState AmbientMapFrontSampler : register(REG_SMP_AMBIENT_FRONT);

Texture2D AmbientMapBackTexture : register(REG_TEX_AMBIENT_BACK);
SamplerState AmbientMapBackSampler : register(REG_SMP_AMBIENT_BACK);

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

    input.UV = ConvertAnimUV(input.UV);
	
    // Apply parallax mapping
    float3x3 TBNf = float3x3(input.Tangent, input.Binormal, input.FaceNormal);
    input.UV = ParallaxOcclusionMapping(TBNf, input.WorldPosition, input.UV);  

    float4 ORSH = ConvertAnimOSRH(ORSHTexture.Sample(ORSHSampler, input.UV));
    float ambientOcclusion = ORSH.x;
    float roughness = ORSH.y;
    float specular = ORSH.z;
	
    float3 emissive = EmissiveTexture.Sample(EmissiveSampler, input.UV).xyz;
	
	float3x3 TBN = float3x3(input.Tangent, input.Binormal, input.Normal);
	float3 normal = ConvertAnimNormal(UnpackNormalMap(NormalTexture.Sample(NormalTextureSampler, input.UV)));
	normal = EnsureNormal(mul(normal, TBN), input.WorldPosition);
	
	float4 tex = Texture.Sample(Sampler, input.UV);
	DoAlphaTest(tex);
	
    // Material effects
    tex.xyz = CalculateReflections(input.WorldPosition, tex.xyz, normal , specular);

    // Ambient occlusion
    float occlusion = CalculateOcclusion(GetSamplePosition(input.PositionCopy), tex.w);
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
