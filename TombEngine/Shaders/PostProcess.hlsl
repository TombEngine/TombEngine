// TEN_SHADER: Vertex VS samplers=0 storage_textures=0 storage_buffers=0 uniform_buffers=0
// TEN_SHADER: Pixel PS samplers=7 storage_textures=0 storage_buffers=0 uniform_buffers=3

// Distortion, depth of field, glow, lens flares, color scheme (monochrome, negative, exclusion), screen fade,
// cinematic bars, tint and gamma in a single pass (SceneRendererPostProcess.cpp). PS reads the scene, the
// distortion payload, the depth copy and the glow, depth of field and bloom targets. With the HDR scene target, the
// bloom and the exposure are applied and the result is tonemapped before the color scheme.

#include "./ShaderInterop.hlsli"
#include "./Common.hlsli"
#include "./Fullscreen.hlsli"
#include "./Noise.hlsli"

#define COLOR_SCHEME_NONE       0
#define COLOR_SCHEME_MONOCHROME 1
#define COLOR_SCHEME_NEGATIVE   2
#define COLOR_SCHEME_EXCLUSION  3

#define LENS_FLARES_MAX 8

TEN_PS_UNIFORMS(PostProcessUniforms, 2)
{
    float3 Tint;
    float  Gamma;
    uint   ColorScheme;
    float  ColorSchemeStrength;
    float  ScreenFade;
    float  CinematicBarsHeight;
    float  ViewportHeight;
    float  ViewportWidth;
    float  NearPlane;
    float  FarPlane;
    uint   IsDistortionActive;
    float  Time;
    uint   LensFlareCount;
    uint   IsGlowActive;
    uint   IsDofActive; // Bit 0 far blur, bit 1 near blur.
    uint   IsHdrActive;
    float  Exposure;
    float  BloomIntensity;
    float4 LensFlarePositions[LENS_FLARES_MAX]; // Normalized device xy, depth.
    float4 LensFlareColors[LENS_FLARES_MAX];
};

TEN_PS_TEXTURE(Texture2D, SceneMap, 0);
TEN_PS_TEXTURE(Texture2D, DistortionMap, 1);
TEN_PS_TEXTURE(Texture2D, DepthMap, 2);
TEN_PS_TEXTURE(Texture2D, GlowMap, 3);
TEN_PS_TEXTURE(Texture2D, NearBlurMap, 4); // Depth of field, half resolution, alpha is the circle of confusion.
TEN_PS_TEXTURE(Texture2D, FarBlurMap, 5);
TEN_PS_TEXTURE(Texture2D, BloomMap, 6);    // Half resolution, HDR only.

#define DISTORTION_MIN_WEIGHT                       0.001f
#define DISTORTION_REFERENCE_DEPTH                  1024.0f
#define DISTORTION_REFRACTION_PIXELS                56.0f
#define DISTORTION_DEPTH_REJECT_MIN                 64.0f
#define DISTORTION_EDGE_GUARD_PIXELS                2.0f
#define DISTORTION_NOISE_STRENGTH_SURFACE           0.15f
#define DISTORTION_NOISE_STRENGTH_BILLBOARD         0.035f
#define DISTORTION_NOISE_SCALE_SURFACE              3.5f
#define DISTORTION_NOISE_SCALE_BILLBOARD            25.0f
#define DISTORTION_NOISE_SHIMMER_STRENGTH_SURFACE   0.35f
#define DISTORTION_NOISE_SHIMMER_STRENGTH_BILLBOARD 0.8f
#define DISTORTION_NOISE_SHIMMER_SCALE_SURFACE      40.0f
#define DISTORTION_NOISE_SHIMMER_SCALE_BILLBOARD    65.0f
#define DISTORTION_NOISE_SPEED_BILLBOARD            0.25f
#define DISTORTION_NOISE_SPEED_SURFACE              0.05f
#define DISTORTION_DISTANCE_FADE_START              4096.0f
#define DISTORTION_DISTANCE_FADE_END                10240.0f

#define TONEMAP_KNEE 0.8f

// Depth buffer value mapped back to view space distance in world units.
float GetSceneViewDepth(float2 uv)
{
    float depth = DepthMap.SampleLevel(DepthMapSampler, uv, 0.0f).x;
    return (NearPlane * FarPlane) / (FarPlane - depth * (FarPlane - NearPlane));
}

// Refracted scene UV, from the payload accumulated by the distortion pass.
float2 GetDistortedUV(float2 uv)
{
    float4 distortion = DistortionMap.SampleLevel(DistortionMapSampler, uv, 0.0f);
    float totalStrength = distortion.x;
    if (totalStrength <= DISTORTION_MIN_WEIGHT)
        return uv;

    // Undo the premultiplication of EncodeDistortionPayload (Common.hlsli): strength weighted average emitter depth
    // from its high and low bytes, and the dominant emitter type.
    float emitterDepth = (distortion.y / totalStrength) * 255.0f * 256.0f + (distortion.z / totalStrength) * 255.0f;
    float typeBlend = step(0.5f, distortion.w / totalStrength);
    if (GetSceneViewDepth(uv) - DISTORTION_DEPTH_REJECT_MIN < emitterDepth)
        return uv;

    float distanceFade = 1.0f - smoothstep(DISTORTION_DISTANCE_FADE_START, DISTORTION_DISTANCE_FADE_END, emitterDepth);
    float weight = saturate(totalStrength) * distanceFade;
    if (weight <= DISTORTION_MIN_WEIGHT)
        return uv;

    float noiseStrength = lerp(DISTORTION_NOISE_STRENGTH_BILLBOARD, DISTORTION_NOISE_STRENGTH_SURFACE, typeBlend) * distanceFade;
    float noiseScale = lerp(DISTORTION_NOISE_SCALE_BILLBOARD, DISTORTION_NOISE_SCALE_SURFACE, typeBlend) * distanceFade;
    float shimmerStrength = lerp(DISTORTION_NOISE_SHIMMER_STRENGTH_BILLBOARD, DISTORTION_NOISE_SHIMMER_STRENGTH_SURFACE, typeBlend) * distanceFade;
    float shimmerScale = lerp(DISTORTION_NOISE_SHIMMER_SCALE_BILLBOARD, DISTORTION_NOISE_SHIMMER_SCALE_SURFACE, typeBlend) * distanceFade;
    float noiseSpeed = lerp(DISTORTION_NOISE_SPEED_BILLBOARD, DISTORTION_NOISE_SPEED_SURFACE, typeBlend) * Time;

    float2 refraction = float2(SimplexNoise(float3(uv * noiseScale, noiseSpeed)), SimplexNoise(float3(uv * noiseScale + 5.7f, noiseSpeed + 1.3f))) * 1.5f;

    float shimmerTime = noiseSpeed * 3.5f;
    float2 shimmer = float2(SimplexNoise(float3(uv * shimmerScale * 1.7f, shimmerTime + 13.1f)), SimplexNoise(float3(uv * shimmerScale * 2.3f, shimmerTime + 31.7f))) * shimmerStrength;
    refraction += shimmer * (1.0f - smoothstep(DISTORTION_DISTANCE_FADE_START / 1.5f, DISTORTION_DISTANCE_FADE_END / 3.0f, emitterDepth));

    // Offset in pixels, shrinking with the square root of the emitter distance beyond the reference depth.
    float2 texelSize = float2(1.0f / ViewportWidth, 1.0f / ViewportHeight);
    float perspectiveScale = rsqrt(max(emitterDepth, DISTORTION_REFERENCE_DEPTH) / DISTORTION_REFERENCE_DEPTH);
    float2 offset = refraction * (noiseStrength * DISTORTION_REFRACTION_PIXELS * weight * perspectiveScale) * texelSize;
    float2 edgeGuard = texelSize * DISTORTION_EDGE_GUARD_PIXELS;
    float2 refractedUV = clamp(uv + offset, edgeGuard, 1.0f - edgeGuard);

    // Geometry in front of the emitter is not refracted into it.
    if (GetSceneViewDepth(refractedUV) - DISTORTION_DEPTH_REJECT_MIN < emitterDepth)
        return uv;

    return refractedUV;
}

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

// Identity up to the knee, then an exponential shoulder that approaches white: colors of the 8 bit range keep their look
// and brighter ones roll off instead of clipping.
float3 Tonemap(float3 color)
{
    float3 over = max(color - TONEMAP_KNEE, 0.0f);
    return min(max(color, 0.0f), TONEMAP_KNEE) + (1.0f - TONEMAP_KNEE) * (1.0f - exp(-over / (1.0f - TONEMAP_KNEE)));
}

// Lens flare in normalized device coordinates.
float3 GetLensFlare(float2 uv, float2 position)
{
    float2 main = uv - position;
    float2 uvd = uv * length(uv);

    float angle = atan2(main.y, main.x);
    float distance = pow(length(main), 0.1f);

    // Starburst.
    float f0 = pow(1.0f / (length(uv - position) * 35.0f + 1.0f), 1.5f);
    float rotationOffset = dot(position, float2(0.5f, 0.5f)) * (PI / 2.0f);
    f0 += f0 * (sin((angle + rotationOffset + 1.0f / 18.0f) * 8.0f) * 0.2f + distance * 0.1f + 0.2f);

    // Glow.
    float f2  = max(1.0f / (1.0f + 32.0f * length(uvd + 0.8f  * position)), 0.0f) * 0.25f;
    float f22 = max(1.0f / (1.0f + 32.0f * length(uvd + 0.85f * position)), 0.0f) * 0.23f;
    float f23 = max(1.0f / (1.0f + 32.0f * length(uvd + 0.9f  * position)), 0.0f) * 0.21f;

    // Circular, smaller and symmetric lens artifacts.
    float2 uvx = lerp(uv, uvd, -0.5f);
    float f4  = max(0.01f - pow(length(uvx + 0.4f  * position), 2.4f), 0.0f) * 9.0f;
    float f42 = max(0.01f - pow(length(uvx + 0.45f * position), 2.4f), 0.0f) * 7.0f;
    float f43 = max(0.01f - pow(length(uvx + 0.5f  * position), 2.4f), 0.0f) * 6.0f;

    uvx = lerp(uv, uvd, -0.4f);
    float f5  = max(0.01f - pow(length(uvx + 0.2f * position), 5.5f), 0.0f) * 2.0f;
    float f52 = max(0.01f - pow(length(uvx + 0.4f * position), 5.5f), 0.0f) * 2.0f;
    float f53 = max(0.01f - pow(length(uvx + 0.6f * position), 5.5f), 0.0f) * 2.0f;

    uvx = lerp(uv, uvd, -0.5f);
    float f6  = max(0.01f - pow(length(uvx - 0.3f   * position), 1.6f), 0.0f) * 9.0f;
    float f62 = max(0.01f - pow(length(uvx - 0.325f * position), 1.6f), 0.0f) * 6.0f;
    float f63 = max(0.01f - pow(length(uvx - 0.35f  * position), 1.6f), 0.0f) * 7.0f;

    float3 lensFlare = float3(f2 + f4 + f5 + f6, f22 + f42 + f52 + f62, f23 + f43 + f53 + f63);

    // Subtle anamorphic glare. Squares are products: pow of a negative base is NaN, which SPIR-V max does not filter.
    float2 glareOffset = main * float2(1.2f, 35.0f);
    float glare = exp(-dot(glareOffset, glareOffset));
    float3 anamorphicGlare = float3(0.6f, 0.5f, 1.0f) * glare * 0.05f;

    float3 color = saturate(f0) * 0.5f + lensFlare + anamorphicGlare;
    return color * 1.3f - length(uvd) * 0.05f;
}

// Soft visibility of a flare from 9 depth taps around it: sky texels (cleared depth) are visible.
float GetLensFlareVisibility(float2 position)
{
    if (abs(position.x) > 1.0f || abs(position.y) > 1.0f)
        return 1.0f;

    const float2 KERNEL_RADIUS = float2(0.012f, 0.018f);

    float2 flareUV = float2(position.x * 0.5f + 0.5f, 0.5f - position.y * 0.5f);
    float visibility = 0.0f;

    [unroll]
    for (int y = -1; y <= 1; y++)
    {
        [unroll]
        for (int x = -1; x <= 1; x++)
        {
            float depth = DepthMap.SampleLevel(DepthMapSampler, saturate(flareUV + float2(x, y) * KERNEL_RADIUS), 0.0f).x;
            visibility += (depth >= 0.9999f) ? 1.0f : 0.0f;
        }
    }

    return visibility / 9.0f;
}

float3 ApplyLensFlares(float3 color, float2 uv)
{
    float2 position = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
    float3 totalColor = 0.0f;

    for (uint i = 0; i < min(LensFlareCount, LENS_FLARES_MAX); i++)
    {
        // Visibility is the same for every pixel, so occluded flares skip the flare math coherently.
        float visibility = GetLensFlareVisibility(LensFlarePositions[i].xy);
        if (visibility <= 0.0f)
            continue;

        float3 flareColor = max(0.0f, LensFlareColors[i].rgb * 4.5f * GetLensFlare(position, LensFlarePositions[i].xy));

        // Color correction: desaturates bright flares.
        float weight = flareColor.r + flareColor.g + flareColor.b;
        flareColor = lerp(flareColor, weight * 0.5f, weight * 0.1f);

        totalColor += flareColor * visibility;
    }

    return lerp(color, color + totalColor, saturate(dot(totalColor, 0.5f)));
}

// Steps: cinematic bars, distortion, depth of field, glow, lens flares, HDR bloom and tonemap, color scheme and fade,
// tint and gamma.
float4 PS(FullscreenPixelInput input) : SV_Target
{
    float y = input.Position.y / ViewportHeight;
    if (y > 1.0f - CinematicBarsHeight || y < CinematicBarsHeight)
        return float4(0.0f, 0.0f, 0.0f, 1.0f);

    float2 uv = IsDistortionActive ? GetDistortedUV(input.UV) : input.UV;
    float3 color = SceneMap.SampleLevel(SceneMapSampler, uv, 0.0f).rgb;

    // Depth of field: far blur, then near blur on top.
    if (IsDofActive & 1)
    {
        float4 farBlur = FarBlurMap.SampleLevel(FarBlurMapSampler, input.UV, 0.0f);
        color = lerp(color, farBlur.rgb, saturate(farBlur.a));
    }

    if (IsDofActive & 2)
    {
        float4 nearBlur = NearBlurMap.SampleLevel(NearBlurMapSampler, input.UV, 0.0f);
        color = lerp(color, nearBlur.rgb, saturate(nearBlur.a));
    }

    // Glow combine with soft add, or plain add for the HDR scene, whose colors may exceed 1.
    if (IsGlowActive)
    {
        float3 glow = GlowMap.SampleLevel(GlowMapSampler, input.UV, 0.0f).rgb;
        color = IsHdrActive ? (color + glow) : (1.0f - (1.0f - color) * (1.0f - glow));
    }

    if (LensFlareCount > 0)
        color = ApplyLensFlares(color, input.UV);

    if (IsHdrActive)
        color = Tonemap((color + BloomMap.SampleLevel(BloomMapSampler, input.UV, 0.0f).rgb * BloomIntensity) * Exposure);

    color = ApplyColorScheme(color) * ScreenFade;

    // Neutral tint is 0.5, compensated by the color modulation scale.
    color = saturate(pow(saturate(color * Tint * COLOR_MODULATION_SCALE), 1.0f / Gamma));
    return float4(color, 1.0f);
}

