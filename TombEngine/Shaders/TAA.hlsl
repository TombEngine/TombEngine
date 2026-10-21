// TEN_SHADER: Compute CS samplers=4 storage_textures=0 storage_buffers=0 uniform_buffers=1 rw_storage_textures=1 threads=16,16,1 permutations=TAA_UPSCALE

// Temporal antialiasing and upscaling resolve (SceneRendererTAA.cpp), the "TAA" pass after the scene pass. The scene is
// rendered with a sub-pixel jitter that changes every frame, at the render resolution; this pass writes the next history
// at the window resolution, which the later passes read as the scene. Each output texel reconstructs the current frame
// from the 3x3 jittered scene samples nearest to it, weighted by their distance, and blends it with the history of the
// previous frames, fetched where the prepass motion vectors (or the camera motion, for texels without geometry) say the
// texel was. The history is clamped to the color range of the samples, so stale history (disocclusions, transparent and
// animated surfaces without motion vectors) cannot ghost. Below native scale the samples cover different output texels
// every frame, and the history gathers them into the full resolution. The TAA_UPSCALE permutation runs below native
// scale; at native scale the current color of a texel is its jittered sample.
// A compute shader: each group gathers the colors, depths and motions of the input texels under its output tile into
// group shared memory once, so the 3x3 neighborhoods cost no texture fetches.

#include "./ShaderInterop.hlsli"

#define GROUP_SIZE 16
#if TAA_UPSCALE
#define INPUT_TILE_SIZE 20 // Input texels under a group of output texels at most native scale, with a two texel border.
#else
#define INPUT_TILE_SIZE (GROUP_SIZE + 2)
#endif

TEN_CS_UNIFORMS(TemporalUniforms, 0)
{
    float4x4 ReprojectionMatrix; // Current unjittered clip space to the previous frame, for texels without geometry.
    float2   InputSize;          // Render resolution.
    float2   OutputSize;         // Window resolution of the history.
    float2   InputTexelSize;
    float2   OutputTexelSize;
    float2   SampleOffset;       // Position of the jittered samples relative to the input texel centers, in input texels.
    float    CurrentWeight;      // Share of a current sample at the output texel center in the blend.
    uint     IsHistoryValid;     // False after a camera cut or a target change.
    uint     FrameIndex;         // Changes the rounding noise every frame.
    uint     TemporalPadding0;
    uint     TemporalPadding1;
    uint     TemporalPadding2;
};

TEN_CS_TEXTURE(Texture2D, CurrentMap, 0);  // Scene target.
TEN_CS_TEXTURE(Texture2D, HistoryMap, 1);  // Resolved scene of the previous frame, linear filtered.
TEN_CS_TEXTURE(Texture2D, VelocityMap, 2); // Prepass motion in UV units.
TEN_CS_TEXTURE(Texture2D, DepthMap, 3);    // Prepass depth copy, 1 where no geometry was drawn.

#ifdef __spirv__
[[vk::image_format("rgba8")]]
#endif
TEN_CS_RW_TEXTURE(RWTexture2D<float4>, OutputMap, 0); // Next history, 8 bits per channel.

groupshared float3 TileColors[INPUT_TILE_SIZE * INPUT_TILE_SIZE]; // YCoCg.
groupshared float  TileDepths[INPUT_TILE_SIZE * INPUT_TILE_SIZE];
groupshared float2 TileMotions[INPUT_TILE_SIZE * INPUT_TILE_SIZE];

// Luma and chroma: the neighborhood range is tighter in this space than in RGB.
float3 RGBToYCoCg(float3 color)
{
    return float3(dot(color, float3(0.25f, 0.5f, 0.25f)), dot(color, float3(0.5f, 0.0f, -0.5f)), dot(color, float3(-0.25f, 0.5f, -0.25f)));
}

float3 YCoCgToRGB(float3 color)
{
    return float3(color.x + color.y - color.z, color.x + color.z, color.x - color.y - color.z);
}

// Catmull-Rom filtered history in five bilinear taps, which keeps the history sharp when it is resampled every frame.
float3 SampleHistoryCatmullRom(float2 uv)
{
    float2 position = uv / OutputTexelSize;
    float2 center = floor(position - 0.5f) + 0.5f;
    float2 f = position - center;

    float2 w0 = f * (-0.5f + f * (1.0f - 0.5f * f));
    float2 w1 = 1.0f + f * f * (-2.5f + 1.5f * f);
    float2 w2 = f * (0.5f + f * (2.0f - 1.5f * f));
    float2 w3 = f * f * (-0.5f + 0.5f * f);
    float2 w12 = w1 + w2;

    float2 uv0 = (center - 1.0f) * OutputTexelSize;
    float2 uv3 = (center + 2.0f) * OutputTexelSize;
    float2 uv12 = (center + w2 / w12) * OutputTexelSize;

    float3 result = HistoryMap.SampleLevel(HistoryMapSampler, float2(uv12.x, uv0.y), 0.0f).rgb * (w12.x * w0.y);
    result += HistoryMap.SampleLevel(HistoryMapSampler, float2(uv0.x, uv12.y), 0.0f).rgb * (w0.x * w12.y);
    result += HistoryMap.SampleLevel(HistoryMapSampler, float2(uv12.x, uv12.y), 0.0f).rgb * (w12.x * w12.y);
    result += HistoryMap.SampleLevel(HistoryMapSampler, float2(uv3.x, uv12.y), 0.0f).rgb * (w3.x * w12.y);
    result += HistoryMap.SampleLevel(HistoryMapSampler, float2(uv12.x, uv3.y), 0.0f).rgb * (w12.x * w3.y);

    float weight = (w12.x * w0.y) + (w0.x * w12.y) + (w12.x * w12.y) + (w3.x * w12.y) + (w12.x * w3.y);
    return max(result / weight, 0.0f);
}

// Moves a color outside the box toward the box center until it lies on the box.
float3 ClipToBox(float3 color, float3 boxMin, float3 boxMax)
{
    float3 center = (boxMin + boxMax) * 0.5f;
    float3 extents = max((boxMax - boxMin) * 0.5f, 0.0001f);
    float3 offset = color - center;
    float3 units = abs(offset / extents);
    float scale = max(units.x, max(units.y, units.z));
    return (scale > 1.0f) ? (center + offset / scale) : color;
}

// Noise in [0, 1) per texel and frame (interleaved gradient noise).
float GetRoundingNoise(int2 pixel)
{
    float2 position = float2(pixel) + 5.588238f * (float)(FrameIndex % 64);
    return frac(52.982919f * frac(dot(position, float2(0.06711056f, 0.00583715f))));
}

// Motion of the nearest sample of the neighborhood: the edges of a moving object, where the texel itself may belong to
// the background, follow the object, which keeps them from trailing. Texels without geometry are at the far plane:
// their motion comes from the camera motion alone.
float2 GetMotion(float2 uv, int nearestIndex, float depth)
{
    if (depth < 1.0f)
        return TileMotions[nearestIndex];

    float2 ndc = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
    float4 previousClip = mul(float4(ndc, depth, 1.0f), ReprojectionMatrix);
    if (previousClip.w <= 0.0f)
        return float2(0.0f, 0.0f);

    return (ndc - previousClip.xy / previousClip.w) * float2(0.5f, -0.5f);
}

// Fractional input texel index of the samples nearest to an output texel: the sample of input texel i lies at i + 0.5 +
// SampleOffset, in input texels.
float2 GetSamplePosition(int2 pixel)
{
    return (float2(pixel) + 0.5f) * (InputSize * OutputTexelSize) - 0.5f - SampleOffset;
}

// Colors, depths and motions of the input texels under the group, in 2x2 blocks: each block takes six gathers instead
// of twelve loads, which matters on small GPUs, where the resolve is bound by texture fetches. The point clamp samplers
// repeat the edge texels for blocks past the target. Gather returns the lower left, lower right, upper right and upper
// left texels in x, y, z and w.
void LoadTile(int2 tileOrigin, int threadIndex)
{
    const int BLOCKS = INPUT_TILE_SIZE / 2;
    if (threadIndex >= (BLOCKS * BLOCKS))
        return;

    int2 block = int2(threadIndex % BLOCKS, threadIndex / BLOCKS);
    float2 uv = float2(tileOrigin + block * 2 + 1) * InputTexelSize;

    float4 depths = DepthMap.GatherRed(DepthMapSampler, uv);
    float4 reds = CurrentMap.GatherRed(CurrentMapSampler, uv);
    float4 greens = CurrentMap.GatherGreen(CurrentMapSampler, uv);
    float4 blues = CurrentMap.GatherBlue(CurrentMapSampler, uv);
    float4 motionsX = VelocityMap.GatherRed(VelocityMapSampler, uv);
    float4 motionsY = VelocityMap.GatherGreen(VelocityMapSampler, uv);

    const int2 OFFSETS[4] = { int2(0, 1), int2(1, 1), int2(1, 0), int2(0, 0) };

    [unroll]
    for (int i = 0; i < 4; i++)
    {
        int2 texel = block * 2 + OFFSETS[i];
        int index = texel.y * INPUT_TILE_SIZE + texel.x;
        TileColors[index] = RGBToYCoCg(float3(reds[i], greens[i], blues[i]));
        TileDepths[index] = depths[i];
        TileMotions[index] = float2(motionsX[i], motionsY[i]);
    }
}

// First input texel of the tile of a group: at native scale the output texels are the input texels, below it the tile
// starts one texel before the nearest sample of the first output texel of the group, and the last one needs fewer than
// INPUT_TILE_SIZE input texels after it.
int2 GetTileOrigin(int2 groupOrigin)
{
#if TAA_UPSCALE
    return int2(floor(GetSamplePosition(groupOrigin))) - 1;
#else
    return groupOrigin - 1;
#endif
}

// Center of the 3x3 samples of an output texel in the tile and their weights in the current color, three per axis of a
// separable Gaussian close to Blackman-Harris. Below native scale, a current sample farther than half an output texel
// from the texel center also adds less to the history (confidence): an output texel is resolved over the frames in
// which a sample falls near it. At native scale the current color is the texel itself, as rendered with the jitter.
int GetSampleWeights(int2 pixel, int2 tileOrigin, out float3 weightsX, out float3 weightsY, out float confidence)
{
#if TAA_UPSCALE
    float2 samplePosition = GetSamplePosition(pixel);
    int2 nearestSample = int2(floor(samplePosition + 0.5f));
    float2 nearestDistance = float2(nearestSample) - samplePosition;
    float3 distancesX = float3(nearestDistance.x - 1.0f, nearestDistance.x, nearestDistance.x + 1.0f);
    float3 distancesY = float3(nearestDistance.y - 1.0f, nearestDistance.y, nearestDistance.y + 1.0f);
    weightsX = exp(-2.29f * distancesX * distancesX);
    weightsY = exp(-2.29f * distancesY * distancesY);

    float2 outputDistance = nearestDistance * (OutputSize * InputTexelSize);
    confidence = saturate(1.77f * exp(-2.29f * dot(outputDistance, outputDistance)));
    return (nearestSample.y - tileOrigin.y) * INPUT_TILE_SIZE + (nearestSample.x - tileOrigin.x);
#else
    weightsX = float3(0.0f, 1.0f, 0.0f);
    weightsY = float3(0.0f, 1.0f, 0.0f);
    confidence = 1.0f;
    return (pixel.y - tileOrigin.y) * INPUT_TILE_SIZE + (pixel.x - tileOrigin.x);
#endif
}

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void CS(uint3 groupId : SV_GroupID, uint3 threadId : SV_GroupThreadID)
{
    int2 groupOrigin = int2(groupId.xy) * GROUP_SIZE;
    int2 tileOrigin = GetTileOrigin(groupOrigin);
    LoadTile(tileOrigin, (int)(threadId.y * GROUP_SIZE + threadId.x));
    GroupMemoryBarrierWithGroupSync();

    int2 pixel = groupOrigin + int2(threadId.xy);
    if (any(float2(pixel) >= OutputSize))
        return;

    // The 3x3 samples around the nearest one: their weighted mean is the current color, the mean and deviation of their
    // colors bound the accepted history (variance clipping), and the nearest depth among them gives the motion.
    float3 weightsX;
    float3 weightsY;
    float confidence;
    int center = GetSampleWeights(pixel, tileOrigin, weightsX, weightsY, confidence);

    float3 current = float3(0.0f, 0.0f, 0.0f);
    float weightSum = 0.0f;
    float depth = 1.0f;
    int nearestIndex = center;
    float3 moment1 = float3(0.0f, 0.0f, 0.0f);
    float3 moment2 = float3(0.0f, 0.0f, 0.0f);

    [unroll]
    for (int y = -1; y <= 1; y++)
    {
        [unroll]
        for (int x = -1; x <= 1; x++)
        {
            int index = center + y * INPUT_TILE_SIZE + x;
            float3 neighbor = TileColors[index];
            moment1 += neighbor;
            moment2 += neighbor * neighbor;

            float weight = weightsX[x + 1] * weightsY[y + 1];
            current += neighbor * weight;
            weightSum += weight;

            float neighborDepth = TileDepths[index];
            if (neighborDepth < depth)
            {
                depth = neighborDepth;
                nearestIndex = index;
            }
        }
    }

    current /= weightSum;

    float3 mean = moment1 / 9.0f;
    float3 deviation = sqrt(max(moment2 / 9.0f - mean * mean, 0.0f));
    float3 boxMin = mean - deviation;
    float3 boxMax = mean + deviation;

    float3 result = current;
    float2 uv = (float2(pixel) + 0.5f) * OutputTexelSize;
    float2 historyUV = uv - GetMotion(uv, nearestIndex, depth);
    if (IsHistoryValid != 0 && all(historyUV >= 0.0f) && all(historyUV <= 1.0f))
    {
        float3 history = ClipToBox(RGBToYCoCg(SampleHistoryCatmullRom(historyUV)), boxMin, boxMax);

        // Weights divided by the luma keep single bright texels from flickering through the blend.
        float currentShare = CurrentWeight * confidence;
        float currentWeight = currentShare / (1.0f + current.x);
        float historyWeight = (1.0f - currentShare) / (1.0f + history.x);
        result = (current * currentWeight + history * historyWeight) / (currentWeight + historyWeight);
    }

    // The 8 bit history is rounded stochastically: plain rounding would stop a slow blend short of its target, since the
    // current share of a small difference rounds to nothing. The noise averages out over frames.
    OutputMap[pixel] = float4(saturate(YCoCgToRGB(result) + (GetRoundingNoise(pixel) - 0.5f) / 255.0f), 1.0f);
}
