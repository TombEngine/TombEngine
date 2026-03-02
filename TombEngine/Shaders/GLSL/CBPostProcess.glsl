#ifndef CBPOSTPROCESSSHADER
#define CBPOSTPROCESSSHADER

#include "Math.glsl"

struct ShaderLensFlare
{
    vec3 Position;
    float Padding1;
    //--
    vec3 Color;
    float Padding2;
};

layout(std140, binding = 7) uniform CBPostProcess
{
    float CinematicBarsHeight;
    float ScreenFadeFactor;
    uvec2 ViewportSize;
    //--
    // Packed as vec4 to match C++ struct layout (HLSL packs float+float3 in one row,
    // but GLSL std140 would align vec3 to 16 bytes, causing a layout mismatch).
    vec4 _EffectStrengthAndTint; // x = EffectStrength, yzw = Tint
    //--
    vec4 SSAOKernel[64];
    //--
    ShaderLensFlare LensFlares[8];
    //--
    int NumLensFlares;
    float DownscaleFactor;
    vec2 CPostProcessBuffer_Padding0;
    //--
    vec2 TexelSize;
    vec2 BlurDirection;
    //--
    float BlurSigma;
    int BlurRadius;
    float GlowIntensity;
    int GlowSoftAdd;
};

#define EffectStrength _EffectStrengthAndTint.x
#define Tint _EffectStrengthAndTint.yzw

#endif
