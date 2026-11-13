// TEN_SHADER: Vertex VS samplers=0 storage_textures=0 storage_buffers=0 uniform_buffers=0
// TEN_SHADER: Pixel PSPrefilter samplers=1 storage_textures=0 storage_buffers=0 uniform_buffers=3
// TEN_SHADER: Pixel PSDownsample samplers=1 storage_textures=0 storage_buffers=0 uniform_buffers=3
// TEN_SHADER: Pixel PSUpsample samplers=1 storage_textures=0 storage_buffers=0 uniform_buffers=3

// HDR bloom (SceneRendererBloom.cpp): the bright part of the HDR scene is downsampled into a chain of half resolution
// steps, then each step is upsampled with a tent filter and added onto the next larger one. The post-process adds the
// largest step to the scene before tonemapping.

#include "./ShaderInterop.hlsli"
#include "./Common.hlsli"
#include "./Fullscreen.hlsli"

TEN_PS_UNIFORMS(BloomUniforms, 2)
{
    float2 TexelSize; // Of the source texture.
    float  Threshold; // Luma above which the scene blooms.
    float  Knee;      // Width of the soft transition below the threshold.
};

TEN_PS_TEXTURE(Texture2D, SourceMap, 0);

FullscreenPixelInput VS(uint vertexId : SV_VertexID)
{
    return FullscreenVS(vertexId);
}

float3 SampleSource(float2 uv)
{
    return SourceMap.SampleLevel(SourceMapSampler, uv, 0.0f).rgb;
}

// 13 tap downsample in five overlapping 2x2 boxes: the center box weighs 1/2, the four corner boxes 1/8 each.
float3 Downsample13(float2 uv)
{
    float3 a = SampleSource(uv + TexelSize * float2(-2.0f, -2.0f));
    float3 b = SampleSource(uv + TexelSize * float2( 0.0f, -2.0f));
    float3 c = SampleSource(uv + TexelSize * float2( 2.0f, -2.0f));
    float3 d = SampleSource(uv + TexelSize * float2(-1.0f, -1.0f));
    float3 e = SampleSource(uv + TexelSize * float2( 1.0f, -1.0f));
    float3 f = SampleSource(uv + TexelSize * float2(-2.0f,  0.0f));
    float3 g = SampleSource(uv);
    float3 h = SampleSource(uv + TexelSize * float2( 2.0f,  0.0f));
    float3 i = SampleSource(uv + TexelSize * float2(-1.0f,  1.0f));
    float3 j = SampleSource(uv + TexelSize * float2( 1.0f,  1.0f));
    float3 k = SampleSource(uv + TexelSize * float2(-2.0f,  2.0f));
    float3 l = SampleSource(uv + TexelSize * float2( 0.0f,  2.0f));
    float3 m = SampleSource(uv + TexelSize * float2( 2.0f,  2.0f));

    float3 color = (d + e + i + j) * 0.125f;
    color += (a + b + f + g) * 0.03125f;
    color += (b + c + g + h) * 0.03125f;
    color += (f + g + k + l) * 0.03125f;
    color += (g + h + l + m) * 0.03125f;
    return color;
}

// Share of a color above the threshold, with a quadratic knee below it.
float3 GetBrightColor(float3 color)
{
    float luma = max(Luma(color), 0.0001f);
    float soft = clamp(luma - Threshold + Knee, 0.0f, 2.0f * Knee);
    soft = (soft * soft) / (4.0f * Knee + 0.0001f);
    return color * (max(soft, luma - Threshold) / luma);
}

// First step: the threshold is applied after the downsample, and the result is compressed by its luma so single very
// bright texels cannot flicker through the chain.
float4 PSPrefilter(FullscreenPixelInput input) : SV_Target
{
    float3 color = GetBrightColor(max(Downsample13(input.UV), 0.0f));
    return float4(color / (1.0f + Luma(color) * 0.1f), 1.0f);
}

float4 PSDownsample(FullscreenPixelInput input) : SV_Target
{
    return float4(Downsample13(input.UV), 1.0f);
}

// 3x3 tent filter of the smaller step, added by the pipeline blend onto the larger step.
float4 PSUpsample(FullscreenPixelInput input) : SV_Target
{
    float3 color = SampleSource(input.UV) * 4.0f;
    color += (SampleSource(input.UV + TexelSize * float2(-1.0f, 0.0f)) + SampleSource(input.UV + TexelSize * float2(1.0f, 0.0f)) +
              SampleSource(input.UV + TexelSize * float2(0.0f, -1.0f)) + SampleSource(input.UV + TexelSize * float2(0.0f, 1.0f))) * 2.0f;
    color += SampleSource(input.UV + TexelSize * float2(-1.0f, -1.0f)) + SampleSource(input.UV + TexelSize * float2(1.0f, -1.0f)) +
             SampleSource(input.UV + TexelSize * float2(-1.0f, 1.0f)) + SampleSource(input.UV + TexelSize * float2(1.0f, 1.0f));
    return float4(color / 16.0f, 1.0f);
}
