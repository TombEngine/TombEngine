#ifndef MATERIALSSHADER
#define MATERIALSSHADER

#include "./CBCamera.hlsli"
#include "./CBMaterial.hlsli"

float3 CalculateSkyBoxReflections(float3 worldPosition, float3 faceNormal, float specular, float3 pixelColor, Texture2D reflectionsTexture, SamplerState reflectionsSampler)
{
    float3 N = faceNormal;
    float3 V = normalize(CamPositionWS - worldPosition);
    float3 R = reflect(-V, N);
    float3 d = mul(float4(R, 0.0f), DualParaboloidView).xyz;
    d.z = max(d.z, 0.0);
    float2 proj = d.xy / (d.z + 1.0f);
    float2 reflectedUV = saturate(proj * 0.5f + 0.5f);
    reflectedUV.y = 1.0 - reflectedUV.y;
    float3 reflectedColor = reflectionsTexture.Sample(reflectionsSampler, reflectedUV).rgb;
    float strength = saturate(specular);
		
    float ndotv = saturate(dot(N, V));
    float F0 = saturate(specular);
    float Fres = F0 + (1.0f - F0) * pow(1.0f - ndotv, 5.0f);
		
    return lerp(pixelColor, reflectedColor, strength);
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

float3 CalculateLegacyReflections(float3 normal, float specular, float3 pixelColor, Texture2D reflectionsTexture, SamplerState reflectionsSampler)
{
    // TODO: in the future sample from G-Buffer
    normal = normalize(mul(float4(normal, 0.0f), View).xyz);
    
    float2 reflectedUV = normal.xy * 0.5f + 0.5f;
    reflectedUV = float2(reflectedUV.x, 1.0f - reflectedUV.y);
    float2 reflectedUVSquare = ToCentralSquare(reflectedUV, AspectRatio);
    float3 reflectedColor = reflectionsTexture.Sample(reflectionsSampler, reflectedUVSquare).rgb;
    float strength = saturate(specular);
    float w = saturate(0.5f * strength);
    return lerp(pixelColor, reflectedColor, w);
}

#endif