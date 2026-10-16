// TEN_SHADER: Pixel PSEasu samplers=1 storage_textures=0 storage_buffers=0 uniform_buffers=3 hlsl=2018
// TEN_SHADER: Pixel PSRcas samplers=1 storage_textures=0 storage_buffers=0 uniform_buffers=3 hlsl=2018

// AMD FidelityFX Super Resolution 1: EASU upscales the antialiased scene to the screen resolution, then RCAS sharpens
// the result while the overlay draws it as the background quad, which saves a full screen pass. The algorithms are in FidelityFX/ffx_fsr1.h (MIT license); the constants come from SceneRendererUpscale.cpp.
// Both passes use the half precision path with min16float: GPUs with fast 16-bit math, such as AMD
// integrated GPUs where EASU is bound by arithmetic at 4K, run it at half precision; the others run it at full precision.
// It needs no 16-bit device feature.

#include "./ShaderInterop.hlsli"
#include "./Fullscreen.hlsli"

#define A_GPU 1
#define A_HLSL 1
#define A_HALF 1
#include "./FidelityFX/ffx_a.h"

// Slot 2, as the other screen space passes.
TEN_PS_UNIFORMS(FsrUniforms, 2)
{
    uint4 EasuConstants0;
    uint4 EasuConstants1;
    uint4 EasuConstants2;
    uint4 EasuConstants3;
    uint4 RcasConstants;
};

// Antialiased scene for EASU, upscaled scene for RCAS.
TEN_PS_TEXTURE(Texture2D, SourceMap, 0);

#define FSR_EASU_H 1
AH4 FsrEasuRH(AF2 p) { return AH4(SourceMap.GatherRed(SourceMapSampler, p)); }
AH4 FsrEasuGH(AF2 p) { return AH4(SourceMap.GatherGreen(SourceMapSampler, p)); }
AH4 FsrEasuBH(AF2 p) { return AH4(SourceMap.GatherBlue(SourceMapSampler, p)); }

#define FSR_RCAS_H 1
AH4 FsrRcasLoadH(ASW2 p) { return AH4(SourceMap.Load(int3(p, 0))); }
void FsrRcasInputH(inout AH1 r, inout AH1 g, inout AH1 b) {}

#include "./FidelityFX/ffx_fsr1.h"

float4 PSEasu(FullscreenPixelInput input) : SV_TARGET
{
    AH3 color;
    FsrEasuH(color, AU2(input.Position.xy), EasuConstants0, EasuConstants1, EasuConstants2, EasuConstants3);
    return float4(color, 1.0f);
}

// Same interface as the pixel input of Overlay.hlsl; the quad covers the target, so pixels match the source texels.
struct OverlayPixelInput
{
    float4 Position : SV_POSITION;
    TEN_LOCATION(0) float2 UV    : TEXCOORD0;
    TEN_LOCATION(1) float4 Color : TEXCOORD1;
};

float4 PSRcas(OverlayPixelInput input) : SV_TARGET
{
    AH1 red;
    AH1 green;
    AH1 blue;
    FsrRcasH(red, green, blue, AU2(input.Position.xy), RcasConstants);
    return float4(red, green, blue, 1.0f) * input.Color;
}
