#include "./CBCamera.hlsli"
#include "./Blending.hlsli"
#include "./VertexInput.hlsli"
#include "./ShaderLight.hlsli"
#include "./AnimatedTextures.hlsli"
#include "./VertexEffects.hlsli"
#include "./Materials.hlsli"

cbuffer CBInventoryItem : register(b1)
{
	float4x4 World;
	float4x4 Bones[32];
	float4 ItemPosition;
	float4 AmbientLight;
};

struct PixelShaderInput
{
	float4 Position: SV_POSITION;
	float3 Normal: NORMAL0;
	float3 WorldPosition : POSITION;
	float2 UV: TEXCOORD;
	float4 Color: COLOR;
    float Sheen : SHEEN;
    float3 Tangent : TANGENT;
    float3 Binormal : BINORMAL;
    float3 FaceNormal : NORMAL1;
};

struct PixelShaderOutput
{
    float4 Color : SV_Target0;
    float4 Emissive : SV_Target1;
};
    
Texture2D Texture : register(t0);
SamplerState Sampler : register(s0);

Texture2D NormalTexture : register(t1);
SamplerState NormalTextureSampler : register(s1);

PixelShaderInput VS(VertexShaderInput input)
{
	PixelShaderInput output;

	output.Position = mul(mul(float4(input.Position, 1.0f), World), ViewProjection);
    output.Normal = (mul(input.Normal.xyz, (float3x3) World).xyz);
    output.Tangent = normalize(mul(input.Tangent.xyz, (float3x3) World).xyz);
    output.Binormal = SafeNormalize(mul(cross(input.Normal.xyz, input.Tangent.xyz), (float3x3) World).xyz);
    output.Color = input.Color;
    output.UV = GetUVPossiblyAnimated(input.UV, DecodeIndexInPoly(input.Effects), DecodeAnimationFrameOffset(input.AnimationFrameOffsetIndexHash));
    output.WorldPosition = (mul(float4(input.Position, 1.0f), World).xyz);
    output.Sheen = DecodeSheen(input.Effects);
    output.FaceNormal = normalize(mul(input.FaceNormal.xyz, (float3x3) World).xyz);
    
	return output;
}

PixelShaderOutput PS(PixelShaderInput input) : SV_TARGET
{
    if (Animated && Type == 1)
        input.UV = CalculateUVRotate(input.UV, 0);
	
    PixelShaderOutput output;
    
    output.Color = Texture.Sample(Sampler, input.UV);
    float3 pos = normalize(input.WorldPosition);

    DoAlphaTest(output.Color);
    
    float4 ORSH = ORSHTexture.Sample(ORSHSampler, input.UV);
    float ambientOcclusion = ORSH.x;
    float roughness = ORSH.y;
    float specular = ORSH.z;
	
    float3 emissive = EmissiveTexture.Sample(EmissiveSampler, input.UV).xyz;
	
    float3x3 TBN = float3x3(input.Tangent, input.Binormal, input.Normal);
    float3 normal = UnpackNormalMap(NormalTexture.Sample(NormalTextureSampler, input.UV));
    normal = normalize(mul(normal, TBN));
    
    // Material effects
    output.Color.xyz = CalculateReflections(input.WorldPosition, output.Color.xyz, normal, specular);
	
    ShaderLight l;
    l.Color = float3(1.0f, 1.0f, 0.5f);
    l.Intensity = 0.3f;
    l.Type = LT_SUN;
    l.Direction = normalize(float3(-1.0f, -0.707f, -0.5f));

    float3 lighting = DoDirectionalLight(pos, normal, l);
    lighting += DoSpecularSun(normal, l, input.Sheen, specular, roughness);;
    lighting += emissive;
    
     // Emissive material
    output.Color.xyz += lighting;
    output.Color.xyz = saturate(output.Color.xyz);

	// Adding some pertubations to the lighting to add a cool effect
    float3 noise = SimplexNoise(output.Color.xyz);
    output.Color.xyz = NormalNoise(output.Color, noise, normal);
    
    output.Emissive = float4(emissive, 1.0f);
	
    return output;
}
