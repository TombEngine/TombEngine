#include "./Math.hlsli"
#include "./ShaderLight.hlsli"

#ifndef SKY_CB_MERGED
cbuffer CBSky : register(b1)
{
    float4x4 World;
	//--
    float4 Color;
	//--
    float4 AmbientLight;
	//--
    int ApplyFogBulbs;
    float3 CSkyBuffer_Padding0;
};
#endif