#include "./Math.hlsli"
#include "./ShaderLight.hlsli"

cbuffer CBStaticBuffer : register(b8)
{
	float4x4 World;
	//--
	float4 Color;
	//--
	float4 AmbientLight;
	//--
	ShaderLight StaticLights[MAX_LIGHTS_PER_ITEM];
	//--
	int NumStaticLights;
	int LightType;
	int ApplyFogBulbs;
};