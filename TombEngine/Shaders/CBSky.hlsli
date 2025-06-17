#include "./Math.hlsli"
#include "./ShaderLight.hlsli"

cbuffer CBSkyBuffer : register(b8)
{
	float4x4 World;
	//--
	float4 Color;
	//--
	float4 AmbientLight;
	//--
	int ApplyFogBulbs;
};