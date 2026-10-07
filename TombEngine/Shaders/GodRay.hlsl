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
// t2: Horizon mesh binary silhouette mask (R channel: 1=opaque, 0=sky).
// t3: Main scene render target (full-res, read-only during pass 1).
//     Contains the rendered underwater sky — used for underwater shaft bright-spot source.
Texture2D    CloudTexture       : register(t0);
Texture2D    CloudTextureB      : register(t1);
Texture2D    HorizonMaskTexture : register(t2);
Texture2D    SceneTexture       : register(t3);
SamplerState LinearSamp         : register(s3);   // LinearClamp sampler

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
    if (GodRaySunScreenPos.x < -5.0f || GodRayAutoStrength < 0.001f)
        return float4(0.0f, 0.0f, 0.0f, 0.0f);

    float  aspect     = GodRayViewSize.x / GodRayViewSize.y;
    float2 uvCentered = (uv - 0.5f) * float2(aspect, 1.0f);

    float2 toSun     = uv - GodRaySunScreenPos;
    float2 marchStep = toSun * clamp(GodRayLength, 0.01f, 1.5f) / (float)GodRaySampleCount;
    float2 sampleUV  = uv + marchStep * GodRayHash(uv);

    float discScale = GodRaySoftness * 8.0f;

    // =========================================================================
    // UNDERWATER PATH — distinct from normal god rays.
    // The cloud RT is empty when UnderwaterSky is active. Instead, sample the
    // main scene RT (t3) which contains the rendered underwater sky. Only the
    // medium-bright glow rim (not the solid white core) acts as the shaft source
    // so rays appear to diverge from the edge of the sun glow, not its center.
    // =========================================================================
    if (GodRayUnderwaterActive > 0.5f)
    {
        // Dedicated march step using underwater-specific length.
        // When the sun is near the zenith (elevation -> 1.0), physically the
        // shafts should be very short because the light travels straight down
        // through the water column. Without this attenuation the rays extend
        // far across the screen, looking unnatural.
        float zenithAtten = 1.0f - smoothstep(0.55f, 0.95f, GodRaySunElevation);
        float lengthScale = lerp(0.22f, 1.0f, zenithAtten);

        float2 toSunUW     = uv - GodRaySunScreenPos;
        float2 marchStepUW = toSunUW * clamp(GodRayUnderwaterRayLength, 0.01f, 1.5f) * lengthScale / (float)GodRayUnderwaterSampleCount;
        float2 sampleUVUW  = uv + marchStepUW * GodRayHash(uv);

        float  accumulated = 0.0f;
        float  w           = 1.0f;

        [loop]
        for (int i = 0; i < GodRayUnderwaterSampleCount; i++)
        {
            sampleUVUW -= marchStepUW;
            float2 cuv = saturate(sampleUVUW);
            float sampleOnScreen = (sampleUVUW.x >= 0.0f && sampleUVUW.x <= 1.0f &&
                                    sampleUVUW.y >= 0.0f && sampleUVUW.y <= 1.0f) ? 1.0f : 0.0f;

            float3 sceneRgb  = SceneTexture.SampleLevel(LinearSamp, cuv, 0).rgb;
            float  sceneLuma = dot(saturate(sceneRgb), float3(0.299f, 0.587f, 0.114f));

            // Natural luminance source: scene brightness drives ray emission.
            // Sharpness controls contrast — higher values suppress dim pixels more.
            float source = pow(sceneLuma, max(0.5f, GodRayUnderwaterSharpness));
            source = saturate(source * GodRayUnderwaterBrightness);

            accumulated += source * sampleOnScreen * w;
            w *= GodRayUnderwaterRayDecay;
        }

        accumulated /= (float)GodRayUnderwaterSampleCount;
        float3 col = GodRaySunColor * accumulated * GodRayUnderwaterRayIntensity * GodRayAutoStrength;
        col *= smoothstep(1.2f, 0.6f, length(uvCentered));
        return float4(sqrt(saturate(col)), 0.0f);
    }

    // =========================================================================
    // NORMAL PATH — unchanged from before underwater sky was added.
    // Uses cloud RT occlusion mask + virtual sun disc.
    // =========================================================================

    float2 sunEdgeDist    = max(abs(GodRaySunScreenPos - 0.5f) - 0.5f, 0.0f);
    float  sunOnScreenFade = 1.0f - saturate(max(sunEdgeDist.x, sunEdgeDist.y) * 6.0f);
    float2 sunUV           = saturate(GodRaySunScreenPos);
    float4 sunCloudSampleA = CloudTexture.SampleLevel(LinearSamp, sunUV, 0);
    float4 sunCloudSampleB = CloudTextureB.SampleLevel(LinearSamp, sunUV, 0);
    float  sunCloudA       = sunCloudSampleA.a;
    float  sunCloudB       = sunCloudSampleB.a;
    float  sunHorizonMask  = HorizonMaskTexture.SampleLevel(LinearSamp, sunUV, 0).r;
    float  sunOcclusion    = saturate(max(sunCloudA, sunCloudB) + sunHorizonMask);
    float3 sunCloudRGB        = (sunCloudA >= sunCloudB) ? sunCloudSampleA.rgb : sunCloudSampleB.rgb;
    float  sunCloudLuma       = dot(saturate(sunCloudRGB), float3(0.299f, 0.587f, 0.114f));
    float  cloudPresenceGate  = saturate(sunOcclusion * 20.0f);
    float  darkUndersideBoost = saturate(1.0f - sunCloudLuma / 0.5f) * cloudPresenceGate;
    float sunsetFac       = saturate(1.0f - GodRaySunElevation / 0.25f);
    float godRayOccAggr   = lerp(1.0f, 8.0f, max(sunsetFac, darkUndersideBoost));
    float sunOccludeInput = saturate(sunOcclusion * godRayOccAggr);
    float  occludeT   = smoothstep(0.2f, 0.55f, sunOccludeInput);
    occludeT          = occludeT * occludeT * occludeT;
    float  sunDiscVis = sunOnScreenFade * (1.0f - occludeT);

    float  accumulated = 0.0f;
    float  w           = 1.0f;

    [loop]
    for (int i = 0; i < GodRaySampleCount; i++)
    {
        sampleUV -= marchStep;
        float2 cuv = saturate(sampleUV);
        float sampleOnScreen = (sampleUV.x >= 0.0f && sampleUV.x <= 1.0f &&
                                sampleUV.y >= 0.0f && sampleUV.y <= 1.0f) ? 1.0f : 0.0f;

        float cloudAlphaA  = CloudTexture.SampleLevel(LinearSamp, cuv, 0).a;
        float cloudAlphaB  = CloudTextureB.SampleLevel(LinearSamp, cuv, 0).a;
        float horizonMask  = HorizonMaskTexture.SampleLevel(LinearSamp, cuv, 0).r;
        float cloudOpacity = saturate(max(cloudAlphaA, cloudAlphaB) + horizonMask);
        float cloudGap     = smoothstep(0.05f, 0.35f, 1.0f - cloudOpacity);

        float sunDist = distance(cuv, GodRaySunScreenPos);
        float sunDisc = exp(-sunDist * discScale) * sunDiscVis;

        float sceneBrightness = saturate(cloudGap + sunDisc) * sampleOnScreen;
        accumulated += sceneBrightness * w;
        w *= GodRayDecay;
    }

    accumulated /= (float)GodRaySampleCount;
    float3 col = GodRaySunColor * accumulated * GodRayIntensity * GodRayAutoStrength;
    col *= smoothstep(1.2f, 0.6f, length(uvCentered));
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
