// TEN_SHADER: Vertex VS samplers=0 storage_textures=0 storage_buffers=0 uniform_buffers=0
// TEN_SHADER: Pixel PSDownscale samplers=1 storage_textures=0 storage_buffers=0 uniform_buffers=3
// TEN_SHADER: Pixel PSBlur samplers=1 storage_textures=0 storage_buffers=0 uniform_buffers=3
// TEN_SHADER: Pixel PSCombine samplers=2 storage_textures=0 storage_buffers=0 uniform_buffers=3

// Glow of the frontend (Renderer::ApplyGlow): the prepass emissive target is downscaled and blurred
// with a separable gaussian, then the post-process adds it with a soft add.

#include "./ShaderInterop.hlsli"
#include "./Fullscreen.hlsli"

#define GLOW_BLUR_SIGMA  10.0f
#define GLOW_BLUR_RADIUS 24

TEN_PS_UNIFORMS(GlowUniforms, 2)
{
    float2 TexelSize; // Of the destination target.
    float2 Direction;
};

TEN_PS_TEXTURE(Texture2D, SourceMap, 0);
TEN_PS_TEXTURE(Texture2D, GlowMap, 1);

FullscreenPixelInput VS(uint vertexId : SV_VertexID)
{
    return FullscreenVS(vertexId);
}

float3 SampleSource(float2 uv)
{
    return SourceMap.SampleLevel(SourceMapSampler, uv, 0.0f).rgb;
}

// Same 9 tap filter.
float4 PSDownscale(FullscreenPixelInput input) : SV_Target
{
    float2 dx = float2(TexelSize.x * 0.5f, 0.0f);
    float2 dy = float2(0.0f, TexelSize.y * 0.5f);

    float3 color = SampleSource(input.UV) * 0.5f;
    color += (SampleSource(input.UV + dx) + SampleSource(input.UV - dx) + SampleSource(input.UV + dy) + SampleSource(input.UV - dy)) * 0.125f;
    color += (SampleSource(input.UV + dx + dy) + SampleSource(input.UV + dx - dy) + SampleSource(input.UV - dx + dy) + SampleSource(input.UV - dx - dy)) * 0.03125f;
    return float4(color, 1.0f);
}

float GetGaussianWeight(float x)
{
    return exp(-(x * x) / (2.0f * GLOW_BLUR_SIGMA * GLOW_BLUR_SIGMA));
}

// Neighbouring taps are paired into one bilinear sample between them, at the offset weighted by their gaussian weights,
// which halves the samples for the same filter.
float4 PSBlur(FullscreenPixelInput input) : SV_Target
{
    float3 color = SampleSource(input.UV);
    float weightSum = 1.0f;

    [unroll]
    for (int i = 1; i <= GLOW_BLUR_RADIUS; i += 2)
    {
        float weight0 = GetGaussianWeight((float)i);
        float weight1 = GetGaussianWeight((float)(i + 1));
        float weight = weight0 + weight1;
        float2 offset = Direction * TexelSize * ((i * weight0 + (i + 1) * weight1) / weight);
        color += (SampleSource(input.UV + offset) + SampleSource(input.UV - offset)) * weight;
        weightSum += weight * 2.0f;
    }

    return float4(color / weightSum, 1.0f);
}

// Glow combine with soft add, for menu frames whose glow is not part of the scene post-process.
float4 PSCombine(FullscreenPixelInput input) : SV_Target
{
    float3 color = SampleSource(input.UV);
    float3 glow = GlowMap.SampleLevel(GlowMapSampler, input.UV, 0.0f).rgb;
    return float4(1.0f - (1.0f - color) * (1.0f - glow), 1.0f);
}
