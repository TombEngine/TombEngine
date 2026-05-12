#include "./Math.hlsli"
#include "./VertexInput.hlsli"
#include "./CBCamera.hlsli"
#include "./CBPostProcess.hlsli"
#include "./Samplers.hlsli"

// Horizon-Based Ambient Occlusion (Bavoil & Sainz, 2008).
//
// For each pixel we cast NUM_DIRS rays in screen-space (rotated per-pixel by a 4x4 random
// texture) and along each ray we march NUM_STEPS samples. For every sample we measure the
// elevation angle of the neighbour above the tangent plane defined by the pixel normal.
// The maximum elevation (the "horizon") drives the occlusion contribution along that ray.
//
// Compared to the previous Crytek-style SSAO (64 random hemisphere samples + bilateral blur)
// this is ~4x cheaper (16 taps), exhibits less salt-and-pepper noise, and reacts to local
// geometry curvature instead of plain proximity, producing crisper contact shadows.

#define NUM_DIRS  4
#define NUM_STEPS 4

#define HBAO_RADIUS    64.0f  // World-space sphere of influence.
#define HBAO_BIAS      0.10f  // Sin of minimum considered horizon angle (avoids self-occlusion).
#define HBAO_INTENSITY 1.5f   // Final occlusion scale.
#define HBAO_MAX_DIST  40960.0f

#define SIGMA  3.0
#define BSIGMA 0.3
#define MSIZE  5

struct PixelShaderInput
{
    float4 Position : SV_POSITION;
    float2 UV       : TEXCOORD0;
};

Texture2D DepthTexture   : register(t0);
Texture2D NormalsTexture : register(t1);
Texture2D NoiseTexture   : register(t2);
Texture2D AOTexture      : register(t9);

float3 DecodeNormal(float3 n)
{
    return n * 2.0f - 1.0f;
}

float3 ReconstructPositionFromDepth(float2 uv)
{
    float x = uv.x * 2.0f - 1.0f;
    float y = (1.0f - uv.y) * 2.0f - 1.0f;
    float z = DepthTexture.Sample(PointWrapSampler, uv).x;

    float4 projectedPosition = float4(x, y, z, 1.0f);
    float4 position = mul(projectedPosition, InverseProjection);

    return position.xyz / position.w;
}

float PS(PixelShaderInput input) : SV_Target
{
    float3 position = ReconstructPositionFromDepth(input.UV);
    float3 encodedNormal = NormalsTexture.Sample(PointWrapSampler, input.UV).xyz;

    // Early exit on sky / out-of-range fragments.
    float farMask      = step(HBAO_MAX_DIST, length(position));
    float noNormalMask = step(length(encodedNormal), 0.0001f);
    if (saturate(farMask + noNormalMask) > 0.0f)
        return 1.0f;

    float3 normal = normalize(DecodeNormal(encodedNormal));

    // Random rotation read from the 4x4 noise (tiled across the screen). We only use the
    // xy components: a rotation seed for the ray fan and a jitter for marching step length.
    float2 noiseScale = ViewportSize / 4.0f;
    float3 rnd = NoiseTexture.Sample(PointWrapSampler, input.UV * noiseScale).xyz;
    float baseAngle = rnd.x * 6.2831853f;
    float stepJitter = rnd.y;

    // World-space radius → screen-space (UV) radius using the projection focal length and
    // current view-space depth. abs() guards against the small positive Z that EnsureNormal
    // would otherwise produce near the near plane.
    float linearZ = max(abs(position.z), 0.001f);
    float radiusUV = HBAO_RADIUS * 0.5f * Projection._m00 / linearZ;
    float stepUV   = radiusUV / NUM_STEPS;

    float occlusion = 0.0f;

    [unroll]
    for (int d = 0; d < NUM_DIRS; ++d)
    {
        float angle = baseAngle + (6.2831853f * d / NUM_DIRS);
        float2 dir = float2(cos(angle), sin(angle));

        float maxSinHorizon = HBAO_BIAS;

        [unroll]
        for (int s = 1; s <= NUM_STEPS; ++s)
        {
            float2 sampleUV = input.UV + dir * stepUV * (s + stepJitter);
            float3 samplePos = ReconstructPositionFromDepth(sampleUV);

            float3 delta = samplePos - position;
            float distSq = dot(delta, delta);
            float invLen = rsqrt(max(distSq, 1e-4f));

            // sin(horizon) = (delta . normal) / |delta|.
            float sinH = dot(delta, normal) * invLen;

            // Distance falloff: linear over r^2.
            float falloff = saturate(1.0f - distSq / (HBAO_RADIUS * HBAO_RADIUS));

            // Track weighted horizon. Falloff modulates the contribution so distant samples
            // never push the horizon up beyond their relevance.
            maxSinHorizon = max(maxSinHorizon, sinH * falloff);
        }

        occlusion += max(0.0f, maxSinHorizon - HBAO_BIAS);
    }

    occlusion = saturate(occlusion * HBAO_INTENSITY / NUM_DIRS);

    return 1.0f - occlusion;
}

float normpdf(float x, float sigma)
{
    return 0.39894 * exp(-0.5 * x * x / (sigma * sigma)) / sigma;
}

float PSBlur(PixelShaderInput input) : SV_Target
{
    float2 texelSize = TexelSize;
    float result = 0.0f;

    const int kernelSize = (MSIZE - 1) / 2;
    float kernel[MSIZE];
    float bZ = 0.0;

    for (int j = 0; j <= kernelSize; j++)
    {
        kernel[kernelSize + j] = kernel[kernelSize - j] = normpdf(float(j), SIGMA);
    }

    float color;
    float baseColor = AOTexture.Sample(PointWrapSampler, input.UV).x;
    float gfactor;
    float bfactor;
    float bZnorm = 1.0 / normpdf(0.0, BSIGMA);

    for (int i = -kernelSize; i <= kernelSize; i++)
    {
        for (int j = -kernelSize; j <= kernelSize; j++)
        {
            float2 offset = float2(i, j) * texelSize;
            color = AOTexture.Sample(PointWrapSampler, input.UV + offset).x;

            gfactor = kernel[kernelSize + j] * kernel[kernelSize + i];
            bfactor = normpdf(color - baseColor, BSIGMA) * bZnorm * gfactor;
            bZ += bfactor;

            result += bfactor * color;
        }
    }

    return (result / bZ);
}
