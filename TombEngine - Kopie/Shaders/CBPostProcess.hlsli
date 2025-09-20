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
    int ViewportWidth;
    int ViewportHeight;
    //--
    float EffectStrength;
    float3 Tint;
    //--
    float4 SSAOKernel[64];
    //--
    ShaderLensFlare LensFlares[8];
    //--
    int NumLensFlares;
};