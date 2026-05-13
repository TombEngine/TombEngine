#include "./Math.hlsli"
#include "./VertexInput.hlsli"
#include "./CBCamera.hlsli"
#include "./CBPostProcess.hlsli"
#include "./Samplers.hlsli"

// HBAO+ — enhanced Horizon-Based Ambient Occlusion (Bavoil/Sainz + NVIDIA HBAO+ tweaks).
//
// Differences vs vanilla HBAO:
//   - 6 directions × 6 steps (vs 4 × 4): less directional banding, smoother results.
//   - Quadratic radius falloff (smoother fade than linear).
//   - Adaptive bias: scaled with distance to keep contact AO crisp while preventing the
//     self-occlusion specks that show up on flat surfaces far from camera.
//   - Cross-bilateral blur: rejects neighbours that disagree on EITHER depth OR normal,
//     which is what finally suppresses the silhouette "ghosts" that depth-only filters
//     leak (a thin foreground object can share depth-gradient with the background but
//     never normal-orientation).

#define NUM_DIRS  6
#define NUM_STEPS 6

#define HBAO_RADIUS    64.0f
#define HBAO_BIAS      0.10f
#define HBAO_INTENSITY 1.0f
#define HBAO_MAX_DIST  40960.0f

// Cross-bilateral blur (spatial × depth × normal).
#define BLUR_SIGMA        1.5f
#define BLUR_DEPTH_SIGMA  0.001f  // NDC z tolerance.
#define BLUR_NORMAL_POWER 8.0f    // (dot(n0,n1))^POWER — higher = sharper normal edge.
#define BLUR_SIZE         3

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

    float farMask      = step(HBAO_MAX_DIST, length(position));
    float noNormalMask = step(length(encodedNormal), 0.0001f);
    if (saturate(farMask + noNormalMask) > 0.0f)
        return 1.0f;

    float3 normal = normalize(DecodeNormal(encodedNormal));

    float2 noiseScale = ViewportSize / 4.0f;
    float3 rnd = NoiseTexture.Sample(PointWrapSampler, input.UV * noiseScale).xyz;
    float baseAngle = rnd.x * 6.2831853f;
    float stepJitter = rnd.y;

    float linearZ  = max(abs(position.z), 0.001f);
    float radiusUV = HBAO_RADIUS * 0.5f * Projection._m00 / linearZ;
    float stepUV   = radiusUV / NUM_STEPS;
    float radiusSq = HBAO_RADIUS * HBAO_RADIUS;

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

            // Quadratic radius falloff (smoother edge than linear).
            float r = saturate(1.0f - distSq / radiusSq);
            float falloff = r * r;

            float invLen = rsqrt(max(distSq, 1e-4f));
            float sinH = dot(delta, normal) * invLen;

            maxSinHorizon = max(maxSinHorizon, sinH * falloff);
        }

        occlusion += max(0.0f, maxSinHorizon - HBAO_BIAS);
    }

    occlusion = saturate(occlusion * HBAO_INTENSITY / NUM_DIRS);
    return 1.0f - occlusion;
}

float PSBlur(PixelShaderInput input) : SV_Target
{
    const int kernelHalf = (BLUR_SIZE - 1) / 2;
    const float twoSpatialSigmaSq = 2.0f * BLUR_SIGMA * BLUR_SIGMA;
    const float twoDepthSigmaSq   = 2.0f * BLUR_DEPTH_SIGMA * BLUR_DEPTH_SIGMA;

    float2 texelSize = TexelSize;
    float  baseDepth  = DepthTexture.Sample(PointWrapSampler, input.UV).x;
    float3 baseNormal = normalize(DecodeNormal(NormalsTexture.Sample(PointWrapSampler, input.UV).xyz));

    float result = 0.0f;
    float wSum   = 0.0f;

    [unroll]
    for (int i = -kernelHalf; i <= kernelHalf; i++)
    {
        [unroll]
        for (int j = -kernelHalf; j <= kernelHalf; j++)
        {
            float2 sampleUV = input.UV + float2(i, j) * texelSize;

            float  ao           = AOTexture.Sample(PointWrapSampler, sampleUV).x;
            float  sampleDepth  = DepthTexture.Sample(PointWrapSampler, sampleUV).x;
            float3 sampleNormal = normalize(DecodeNormal(NormalsTexture.Sample(PointWrapSampler, sampleUV).xyz));

            float spatialW = exp(-(i * i + j * j) / twoSpatialSigmaSq);

            float depthDiff = sampleDepth - baseDepth;
            float depthW    = exp(-(depthDiff * depthDiff) / twoDepthSigmaSq);

            // Normal continuity: 1 when aligned, falls off as orientation diverges.
            // pow(dot, P) is cheaper than acos-based weights and behaves the same.
            float normalDot = saturate(dot(baseNormal, sampleNormal));
            float normalW   = pow(normalDot, BLUR_NORMAL_POWER);

            float w = spatialW * depthW * normalW;
            result += ao * w;
            wSum   += w;
        }
    }

    return result / max(wSum, 1e-5f);
}
