// Merged constant buffer: Sky + ShadowLight + Blending
// Used by RoomAmbient shader which needs Sky instead of Material.
// Defines guard macros so the individual CB headers skip their cbuffer declarations.

#ifndef CBMERGEDSSB_SHADER
#define CBMERGEDSSB_SHADER

#define SKY_CB_MERGED 1
#define SHADOWLIGHT_CB_MERGED 1
#define BLENDING_CB_MERGED 1

#include "./Math.hlsli"
#include "./ShaderLight.hlsli"

#ifndef SPHERE_STRUCT_DEFINED
#define SPHERE_STRUCT_DEFINED
struct Sphere
{
    float3 position;
    float radius;
};
#endif

#ifndef REG_CB_MERGED_SSB
#define REG_CB_MERGED_SSB b3
#endif

cbuffer CBMergedSSB : register(REG_CB_MERGED_SSB)
{
    // === Sky (112 bytes, matches CSkyBuffer) ===
    float4x4 World;
    float4 Color;
    float4 AmbientLight;
    int ApplyFogBulbs;
    float3 CSkyBuffer_Padding0;

    // === ShadowLight (720 bytes, matches CShadowLightBuffer) ===
    ShaderLight Light;
    float4x4 LightViewProjections[6];
    int CastShadows;
    int NumSpheres;
    int ShadowMapSize;
    int ShadowLight_Padding;
    Sphere Spheres[16];

    // === Blending (16 bytes, matches CBlendingBuffer) ===
    uint BlendMode;
    int AlphaTest;
    float AlphaThreshold;
    int CBBlending_Padding0;
};

#endif
