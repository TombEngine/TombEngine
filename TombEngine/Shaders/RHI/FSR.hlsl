// TEN_SHADER: Pixel PSEasu samplers=1 storage_textures=0 storage_buffers=0 uniform_buffers=3 hlsl=2018
// TEN_SHADER: Pixel PSRcas samplers=1 storage_textures=0 storage_buffers=0 uniform_buffers=3 hlsl=2018

// AMD FidelityFX Super Resolution 1: EASU upscales the antialiased scene to the screen resolution, then RCAS sharpens
// the result. The algorithms are in FidelityFX/ffx_fsr1.h (MIT license); the constants come from NextRendererUpscale.cpp.

#include "./ShaderInterop.hlsli"
#include "./Fullscreen.hlsli"

#define A_GPU 1
#define A_HLSL 1
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

#define FSR_EASU_F 1
AF4 FsrEasuRF(AF2 p) { return SourceMap.GatherRed(SourceMapSampler, p); }
AF4 FsrEasuGF(AF2 p) { return SourceMap.GatherGreen(SourceMapSampler, p); }
AF4 FsrEasuBF(AF2 p) { return SourceMap.GatherBlue(SourceMapSampler, p); }

#define FSR_RCAS_F 1
AF4 FsrRcasLoadF(ASU2 p) { return SourceMap.Load(int3(p, 0)); }
void FsrRcasInputF(inout AF1 r, inout AF1 g, inout AF1 b) {}

#include "./FidelityFX/ffx_fsr1.h"

float4 PSEasu(FullscreenPixelInput input) : SV_TARGET
{
    AF3 color;
    FsrEasuF(color, AU2(input.Position.xy), EasuConstants0, EasuConstants1, EasuConstants2, EasuConstants3);
    return float4(color, 1.0f);
}

float4 PSRcas(FullscreenPixelInput input) : SV_TARGET
{
    float4 color = float4(0.0f, 0.0f, 0.0f, 1.0f);
    FsrRcasF(color.r, color.g, color.b, AU2(input.Position.xy), RcasConstants);
    return color;
}
