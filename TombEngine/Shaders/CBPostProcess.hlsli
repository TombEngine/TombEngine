#include "./Math.hlsli"

struct ShaderLensFlare
{
    float3 Position;
    float Padding1;
    //--
    float3 Color;
    float Padding2;
};

cbuffer CBPostProcess : register(b7)
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
    float2 Padding3;
    //--
    float2 TexelSize; 
    float2 BlurDirection; 
    //--
    float BlurSigma;
    int BlurRadius;
    float GlowIntensity;
    int GlowSoftAdd;
};