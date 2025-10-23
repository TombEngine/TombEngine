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

Texture2D SSAOTexture : register(t9);
SamplerState SSAOSampler : register(s9);

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
	
    if (Animated && Type == 1)
        input.UV = CalculateUVRotate(input.UV, 0);

	float4 tex = Texture.Sample(Sampler, input.UV);
	
	DoAlphaTest(tex);

    float4 occlusionRoughnessSpecular = OcclusionRoughnessSpecularTexture.Sample(OcclusionRoughnessSpecularSampler, input.UV);
    float ambientOcclusion = occlusionRoughnessSpecular.x;
    float roughness = occlusionRoughnessSpecular.y;
    float specular = occlusionRoughnessSpecular.z;
	
    float3 emissive = EmissiveTexture.Sample(EmissiveSampler, input.UV).xyz;
	
	uint mode = StaticMeshes[input.InstanceID].LightInfo.y;
	uint numLights = StaticMeshes[input.InstanceID].LightInfo.x;

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
