#include "./Math.hlsli"
#include "./ShaderLight.hlsli"

#ifndef SKY_CB_MERGED
#ifndef REG_CB_SKY
#define REG_CB_SKY b8
#endif
cbuffer CBSky : register(REG_CB_SKY)
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