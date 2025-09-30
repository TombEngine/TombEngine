#ifndef MATERIALSSHADER
#define MATERIALSSHADER

#include "./CBCamera.hlsli"
#include "./CBMaterial.hlsli"

float3 CalculateSkyBoxReflections(float3 worldPosition, float3 faceNormal, float specular, float3 pixelColor, Texture2D skyboxTexture, SamplerState skyboxSampler)
{
    float3 N = faceNormal;
    float3 V = normalize(CamPositionWS - worldPosition);
    float3 R = reflect(-V, N);
    float3 d = mul(float4(R, 0.0f), DualParaboloidView).xyz;
    d.z = max(d.z, 0.0);
    float2 proj = d.xy / (d.z + 1.0f);
    float2 reflectedUV = saturate(proj * 0.5f + 0.5f);
    reflectedUV.y = 1.0 - reflectedUV.y;
    float3 reflectedColor = skyboxTexture.Sample(skyboxSampler, reflectedUV).rgb;
    float strength = saturate(specular);
		
    float ndotv = saturate(dot(N, V));
    float F0 = saturate(specular);
    float Fres = F0 + (1.0f - F0) * pow(1.0f - ndotv, 5.0f);
		
    return lerp(pixelColor, reflectedColor, strength);
}

#endif