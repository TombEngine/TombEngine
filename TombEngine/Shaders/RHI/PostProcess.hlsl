// TEN_SHADER: Vertex VS samplers=0 storage_textures=0 storage_buffers=0 uniform_buffers=0
// TEN_SHADER: Pixel PS samplers=1 storage_textures=0 storage_buffers=0 uniform_buffers=3
// TEN_SHADER: Pixel PSDepthCopy samplers=1 storage_textures=0 storage_buffers=0 uniform_buffers=3

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
