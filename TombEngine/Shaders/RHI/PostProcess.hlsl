// TEN_SHADER: Vertex VS samplers=0 storage_textures=0 storage_buffers=0 uniform_buffers=0
// TEN_SHADER: Pixel PS samplers=1 storage_textures=0 storage_buffers=0 uniform_buffers=3
// TEN_SHADER: Pixel PSDepthCopy samplers=1 storage_textures=0 storage_buffers=0 uniform_buffers=3
// TEN_SHADER: Pixel PSFXAA samplers=1 storage_textures=0 storage_buffers=0 uniform_buffers=3

// Color scheme, screen fade, cinematic bars, tint and gamma of the legacy post-process chain
// (Shaders/PostProcess.hlsl PSMonochrome, PSNegative, PSExclusion and PSFinalPass) in a single pass.

#include "./ShaderInterop.hlsli"
#include "./Common.hlsli"
#include "./Fullscreen.hlsli"

#define COLOR_SCHEME_NONE       0
#define COLOR_SCHEME_MONOCHROME 1
#define COLOR_SCHEME_NEGATIVE   2
#define COLOR_SCHEME_EXCLUSION  3

TEN_PS_UNIFORMS(PostProcessUniforms, 2)
{
    float3 Tint;
    float  Gamma;
    uint   ColorScheme;
    float  ColorSchemeStrength;
    float  ScreenFade;
    float  CinematicBarsHeight;
    float  ViewportHeight;
    float  PostProcessPadding0;
    float  PostProcessPadding1;
    float  PostProcessPadding2;
};

TEN_PS_TEXTURE(Texture2D, SceneMap, 0);

FullscreenPixelInput VS(uint vertexId : SV_VertexID)
{
    return FullscreenVS(vertexId);
}

float3 ApplyColorScheme(float3 color)
{
    float3 schemeColor = color;
    if (ColorScheme == COLOR_SCHEME_MONOCHROME)
        schemeColor = Luma(color);
    else if (ColorScheme == COLOR_SCHEME_NEGATIVE)
        schemeColor = Luma(1.0f - color);
    else if (ColorScheme == COLOR_SCHEME_EXCLUSION)
        schemeColor = color + (1.0f - color) - 2.0f * color * (1.0f - color);

    return lerp(color, schemeColor, ColorSchemeStrength);
}

float4 PS(FullscreenPixelInput input) : SV_Target
{
    float y = input.Position.y / ViewportHeight;
    if (y > 1.0f - CinematicBarsHeight || y < CinematicBarsHeight)
        return float4(0.0f, 0.0f, 0.0f, 1.0f);

    float3 color = ApplyColorScheme(SceneMap.SampleLevel(SceneMapSampler, input.UV, 0.0f).rgb) * ScreenFade;

    // Neutral tint is 0.5, compensated by the color modulation scale as in the legacy final pass.
    color = saturate(pow(saturate(color * Tint * COLOR_MODULATION_SCALE), 1.0f / Gamma));
    return float4(color, 1.0f);
}

// Copies the depth buffer into a color texture that the scene pass can sample while the depth is its target.
float PSDepthCopy(FullscreenPixelInput input) : SV_Target
{
    return SceneMap.SampleLevel(SceneMapSampler, input.UV, 0.0f).x;
}

// FXAA of the legacy renderer (Shaders/FXAA.hlsl), sampling the post-processed image with a linear sampler.
#define FXAA_SPAN_MAX   8.0f
#define FXAA_REDUCE_MUL (1.0f / 4.0f)
#define FXAA_REDUCE_MIN (1.0f / 64.0f)

float3 SampleScene(float2 uv)
{
    return SceneMap.SampleLevel(SceneMapSampler, uv, 0.0f).rgb;
}

float4 PSFXAA(FullscreenPixelInput input) : SV_Target
{
    float width = 0.0f;
    float height = 0.0f;
    SceneMap.GetDimensions(width, height);
    float2 texel = float2(1.0f / width, 1.0f / height);

    float3 rgbNW = SampleScene(input.UV + float2(-texel.x, -texel.y));
    float3 rgbNE = SampleScene(input.UV + float2(texel.x, -texel.y));
    float3 rgbSW = SampleScene(input.UV + float2(-texel.x, texel.y));
    float3 rgbSE = SampleScene(input.UV + float2(texel.x, texel.y));
    float3 rgbM = SampleScene(input.UV);

    float lumaNW = Luma(rgbNW);
    float lumaNE = Luma(rgbNE);
    float lumaSW = Luma(rgbSW);
    float lumaSE = Luma(rgbSE);
    float lumaM = Luma(rgbM);
    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

    float2 direction = float2(-((lumaNW + lumaNE) - (lumaSW + lumaSE)), (lumaNW + lumaSW) - (lumaNE + lumaSE));
    float directionReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * (0.25f * FXAA_REDUCE_MUL), FXAA_REDUCE_MIN);
    float rcpDirectionMin = 1.0f / (min(abs(direction.x), abs(direction.y)) + directionReduce);
    direction = clamp(direction * rcpDirectionMin, -FXAA_SPAN_MAX, FXAA_SPAN_MAX) * texel;

    float3 rgbA = 0.5f * (SampleScene(input.UV + direction * (1.0f / 3.0f - 0.5f)) + SampleScene(input.UV + direction * (2.0f / 2.0f - 0.5f)));
    float3 rgbB = rgbA * 0.5f + 0.25f * (SampleScene(input.UV + direction * -0.5f) + SampleScene(input.UV + direction * 0.5f));

    float lumaB = Luma(rgbB);
    return float4(((lumaB < lumaMin) || (lumaB > lumaMax)) ? rgbA : rgbB, 1.0f);
}
