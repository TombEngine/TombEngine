// @SDLGPU_RESOURCE_MAP
// VS_UBO: 0=0 3=1 6=2
// PS_UBO: 0=0 3=1 6=2
// PS_UBO_MERGE: 3=2,4,12
// PS_TEX: 0=0 1=1 3=2 9=3 10=4 11=5 12=6 13=7
// PS_TEX_ARRAY: 2 7
// @END_RESOURCE_MAP
#pragma pack_matrix(row_major)

#ifdef VERTEX_SHADER
// VS CBs: Camera(0), InstancedStatics(3), AnimTex(6) -> contiguous b0-b2
#define REG_CB_CAMERA b0
#define REG_CB_INSTANCED_STATICS b1
#define REG_CB_ANIMATED_TEXTURE b2
// VS unused CBs -> after VS-used
#define REG_CB_MATERIAL b3
#define REG_CB_SHADOW_LIGHT b4
#define REG_CB_BLENDING b5
// VS unused textures: use defaults from includes (unique registers, no type conflicts)
#else
// PS CBs: Camera(0), InstancedStatics(3), AnimTex(6), MergedMSB(Material+ShadowLight+Blending) -> b0-b3
#define REG_CB_CAMERA b0
#define REG_CB_INSTANCED_STATICS b1
#define REG_CB_ANIMATED_TEXTURE b2
#define REG_CB_MERGED_MSB b3
// PS textures -> contiguous t0-t7
#define REG_TEX_SHADOWMAP t2
#define REG_SMP_SHADOWMAP s2
#define REG_TEX_SSAO t3
#define REG_SMP_SSAO s3
#define REG_TEX_ORSH t4
#define REG_SMP_ORSH s4
#define REG_TEX_EMISSIVE t5
#define REG_SMP_EMISSIVE s5
#define REG_TEX_LEGACY_REFLECTIONS t6
#define REG_SMP_LEGACY_REFLECTIONS s6
#define REG_TEX_SKYBOX_REFLECTIONS t7
#define REG_SMP_SKYBOX_REFLECTIONS s7
#endif

#include "./CBMergedMSB.hlsli"
#include "./Math.hlsli"
#include "./CBCamera.hlsli"
#include "./CBInstancedStatics.hlsli"
#include "./ShaderLight.hlsli"
#include "./VertexEffects.hlsli"
#include "./VertexInput.hlsli"
#include "./Blending.hlsli"
#include "./Shadows.hlsli"
#include "./AnimatedTextures.hlsli"
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

PixelShaderInput VS(VertexShaderInput input, uint InstanceID : SV_InstanceID)
{
	PixelShaderInput output;

	float wibble = Wibble(input.Effects, DecodeHash(input.AnimationFrameOffsetIndexHash));
	float3 pos = Move(input.Position, input.Effects, wibble);
	float3 col = Glow(input.Color.xyz, input.Effects, wibble);

	float4 worldPosition = (mul(float4(pos, 1.0f), StaticMeshes[InstanceID].World));

    output.Position = mul(worldPosition, ViewProjection);
    output.UV = GetUVPossiblyAnimated(input.UV, DecodeIndexInPoly(input.Effects), DecodeAnimationFrameOffset(input.AnimationFrameOffsetIndexHash));
    output.WorldPosition = worldPosition;
	output.Color = float4(col, input.Color.w);
	output.Color *= StaticMeshes[InstanceID].Color;
	output.PositionCopy = output.Position;
    output.Sheen = DecodeSheen(input.Effects);
	output.InstanceID = InstanceID;

    output.Normal = normalize(mul(input.Normal.xyz, (float3x3) StaticMeshes[InstanceID].World).xyz);
    output.Tangent = normalize(mul(input.Tangent.xyz, (float3x3) StaticMeshes[InstanceID].World).xyz);
    output.Binormal = SafeNormalize(mul(cross(input.Normal.xyz, input.Tangent.xyz), (float3x3) StaticMeshes[InstanceID].World).xyz);
    output.FaceNormal = normalize(mul(input.FaceNormal.xyz, (float3x3) StaticMeshes[InstanceID].World).xyz);
   
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
	
	uint mode = StaticMeshes[input.InstanceID].LightInfo.y;
	uint numLights = StaticMeshes[input.InstanceID].LightInfo.x;
	
    // Material effects
    tex.xyz = CalculateReflections(input.WorldPosition, tex.xyz, normal, specular);
	
    // Ambient occlusion
    float occlusion = CalculateOcclusion(GetSamplePosition(input.PositionCopy), tex.w);
    occlusion *= ambientOcclusion;

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
			input.FogBulbs.w, 
			emissive, 
			specular, 
			roughness) :
		StaticLight(input.Color.xyz, tex.xyz, input.FogBulbs.w, emissive);

	color = DoShadow(input.WorldPosition, normal, color, -0.5f);
	color = DoBlobShadows(input.WorldPosition, color);

	output.Color = float4(color * occlusion, tex.w);
	output.Color = DoFogBulbsForPixel(output.Color, float4(input.FogBulbs.xyz, 1.0f));
	output.Color = DoDistanceFogForPixel(output.Color, FogColor, input.DistanceFog);
	output.Color.w *= input.Color.w;

	return output;
}
