#ifndef CBITEMSHADER
#define CBITEMSHADER

#include "./ShaderLight.hlsli"

#ifndef REG_CB_ITEM
#define REG_CB_ITEM b1
#endif
cbuffer CBItem : register(REG_CB_ITEM)
{
	float4x4 World;
	//--
    float4x4 Bones[MAX_BONES];
	//--
    float4 Color;
	//--
    float4 AmbientLight;
	//--
    int4 BoneLightModes[MAX_BONES / 4];
	//--
    ShaderLight ItemLights[MAX_LIGHTS_PER_ITEM];
	//--
	int NumItemLights;
    int Skinned;
    int CBItem_Padding0;
    int CBItem_Padding1;
};

#endif // CBITEMSHADER