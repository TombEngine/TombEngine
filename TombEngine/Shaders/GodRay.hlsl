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

    // Early out: sun off-screen or automatic strength is zero.
    if (GodRaySunScreenPos.x < -0.5f || GodRayAutoStrength < 0.001f)
        return float4(0.0f, 0.0f, 0.0f, 0.0f);

    // Aspect-corrected centered UV — used only for the screen-edge vignette.
    float  aspect     = GodRayViewSize.x / GodRayViewSize.y;
    float2 uvCentered = (uv - 0.5f) * float2(aspect, 1.0f);

    // Step vector: each iteration moves sampleUV one step toward the sun.
    // After GodRaySampleCount steps we reach ~99% of the way to the sun.
    float2 toSun     = uv - GodRaySunScreenPos;   // points AWAY from sun
    float2 marchStep = toSun * 0.99f / (float)GodRaySampleCount;

    // Jitter the starting UV by up to one step to break banding.
    float2 sampleUV = uv + marchStep * GodRayHash(uv);

    // Sun-disc falloff: scale so that the disc is meaningfully wide.
    // GodRaySoftness=1 gives radius ~0.125 UV, =3 gives radius ~0.04 UV.
    float discScale = GodRaySoftness * 8.0f;

    float  accumulated = 0.0f;
    float  w           = 1.0f;           // weight starts full, decays as we move toward sun

    [loop]
    for (int i = 0; i < GodRaySampleCount; i++)
    {
        sampleUV -= marchStep;                         // step toward sun
        float2 cuv = saturate(sampleUV);

        // Cloud opacity: 0 = clear sky gap (light passes), 1 = dense cloud (blocked).
        float cloudAlphaA  = CloudTexture.SampleLevel(LinearSamp, cuv, 0).a;
        float cloudAlphaB  = CloudTextureB.SampleLevel(LinearSamp, cuv, 0).a;
        float cloudOpacity = saturate(max(cloudAlphaA, cloudAlphaB));

        // Sky gap: 1 in clear sky, 0 inside cloud bodies.
        // This is the "scene brightness" that the Shadertoy samples from iChannel0:
        //   bright sky in gaps, dark where clouds block.
        float cloudGap = 1.0f - cloudOpacity;

        // Virtual sun disc at its projected screen position.
        // Adds extra brightness exactly at the light source so rays converge there.
        float sunDist = distance(cuv, GodRaySunScreenPos);
        float sunDisc = exp(-sunDist * discScale);

        // Scene brightness = sky gap fill + sun disc.
        // cloudGap handles shaft shadows; sunDisc adds the bright anchor at the source.
        // saturate keeps values in [0,1] for numerical stability.
        float sceneBrightness = saturate(cloudGap + sunDisc);

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
