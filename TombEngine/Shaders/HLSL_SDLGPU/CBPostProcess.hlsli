#include "./Math.hlsli"

struct ShaderLensFlare
{
    float3 Position;
    float Padding1;
    //--
    float3 Color;
    float Padding2;
};

#ifndef REG_CB_POST_PROCESS
#define REG_CB_POST_PROCESS b7
#endif
cbuffer CBPostProcess : register(REG_CB_POST_PROCESS)
{
    float CinematicBarsHeight;
    float ScreenFadeFactor;
    uint2 ViewportSize;
    //--
    float EffectStrength;
    float3 Tint;
    //--
    float4 SSAOKernel[64];
    //--
    ShaderLensFlare LensFlares[8];
    //--
    int NumLensFlares;
    float DownscaleFactor;
    float2 CPostProcessBuffer_Padding0;
    //--
    float2 TexelSize; 
    float2 BlurDirection; 
    //--
    float BlurSigma;
    int BlurRadius;
    float GlowIntensity;
    int GlowSoftAdd;
};