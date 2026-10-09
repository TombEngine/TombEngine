// TEN_SHADER: Vertex VS samplers=0 storage_textures=0 storage_buffers=0 uniform_buffers=1
// TEN_SHADER: Pixel PS samplers=1 storage_textures=0 storage_buffers=0 uniform_buffers=0

// Screen space quads of the HUD and 2D layer (text, bars, display sprites), with premultiplied colors.

#include "./ShaderInterop.hlsli"

TEN_VS_UNIFORMS(OverlayUniforms, 0)
{
    float2 InverseTargetSize;
    float2 OverlayPadding;
};

TEN_PS_TEXTURE(Texture2D, OverlayMap, 0);

struct VertexInput
{
    TEN_LOCATION(0) float2 Position : TEXCOORD0; // Pixels from the top left corner.
    TEN_LOCATION(1) float2 UV       : TEXCOORD1;
    TEN_LOCATION(2) float4 Color    : TEXCOORD2;
};

struct PixelInput
{
    float4 Position : SV_POSITION;
    float2 UV       : TEXCOORD0;
    float4 Color    : TEXCOORD1;
};

PixelInput VS(VertexInput input)
{
    PixelInput output;
    output.Position = float4(input.Position * InverseTargetSize * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    output.UV = input.UV;
    output.Color = input.Color;
    return output;
}

float4 PS(PixelInput input) : SV_TARGET
{
    return OverlayMap.Sample(OverlayMapSampler, input.UV) * input.Color;
}
