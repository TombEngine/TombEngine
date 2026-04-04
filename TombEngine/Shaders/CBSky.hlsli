#include "./Math.hlsli"
#include "./ShaderLight.hlsli"

cbuffer CBSky : register(b8)
{
    float4x4 World;
	//--
    float4 Color;
	//--
    float4 AmbientLight;
	//--
    int ApplyFogBulbs;
    float HorizonGradientFade;  // [0,1] top-to-bottom alpha gradient on horizon mesh
    float MeshWorldYMin;        // world-space Y of mesh bottom edge (camera-relative)
    float MeshWorldYRange;      // world-space Y extent: top - bottom (positive value)
};