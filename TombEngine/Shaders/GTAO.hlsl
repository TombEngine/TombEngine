// TEN_SHADER: Vertex VS samplers=0 storage_textures=0 storage_buffers=0 uniform_buffers=0
// TEN_SHADER: Pixel PS samplers=2 storage_textures=0 storage_buffers=0 uniform_buffers=3
// TEN_SHADER: Pixel PSTemporal samplers=4 storage_textures=0 storage_buffers=0 uniform_buffers=3

// Ground truth ambient occlusion (GTAO, Jimenez et al. 2016) at half resolution (SceneRendererSSAO.cpp), the "GTAO" pass
// after the prepass. PS searches the horizons of one screen space slice per texel in the prepass depth and integrates
// the cosine weighted visibility between them; the slice angles and step offsets change every frame. PSTemporal blends
// the result with the history reprojected through the prepass motion vectors (or the camera motion without TAA), which accumulates many slices over a
// few frames, filtered with a bilateral 3x3 average in the same pass. 1 is unoccluded.

#include "./ShaderInterop.hlsli"
#include "./Fullscreen.hlsli"

#define GTAO_SLICE_COUNT 1 // Per frame: the temporal accumulation gathers the slices of several frames.
#define GTAO_STEP_COUNT  4
#define GTAO_PI          3.14159265f
#define GTAO_HALF_PI     1.57079633f
#define BLUR_SIGMA       3.0f
#define BLUR_BSIGMA      0.3f

// Slot 2, so the frame uniforms in slots 0 and 1 stay valid for the scene pass.
TEN_PS_UNIFORMS(AmbientOcclusionUniforms, 2)
{
    float4x4 Projection;
    float4x4 InverseProjection;
    float4x4 ReprojectionMatrix; // Current clip space to the previous frame, when there are no motion vectors.
    float2   TargetSize;      // Half resolution occlusion target.
    float2   TexelSize;
    float2   DepthTexelSize;  // Texel of the full resolution depth.
    float    Radius;          // World units.
    float    MaxScreenRadius; // UV units: bounds the search of near surfaces.
    float    FrameNoise;      // Changes the slice angles and step offsets every frame.
    float    HistoryWeight;   // Share of the history in the temporal blend.
    uint     IsHistoryValid;
    uint     HasVelocity;     // The prepass wrote motion vectors (TAA); otherwise the camera motion reprojects.

};

// Slot 0: depth for PS, the raw occlusion for PSTemporal, the accumulated occlusion for the blur passes.
TEN_PS_TEXTURE(Texture2D, SourceMap, 0);
// Slot 1: view space normals for PS, the occlusion history for PSTemporal.
TEN_PS_TEXTURE(Texture2D, AuxMap, 1);
TEN_PS_TEXTURE(Texture2D, VelocityMap, 2); // Prepass motion in UV units, full resolution.
TEN_PS_TEXTURE(Texture2D, DepthMap, 3);    // Prepass depth copy, full resolution.

typedef FullscreenPixelInput PixelInput;

PixelInput VS(uint vertexId : SV_VertexID)
{
    return FullscreenVS(vertexId);
}

float3 ReconstructViewPosition(float2 uv)
{
    float depth = SourceMap.SampleLevel(SourceMapSampler, uv, 0.0f).x;
    float4 position = mul(float4(uv.x * 2.0f - 1.0f, (1.0f - uv.y) * 2.0f - 1.0f, depth, 1.0f), InverseProjection);
    return position.xyz / position.w;
}

// The occlusion target has half the depth resolution: position and normal come from the center of one depth texel,
// otherwise slanted surfaces occlude themselves in stripes.
float2 SnapToDepthTexel(float2 uv)
{
    return (floor(uv / DepthTexelSize) + 0.5f) * DepthTexelSize;
}

// Geometric normal from the depth of the neighboring texels, on each axis from the side with the smaller depth step, so
// edges keep the normal of their own surface. Normal maps would tilt the hemisphere below the surface, which the
// horizons then occlude uniformly.
float3 ReconstructViewNormal(float2 uv, float3 position, float2 depthTexel)
{
    float3 right = ReconstructViewPosition(uv + float2(depthTexel.x, 0.0f)) - position;
    float3 left = position - ReconstructViewPosition(uv - float2(depthTexel.x, 0.0f));
    float3 down = ReconstructViewPosition(uv + float2(0.0f, depthTexel.y)) - position;
    float3 up = position - ReconstructViewPosition(uv - float2(0.0f, depthTexel.y));

    float3 horizontal = (abs(right.z) < abs(left.z)) ? right : left;
    float3 vertical = (abs(down.z) < abs(up.z)) ? down : up;
    float3 normal = normalize(cross(vertical, horizontal));
    return (dot(normal, -position) < 0.0f) ? -normal : normal;
}

// Interleaved gradient noise (Jimenez 2014): a per texel value in [0, 1) without a noise texture.
float GetNoise(float2 pixel)
{
    return frac(52.9829189f * frac(dot(pixel, float2(0.06711056f, 0.00583715f))));
}

// Cosine of the highest horizon along one side of a slice. Samples fade to the lowest horizon with their distance, so
// distant geometry does not occlude.
float GetHorizonCosine(float2 uv, float2 direction, float3 position, float3 viewVector, float stepOffset)
{
    float horizonCosine = -1.0f;

    [unroll]
    for (int i = 0; i < GTAO_STEP_COUNT; i++)
    {
        // Samples sit on depth texel centers: a depth placed at another position would leave planes and occlude them.
        float2 sampleUV = SnapToDepthTexel(uv + direction * ((i + stepOffset) / GTAO_STEP_COUNT));
        float3 delta = ReconstructViewPosition(sampleUV) - position;
        float distanceSquared = dot(delta, delta);

        // A sample on the texel of the center has no direction.
        if (distanceSquared < 1.0f)
            continue;

        float sampleCosine = dot(delta * rsqrt(max(distanceSquared, 0.0001f)), viewVector);
        float falloff = saturate(1.0f - distanceSquared / (Radius * Radius));
        horizonCosine = max(horizonCosine, lerp(-1.0f, sampleCosine, falloff));
    }

    return horizonCosine;
}

float PS(PixelInput input) : SV_Target
{
    float2 uv = SnapToDepthTexel(input.UV);
    float3 position = ReconstructViewPosition(uv);
    float3 encodedNormal = AuxMap.SampleLevel(AuxMapSampler, uv, 0.0f).xyz;

    // Sky and far geometry have no occlusion.
    if (length(position) >= 40960.0f || length(encodedNormal) <= 0.0001f)
        return 1.0f;

    float3 normal = ReconstructViewNormal(uv, position, DepthTexelSize);
    float3 viewVector = normalize(-position);

    // Screen radius of the world radius at this depth, in UV units.
    float2 screenRadius = Radius * 0.5f * float2(Projection._11, Projection._22) / max(abs(position.z), 1.0f);
    screenRadius = min(screenRadius, MaxScreenRadius);

    float noise = GetNoise(input.Position.xy + FrameNoise * 64.0f);
    float stepOffset = frac(noise + FrameNoise * 0.61803399f);
    float visibility = 0.0f;

    [unroll]
    for (int slice = 0; slice < GTAO_SLICE_COUNT; slice++)
    {
        // Slice direction on screen (UV y down) and in view space (y up).
        float angle = (slice + noise) * (GTAO_PI / GTAO_SLICE_COUNT);
        float2 screenDirection = float2(cos(angle), sin(angle));
        float3 direction = float3(screenDirection.x, -screenDirection.y, 0.0f);

        // Normal projected on the slice plane, and its angle from the view vector within the plane.
        float3 orthoDirection = direction - dot(direction, viewVector) * viewVector;
        float3 axis = normalize(cross(direction, viewVector));
        float3 projectedNormal = normal - axis * dot(normal, axis);
        float projectedLength = length(projectedNormal);
        float cosN = saturate(dot(projectedNormal, viewVector) / max(projectedLength, 0.0001f));
        float n = sign(dot(orthoDirection, projectedNormal)) * acos(cosN);

        // Horizons on both sides, clamped to the hemisphere of the normal.
        float2 offset = screenDirection * screenRadius;
        float h0 = -acos(GetHorizonCosine(uv, -offset, position, viewVector, stepOffset));
        float h1 = acos(GetHorizonCosine(uv, offset, position, viewVector, stepOffset));
        h0 = n + max(h0 - n, -GTAO_HALF_PI);
        h1 = n + min(h1 - n, GTAO_HALF_PI);

        // Cosine weighted visibility of the arc between the horizons.
        float sinN = sin(n);
        float arc0 = -cos(2.0f * h0 - n) + cosN + 2.0f * h0 * sinN;
        float arc1 = -cos(2.0f * h1 - n) + cosN + 2.0f * h1 * sinN;
        visibility += projectedLength * 0.25f * (arc0 + arc1);
    }

    // A single slice can exceed 1 on slanted surfaces, where only the average over the slice angles is 1: the raw target
    // and the history are half float, and the accumulated value is clamped by the 8 bit occlusion target.
    return max(visibility / GTAO_SLICE_COUNT, 0.0f);
}

float GetGaussian(float x, float sigma)
{
    return 0.39894f * exp(-0.5f * x * x / (sigma * sigma)) / sigma;
}

// Spatial and temporal filtering in one pass at half resolution. The 3x3 neighborhood of the raw occlusion is averaged
// with a bilateral weight on the occlusion difference, which keeps edges sharp, and its range clamps the history,
// widened because a single slice is noisy, so disocclusions do not keep old occlusion. The result is the new history,
// which the scene pass samples directly.
float PSTemporal(PixelInput input) : SV_Target
{
    float current = SourceMap.SampleLevel(SourceMapSampler, input.UV, 0.0f).x;
    float normalization = 1.0f / GetGaussian(0.0f, BLUR_BSIGMA);

    float filtered = 0.0f;
    float weightSum = 0.0f;
    float minimum = current;
    float maximum = current;

    [unroll]
    for (int y = -1; y <= 1; y++)
    {
        [unroll]
        for (int x = -1; x <= 1; x++)
        {
            float neighbor = SourceMap.SampleLevel(SourceMapSampler, input.UV + float2(x, y) * TexelSize, 0.0f).x;
            float weight = GetGaussian(length(float2(x, y)), BLUR_SIGMA) * GetGaussian(neighbor - current, BLUR_BSIGMA) * normalization;
            filtered += neighbor * weight;
            weightSum += weight;
            minimum = min(minimum, neighbor);
            maximum = max(maximum, neighbor);
        }
    }

    filtered /= weightSum;

    float depth = DepthMap.SampleLevel(DepthMapSampler, input.UV, 0.0f).x;
    if (IsHistoryValid == 0 || depth >= 1.0f)
        return filtered;

    // Without motion vectors the texel is reprojected with the camera alone: moving objects keep some stale occlusion,
    // which the neighborhood clamp limits.
    float2 historyUV;
    if (HasVelocity != 0)
    {
        historyUV = input.UV - VelocityMap.SampleLevel(VelocityMapSampler, input.UV, 0.0f).xy;
    }
    else
    {
        float2 ndc = float2(input.UV.x * 2.0f - 1.0f, 1.0f - input.UV.y * 2.0f);
        float4 previousClip = mul(float4(ndc, depth, 1.0f), ReprojectionMatrix);
        historyUV = (previousClip.w > 0.0f) ? (previousClip.xy / previousClip.w * float2(0.5f, -0.5f) + 0.5f) : float2(-1.0f, -1.0f);
    }

    if (any(historyUV < 0.0f) || any(historyUV > 1.0f))
        return filtered;

    float history = clamp(AuxMap.SampleLevel(AuxMapSampler, historyUV, 0.0f).x, minimum - 0.1f, maximum + 0.1f);
    return lerp(filtered, history, HistoryWeight);
}
