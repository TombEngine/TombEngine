// TEN_SHADER: Vertex VS samplers=0 storage_textures=0 storage_buffers=0 uniform_buffers=1
// TEN_SHADER: Pixel PS samplers=0 storage_textures=0 storage_buffers=0 uniform_buffers=0

// Debug lines and triangles in world space, with vertex colors, as the legacy Solid shader.

#include "./GpuShared.hlsli"
#include "./ShaderInterop.hlsli"

TEN_VS_UNIFORMS(FrameVertexUniforms, 0)
{
    TEN_FRAME_VERTEX_UNIFORMS
};

struct VertexInput
{
    TEN_LOCATION(0) float3 Position : TEXCOORD0;
    TEN_LOCATION(1) float4 Color    : TEXCOORD1;
};

struct PixelInput
{
    float4 Position : SV_POSITION;
    float4 Color    : TEXCOORD0;
};

PixelInput VS(VertexInput input)
{
    PixelInput output;
    output.Position = mul(float4(input.Position, 1.0f), ViewProjection);
    output.Color = input.Color;
    return output;
}

float4 PS(PixelInput input) : SV_TARGET
{
    return input.Color;
}
