// TEN_SHADER: Vertex VS samplers=0 storage_textures=0 storage_buffers=0 uniform_buffers=0
// TEN_SHADER: Pixel PSAtmosphericSky samplers=0 storage_textures=0 storage_buffers=0 uniform_buffers=4
// TEN_SHADER: Pixel PSAurora samplers=1 storage_textures=0 storage_buffers=0 uniform_buffers=4
// TEN_SHADER: Pixel PSSunMoonDisc samplers=1 storage_textures=0 storage_buffers=0 uniform_buffers=4
// TEN_SHADER: Pixel PSCopy samplers=1 storage_textures=0 storage_buffers=0 uniform_buffers=0

// Atmospheric sky dome, aurora and sun and moon discs of the legacy renderer (Shaders/AtmosphericSkyCore.hlsli):
// fullscreen triangles with the legacy uniform block in slot 2 and the camera inverses in slot 3.

#include "./ShaderInterop.hlsli"
#include "./Fullscreen.hlsli"
#include "./AtmosphericSkyUniforms.hlsli"

TEN_PS_UNIFORMS(SkyCameraUniforms, 3)
{
    float4x4 InverseProjection;
    float4x4 InverseView;
};

// Copy of the scene with the cloud coverage in its alpha channel.
TEN_PS_TEXTURE(Texture2D, SceneAfterClouds, 0);

#define SunMoonLinearSamp SceneAfterCloudsSampler
#define SKY_PIXEL_INPUT   FullscreenPixelInput

FullscreenPixelInput VS(uint vertexId : SV_VertexID)
{
    return FullscreenVS(vertexId);
}

#include "../AtmosphericSkyCore.hlsli"

// Half resolution aurora upscaled into the scene, as the legacy DrawFullScreenQuad.
float4 PSCopy(FullscreenPixelInput input) : SV_TARGET
{
    return SceneAfterClouds.Sample(SceneAfterCloudsSampler, input.UV);
}
