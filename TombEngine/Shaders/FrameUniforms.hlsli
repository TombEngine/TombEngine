#ifndef TEN_FRAME_UNIFORMS
#define TEN_FRAME_UNIFORMS

// Uniform blocks shared by the scene shaders. Slot 0 is pushed once per frame, slot 1 per draw.
// Layouts are defined in GpuShared.hlsli.

#include "./GpuShared.hlsli"
#include "./ShaderInterop.hlsli"

TEN_VS_UNIFORMS(FrameVertexUniforms, 0)
{
    TEN_FRAME_VERTEX_UNIFORMS
};

TEN_VS_UNIFORMS(DrawVertexUniforms, 1)
{
    TEN_DRAW_VERTEX_UNIFORMS
};

TEN_PS_UNIFORMS(FramePixelUniforms, 0)
{
    TEN_FRAME_PIXEL_UNIFORMS
};

// Scene colors are clamped to [0, 1] for the 8 bit scene target; the HDR target keeps the range above 1.
float3 ClampSceneColor(float3 color)
{
    return clamp(color, 0.0f, SceneColorMax);
}

TEN_PS_UNIFORMS(DrawPixelUniforms, 1)
{
    TEN_DRAW_PIXEL_UNIFORMS
};

#endif // TEN_FRAME_UNIFORMS
