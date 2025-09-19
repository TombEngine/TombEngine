#include "./CBCamera.hlsli"
#include "./Blending.hlsli"
#include "./VertexInput.hlsli"
#include "./ShaderLight.hlsli"
#include "./AnimatedTextures.hlsli"

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
	float3 Normal: NORMAL;
	float3 WorldPosition : POSITION;
	float2 UV: TEXCOORD;
	float4 Color: COLOR;
    float Sheen : SHEEN;
    float3 Tangent : TANGENT;
    float3 Binormal : BINORMAL;
};

Texture2D Texture : register(t0);
SamplerState Sampler : register(s0);

Texture2D NormalTexture : register(t1);
SamplerState NormalTextureSampler : register(s1);

Texture2D OcclusionRoughnessSpecularTexture : register(t10);
SamplerState OcclusionRoughnessSpecularSampler : register(s10);

Texture2D EmissiveTexture : register(t11);
SamplerState EmissiveSampler : register(s11);

PixelShaderInput VS(VertexShaderInput input)
{
	PixelShaderInput output;

	output.Position = mul(mul(float4(input.Position, 1.0f), World), ViewProjection);
    output.Normal = (mul(input.Normal, (float3x3) World).xyz);
    output.Tangent = normalize(mul(input.Tangent, (float3x3) World).xyz);
    output.Binormal = normalize(mul(input.Binormal, (float3x3) World).xyz);
    output.Color = input.Color;
    output.UV = GetUVPossiblyAnimated(input.UV, (input.Effects >> 19) & 3, input.AnimationFrameOffset);
    output.WorldPosition = (mul(float4(input.Position, 1.0f), World).xyz);
    output.Sheen = ((input.Effects >> 13) & 64) / 64.0f;
	return output;
}

float4 PS(PixelShaderInput input) : SV_TARGET
{
    if (Animated && Type == 1)
        input.UV = CalculateUVRotate(input.UV, 0);
	
    float4 output = Texture.Sample(Sampler, input.UV);
    float3 pos = normalize(input.WorldPosition);

    DoAlphaTest(output);
    
    float4 occlusionRoughnessSpecular = OcclusionRoughnessSpecularTexture.Sample(OcclusionRoughnessSpecularSampler, input.UV);
    float ambientOcclusion = occlusionRoughnessSpecular.x;
    float roughness = occlusionRoughnessSpecular.y;
    float specular = occlusionRoughnessSpecular.z;
	
    float3 emissive = EmissiveTexture.Sample(EmissiveSampler, input.UV).xyz;
	
    float3x3 TBN = float3x3(input.Tangent, input.Binormal, input.Normal);
    float3 normal = UnpackNormalMap(NormalTexture.Sample(NormalTextureSampler, input.UV));
    normal = normalize(mul(normal, TBN));
	
    ShaderLight l;
    l.Color = float3(1.0f, 1.0f, 0.5f);
    l.Intensity = 0.3f;
    l.Type = LT_SUN;
    l.Direction = normalize(float3(-1.0f, -0.707f, -0.5f));

    output.xyz += DoDirectionalLight(pos, normal, l);
    output.xyz += DoSpecularSun(normal, l, input.Sheen, specular);

	//adding some pertubations to the lighting to add a cool effect
    float3 noise = SimplexNoise(output.xyz);
    output.xyz = NormalNoise(output, noise, normal);
	
    return output;
}
