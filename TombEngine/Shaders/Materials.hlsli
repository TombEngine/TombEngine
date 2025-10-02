#ifndef MATERIALSSHADER
#define MATERIALSSHADER

#include "./CBCamera.hlsli"
#include "./CBMaterial.hlsli"

Texture2D OcclusionRoughnessSpecularTexture : register(t10);
SamplerState OcclusionRoughnessSpecularSampler : register(s10);

Texture2D EmissiveTexture : register(t11);
SamplerState EmissiveSampler : register(s11);

Texture2D LegacyReflectionsTexture : register(t12);
SamplerState LegacyReflectionsSampler : register(s12);

Texture2DArray SkyboxReflectionsTexture : register(t13);
SamplerState SkyboxReflectionsSampler : register(s13);

float3 CalculateSkyBoxReflections(float3 worldPosition, float3 faceNormal, float specular, float3 pixelColor)
{
    float3 N = normalize(faceNormal);
    float3 V = normalize(CamPositionWS - worldPosition);
    float3 R = reflect(-V, N);
    
    float3 d = normalize(mul(float4(R, 0.0f), DualParaboloidView).xyz);
    
    const bool isTop = (d.z <= 0.0f);
    float denom = isTop ? (1.0f - d.z) : (1.0f + d.z);
    denom = max(denom, EPSILON);
    
    float2 uv = (d.xy / denom) * 0.5f + 0.5f;

    uv = clamp(uv, EPSILON, 1.0f - EPSILON);
    
    int slice = isTop ? 0 : 1;

    float3 reflectedColor = SkyboxReflectionsTexture.Sample(SkyboxReflectionsSampler, float3(uv, slice)).rgb;
    
    return lerp(pixelColor, reflectedColor, saturate(specular));
}

float2 ToCentralSquare(float2 uvEnv, float aspect)
{
    if (aspect >= 1.0)
    {
        float sx = rcp(aspect);
        return float2(uvEnv.x * sx + (0.5 - 0.5 * sx), uvEnv.y);
    }
    else
    {
        float sy = aspect;
        return float2(uvEnv.x, uvEnv.y * sy + (0.5 - 0.5 * sy));
    }
}

float3 CalculateLegacyReflections(float3 normal, float specular, float3 pixelColor)
{
    // TODO: in the future sample from G-Buffer
    normal = normalize(mul(float4(normal, 0.0f), View).xyz);
    
    float2 reflectedUV = normal.xy * 0.5f + 0.5f;
    reflectedUV = float2(reflectedUV.x, 1.0f - reflectedUV.y);
    float2 reflectedUVSquare = ToCentralSquare(reflectedUV, AspectRatio);
    float3 reflectedColor = LegacyReflectionsTexture.Sample(LegacyReflectionsSampler, reflectedUVSquare).rgb;
    float strength = saturate(specular);
    float w = saturate(0.5f * strength);
    return lerp(pixelColor, reflectedColor, w);
}

#endif