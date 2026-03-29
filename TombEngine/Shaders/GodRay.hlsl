// ============================================================================
// GodRay.hlsl — Screen-space god ray / light shaft shader.
//
// Algorithm (adapted from Shadertoy radial-blur approach):
//   For every pixel, march 25 steps from the pixel toward the sun in UV space.
//   At each step the cloud RT is sampled for scattered light.  A virtual sun
//   disc (exp falloff at GodRaySunScreenPos) is added so rays are visible even
//   when the cloud RT has no bright pixel at the sun position.
//   Any step whose weighted sample exceeds a brightness threshold accumulates
//   a warm sun-coloured contribution.  The result is gamma-encoded and
//   additively composited over the main scene.
//
// Entry points:
//   VSGodRay          — fullscreen-triangle vertex shader
//   PSGodRay          — radial light-shaft pixel shader (renders to half-res)
//   PSGodRayComposite — upscale and additively blend over the main scene
// ============================================================================

#include "./CBGodRay.hlsli"

// ---------------------------------------------------------------------------
// Vertex layout (same as PostProcess / VolumetricClouds fullscreen triangle)
// ---------------------------------------------------------------------------

struct VSInput
{
    float3 Position : POSITION0;
    float2 UV       : TEXCOORD0;
    float4 Color    : COLOR0;
};

struct VSOutput
{
    float4 Position     : SV_POSITION;
    float2 UV           : TEXCOORD0;
    float4 PositionCopy : TEXCOORD1;
};

// ---------------------------------------------------------------------------
// Textures
// ---------------------------------------------------------------------------

// t0: Cloud render target layer A (half-res RGBA).  RGB = lit cloud colour.
// t1: Cloud render target layer B (half-res RGBA).  RGB = lit cloud colour.
//     Bound by RendererGodRay.cpp before the god ray pass.
Texture2D    CloudTexture  : register(t0);
Texture2D    CloudTextureB : register(t1);
SamplerState LinearSamp    : register(s3);   // LinearClamp sampler

// ---------------------------------------------------------------------------
// Vertex shader — fullscreen triangle
// ---------------------------------------------------------------------------

VSOutput VSGodRay(VSInput input)
{
    VSOutput output;
    output.Position     = float4(input.Position, 1.0f);
    output.UV           = input.UV;
    output.PositionCopy = output.Position;
    return output;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// UV-space hash for per-pixel jitter (ported from the GLSL original).
float GodRayHash(float2 p)
{
    return frac(sin(dot(p, float2(41.0f, 289.0f))) * 45758.5453f);
}

// ---------------------------------------------------------------------------
// PSGodRay — radial light-shaft accumulation (half-res pass)
// ---------------------------------------------------------------------------

float4 PSGodRay(VSOutput input) : SV_TARGET
{
    float2 uv = input.UV;

    // Early out: sun behind the camera (sentinel -10) or automatic strength is zero.
    // Off-screen-but-in-front suns (UV outside [0,1]) still produce rays from the screen edge.
    if (GodRaySunScreenPos.x < -5.0f || GodRayAutoStrength < 0.001f)
        return float4(0.0f, 0.0f, 0.0f, 0.0f);

    // Aspect-corrected centered UV — used only for the screen-edge vignette.
    float  aspect     = GodRayViewSize.x / GodRayViewSize.y;
    float2 uvCentered = (uv - 0.5f) * float2(aspect, 1.0f);

    // Step vector: each iteration moves sampleUV one step toward the sun.
    // GodRayLength controls how far the march reaches (1.0 = all the way to the sun,
    // 0.5 = half way, giving shorter/tighter shafts).
    float2 toSun     = uv - GodRaySunScreenPos;   // points AWAY from sun
    float2 marchStep = toSun * clamp(GodRayLength, 0.01f, 1.5f) / (float)GodRaySampleCount;

    // Jitter the starting UV by up to one step to break banding.
    float2 sampleUV = uv + marchStep * GodRayHash(uv);

    // Sun-disc falloff: scale so that the disc is meaningfully wide.
    // GodRaySoftness=1 gives radius ~0.125 UV, =3 gives radius ~0.04 UV.
    float discScale = GodRaySoftness * 8.0f;

    // Cloud occlusion AT the sun position: sample both layers at the projected sun UV.
    // Clamp to [0,1] so off-screen sun positions sample the nearest screen edge instead
    // of wrapping/clamping to garbage. A value of 1 means dense cloud in front of the sun.
    // Sun disc occlusion query — only valid when the sun is actually on-screen.
    // saturate(GodRaySunScreenPos) for an off-screen sun clamps to the nearest screen
    // edge, which may be clear sky, setting sunDiscVis=1 and burning a false bright
    // source onto the screen edge. Fade sunDiscVis to zero as the sun exits the border.
    float2 sunEdgeDist    = max(abs(GodRaySunScreenPos - 0.5f) - 0.5f, 0.0f);
    float  sunOnScreenFade = 1.0f - saturate(max(sunEdgeDist.x, sunEdgeDist.y) * 6.0f);
    float2 sunUV           = saturate(GodRaySunScreenPos);
    float4 sunCloudSampleA = CloudTexture.SampleLevel(LinearSamp, sunUV, 0);
    float4 sunCloudSampleB = CloudTextureB.SampleLevel(LinearSamp, sunUV, 0);
    float  sunCloudA       = sunCloudSampleA.a;
    float  sunCloudB       = sunCloudSampleB.a;
    float  sunOcclusion    = saturate(max(sunCloudA, sunCloudB));
    // Dark-underside detection: alto second color produces low-luma RGB in the cloud RT.
    // Sample the dominant layer's color at the sun UV and compute luminance.
    // A presence gate (alpha > ~0.05) prevents empty/black sky from triggering the boost.
    // Luma < 0.5 → dark face → boost aggressiveness up to 8x so even screen-blend
    // dark undersides hide the sun disc, independent of the sunset-elevation ramp.
    float3 sunCloudRGB        = (sunCloudA >= sunCloudB) ? sunCloudSampleA.rgb : sunCloudSampleB.rgb;
    float  sunCloudLuma       = dot(saturate(sunCloudRGB), float3(0.299f, 0.587f, 0.114f));
    float  cloudPresenceGate  = saturate(sunOcclusion * 20.0f);
    float  darkUndersideBoost = saturate(1.0f - sunCloudLuma / 0.5f) * cloudPresenceGate;
    // Sunset factor: 0 when sun is high, 1 when sun is at/below horizon (elevation<0.25).
    // Day:    disc fades when cloud alpha > 0.20, fully gone at 0.55.
    // Sunset: disc fades when cloud alpha > 0.05, fully gone at 0.25.
    //         (cloudAlpha*aggressiveness replaces the direct smoothstep input.)
    float sunsetFac       = saturate(1.0f - GodRaySunElevation / 0.25f);
    float godRayOccAggr   = lerp(1.0f, 8.0f, max(sunsetFac, darkUndersideBoost));
    float sunOccludeInput = saturate(sunOcclusion * godRayOccAggr);
    // Ease-in cubic: sun stays nearly fully visible until clouds are dense,
    // then drops sharply over a short window to full occlusion.
    float  occludeT   = smoothstep(0.2f, 0.55f, sunOccludeInput);
    occludeT          = occludeT * occludeT * occludeT;
    float  sunDiscVis = sunOnScreenFade * (1.0f - occludeT);

    float  accumulated = 0.0f;
    float  w           = 1.0f;           // weight starts full, decays as we move toward sun

    [loop]
    for (int i = 0; i < GodRaySampleCount; i++)
    {
        sampleUV -= marchStep;                         // step toward sun
        float2 cuv = saturate(sampleUV);
        // Skip samples that have marched off the screen boundary. The saturate() above
        // would clamp them to the nearest screen edge (possibly clear sky), inflating
        // the accumulated brightness when the sun source is off-screen.
        float sampleOnScreen = (sampleUV.x >= 0.0f && sampleUV.x <= 1.0f &&
                                sampleUV.y >= 0.0f && sampleUV.y <= 1.0f) ? 1.0f : 0.0f;

        // Cloud opacity: 0 = clear sky gap (light passes), 1 = dense cloud (blocked).
        float cloudAlphaA  = CloudTexture.SampleLevel(LinearSamp, cuv, 0).a;
        float cloudAlphaB  = CloudTextureB.SampleLevel(LinearSamp, cuv, 0).a;
        float cloudOpacity = saturate(max(cloudAlphaA, cloudAlphaB));

        // Sky gap: 1 in clear sky, 0 inside cloud bodies.
        // Apply a soft step so near-opaque cloud areas (cloudGap ~0.05-0.15) that occur
        // even at AltoThickness 4000+ contribute almost nothing, while real sky gaps
        // (cloudGap ~0.5+) contribute fully. This eliminates the faint leak without
        // affecting rays through genuine gaps in the cloud cover.
        float cloudGap = smoothstep(0.05f, 0.35f, 1.0f - cloudOpacity);

        // Virtual sun disc, attenuated by cloud cover at the sun position.
        // When thick storm clouds sit over the sun, sunDiscVis → 0 and the disc vanishes.
        float sunDist = distance(cuv, GodRaySunScreenPos);
        float sunDisc = exp(-sunDist * discScale) * sunDiscVis;

        // Scene brightness = sky gap fill + sun disc.
        float sceneBrightness = saturate(cloudGap + sunDisc) * sampleOnScreen;

        accumulated += sceneBrightness * w;

        w *= GodRayDecay;
    }

    // Normalise by sample count so the result is independent of SampleCount.
    accumulated /= (float)GodRaySampleCount;

    // Tint, scale and modulate by automatic strength.
    float3 col = GodRaySunColor * accumulated * GodRayIntensity * GodRayAutoStrength;

    // Screen-edge vignette: gracefully fades rays near the frame border.
    col *= smoothstep(1.2f, 0.6f, length(uvCentered));

    // Gamma-encode before additive blend so thin rays stay visible.
    return float4(sqrt(saturate(col)), 0.0f);
}

// ---------------------------------------------------------------------------
// PSGodRayComposite — blend half-res god rays onto the full-res scene
// ---------------------------------------------------------------------------
// Uses additive BlendMode on the C++ side (One + One).
// t0 is rebound to the half-res god ray render target before this pass.

float4 PSGodRayComposite(VSOutput input) : SV_TARGET
{
    float3 godRay = CloudTexture.Sample(LinearSamp, input.UV).rgb;
    // alpha=1 is required: BlendMode::Additive uses SrcBlend=SRC_ALPHA (DirectXTK CommonStates).
    // With alpha=0 the entire contribution would be multiplied to zero and nothing would appear.
    return float4(godRay, 1.0f);
}
