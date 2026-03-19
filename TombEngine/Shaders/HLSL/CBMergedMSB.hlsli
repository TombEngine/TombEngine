// Merged constant buffer: Material + ShadowLight + Blending
// Used by shaders that need all three but must fit within SDL_GPU's
// 4 UBO-per-stage limit. Defines guard macros so the individual CB
// headers skip their own cbuffer declarations.

#ifndef CBMERGEDMSB_SHADER
#define CBMERGEDMSB_SHADER

#define MATERIAL_CB_MERGED 1
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

cbuffer CBCLightBuffer : register(b3)
{
    // === Material (80 bytes, matches CMaterialBuffer) ===
    float4 MaterialParameters0;
    float4 MaterialParameters1;
    float4 MaterialParameters2;
    float4 MaterialParameters3;
    unsigned int MaterialTypeAndFlags;
    int CBMaterial_Padding0;
    int CBMaterial_Padding1;
    int CBMaterial_Padding2;

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
