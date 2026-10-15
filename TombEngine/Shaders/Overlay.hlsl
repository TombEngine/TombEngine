// TEN_SHADER: Vertex VS samplers=0 storage_textures=0 storage_buffers=0 uniform_buffers=1
// TEN_SHADER: Pixel PS samplers=1 storage_textures=0 storage_buffers=0 uniform_buffers=0
// TEN_SHADER: Pixel PSBar samplers=1 storage_textures=0 storage_buffers=0 uniform_buffers=1

// Screen space quads of the HUD and 2D layer (text, bars, display sprites), with premultiplied colors.

#include "./GpuShared.hlsli"
#include "./ShaderInterop.hlsli"

TEN_VS_UNIFORMS(OverlayUniforms, 0)
{
    TEN_OVERLAY_UNIFORMS
};

TEN_PS_UNIFORMS(BarUniforms, 0)
{
    float2 BarStartUV;   // Sprite rectangle in its atlas page.
    float2 BarScale;
    float  Percent;
    float  PoisonFactor;
    uint   IsInner;
    float  BarPadding;
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

// Glass overlay: a highlight band near the top of the bar.
float4 ApplyGlassOverlay(float2 uv, float4 color)
{
    float y = 1.0f - distance(0.1f, uv.y - 0.15f);
    y = saturate(y * y * y * y);
    return saturate(lerp(color, color * 1.6f + float4(0.4f, 0.4f, 0.4f, 0.0f), y));
}

// Bar border and inner fill on bar local coordinates.
float4 PSBar(PixelInput input) : SV_TARGET
{
    float4 color = OverlayMap.Sample(OverlayMapSampler, input.UV * BarScale + BarStartUV);
    if (!IsInner)
        return color;

    clip(Percent - input.UV.x);
    color = lerp(color, float4(214.0f / 512.0f, 241.0f / 512.0f, 18.0f / 512.0f, 1.0f), PoisonFactor);
    return ApplyGlassOverlay(input.UV, color);
}
