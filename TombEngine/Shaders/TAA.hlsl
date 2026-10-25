// TEN_SHADER: Compute CS samplers=4 storage_textures=0 storage_buffers=0 uniform_buffers=1 rw_storage_textures=1 threads=16,16,1

// Temporal antialiasing resolve (SceneRendererTAA.cpp), the "TAA" pass after the scene pass. The scene is rendered with
// a sub-pixel jitter that changes every frame; this pass blends it with the history of the previous frames, fetched
// where the prepass motion vectors (or the camera motion, for texels without geometry) say the texel was. The history
// is clamped to the color range of the current neighborhood, so stale history (disocclusions, transparent and
// animated surfaces without motion vectors) cannot ghost. Writes the next history, which the later passes read as the
// scene. A compute shader: each group gathers the colors, depths and motions of its tile and a one texel border once
// into group shared memory, so the 3x3 neighborhoods cost no texture fetches.

#include "./ShaderInterop.hlsli"

#define GROUP_SIZE 16
#define TILE_SIZE  (GROUP_SIZE + 2)

TEN_CS_UNIFORMS(TemporalUniforms, 0)
{
    float4x4 ReprojectionMatrix; // Current unjittered clip space to the previous frame, for texels without geometry.
    float2   TexelSize;
    float    CurrentWeight;      // Share of the current frame in the blend.
    uint     IsHistoryValid;     // False after a camera cut or a target change.
    int2     TargetSize;
    uint     FrameIndex;         // Changes the rounding noise every frame.
    uint     TemporalPadding;
};

TEN_CS_TEXTURE(Texture2D, CurrentMap, 0);  // Scene target.
TEN_CS_TEXTURE(Texture2D, HistoryMap, 1);  // Antialiased scene of the previous frame, linear filtered.
TEN_CS_TEXTURE(Texture2D, VelocityMap, 2); // Prepass motion in UV units.
TEN_CS_TEXTURE(Texture2D, DepthMap, 3);    // Prepass depth copy, 1 where no geometry was drawn.

#ifdef __spirv__
[[vk::image_format("rgba8")]]
#endif
TEN_CS_RW_TEXTURE(RWTexture2D<float4>, OutputMap, 0); // Next history, 8 bits per channel.

groupshared float3 TileColors[TILE_SIZE * TILE_SIZE]; // YCoCg.
groupshared float  TileDepths[TILE_SIZE * TILE_SIZE];
groupshared float2 TileMotions[TILE_SIZE * TILE_SIZE];

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
    float2 position = uv / TexelSize;
    float2 center = floor(position - 0.5f) + 0.5f;
    float2 f = position - center;

    float2 w0 = f * (-0.5f + f * (1.0f - 0.5f * f));
    float2 w1 = 1.0f + f * f * (-2.5f + 1.5f * f);
    float2 w2 = f * (0.5f + f * (2.0f - 1.5f * f));
    float2 w3 = f * f * (-0.5f + 0.5f * f);
    float2 w12 = w1 + w2;

    float2 uv0 = (center - 1.0f) * TexelSize;
    float2 uv3 = (center + 2.0f) * TexelSize;
    float2 uv12 = (center + w2 / w12) * TexelSize;

    float3 result = HistoryMap.SampleLevel(HistoryMapSampler, float2(uv12.x, uv0.y), 0.0f).rgb * (w12.x * w0.y);
    result += HistoryMap.SampleLevel(HistoryMapSampler, float2(uv0.x, uv12.y), 0.0f).rgb * (w0.x * w12.y);
    result += HistoryMap.SampleLevel(HistoryMapSampler, float2(uv12.x, uv12.y), 0.0f).rgb * (w12.x * w12.y);
    result += HistoryMap.SampleLevel(HistoryMapSampler, float2(uv3.x, uv12.y), 0.0f).rgb * (w3.x * w12.y);
    result += HistoryMap.SampleLevel(HistoryMapSampler, float2(uv12.x, uv3.y), 0.0f).rgb * (w12.x * w3.y);

    float weight = (w12.x * w0.y) + (w0.x * w12.y) + (w12.x * w12.y) + (w3.x * w12.y) + (w12.x * w3.y);
    return max(result / weight, 0.0f);
}

// Noise in [0, 1) per texel and frame (interleaved gradient noise).
float GetRoundingNoise(int2 pixel)
{
    float2 position = float2(pixel) + 5.588238f * (float)(FrameIndex % 64);
    return frac(52.982919f * frac(dot(position, float2(0.06711056f, 0.00583715f))));
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

// Motion of the nearest texel of the neighborhood: the edges of a moving object, where the texel itself may belong to
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

// Colors, depths and motions of the tile of the group and its border, in 2x2 blocks: each block takes six gathers
// instead of twelve loads, which matters on small GPUs, where the resolve is bound by texture fetches. The point clamp
// samplers repeat the edge texels for blocks past the target. Gather returns the lower left, lower right, upper right
// and upper left texels in x, y, z and w.
void LoadTile(int2 groupOrigin, int threadIndex)
{
    const int BLOCKS = TILE_SIZE / 2;
    if (threadIndex >= (BLOCKS * BLOCKS))
        return;

    int2 block = int2(threadIndex % BLOCKS, threadIndex / BLOCKS);
    int2 topLeft = groupOrigin - 1 + block * 2;
    float2 uv = float2(topLeft + 1) * TexelSize;

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
        int index = texel.y * TILE_SIZE + texel.x;
        TileColors[index] = RGBToYCoCg(float3(reds[i], greens[i], blues[i]));
        TileDepths[index] = depths[i];
        TileMotions[index] = float2(motionsX[i], motionsY[i]);
    }
}

[numthreads(GROUP_SIZE, GROUP_SIZE, 1)]
void CS(uint3 groupId : SV_GroupID, uint3 threadId : SV_GroupThreadID)
{
    int2 lastPixel = TargetSize - 1;
    int2 groupOrigin = int2(groupId.xy) * GROUP_SIZE;
    LoadTile(groupOrigin, (int)(threadId.y * GROUP_SIZE + threadId.x));
    GroupMemoryBarrierWithGroupSync();

    int2 pixel = groupOrigin + int2(threadId.xy);
    if (any(pixel > lastPixel))
        return;

    // Mean and deviation of the 3x3 neighborhood bound the accepted history (variance clipping); its nearest texel
    // gives the motion.
    int center = (int)((threadId.y + 1) * TILE_SIZE + threadId.x + 1);
    float3 current = TileColors[center];
    float depth = TileDepths[center];
    int nearestIndex = center;
    float3 moment1 = float3(0.0f, 0.0f, 0.0f);
    float3 moment2 = float3(0.0f, 0.0f, 0.0f);

    [unroll]
    for (int y = -1; y <= 1; y++)
    {
        [unroll]
        for (int x = -1; x <= 1; x++)
        {
            int index = center + y * TILE_SIZE + x;
            float3 neighbor = TileColors[index];
            moment1 += neighbor;
            moment2 += neighbor * neighbor;

            float neighborDepth = TileDepths[index];
            if (neighborDepth < depth)
            {
                depth = neighborDepth;
                nearestIndex = index;
            }
        }
    }

    float3 mean = moment1 / 9.0f;
    float3 deviation = sqrt(max(moment2 / 9.0f - mean * mean, 0.0f));
    float3 boxMin = mean - deviation;
    float3 boxMax = mean + deviation;

    float3 result = current;
    float2 uv = (float2(pixel) + 0.5f) * TexelSize;
    float2 historyUV = uv - GetMotion(uv, nearestIndex, depth);
    if (IsHistoryValid != 0 && all(historyUV >= 0.0f) && all(historyUV <= 1.0f))
    {
        float3 history = ClipToBox(RGBToYCoCg(SampleHistoryCatmullRom(historyUV)), boxMin, boxMax);

        // Weights divided by the luma keep single bright texels from flickering through the blend.
        float currentWeight = CurrentWeight / (1.0f + current.x);
        float historyWeight = (1.0f - CurrentWeight) / (1.0f + history.x);
        result = (current * currentWeight + history * historyWeight) / (currentWeight + historyWeight);
    }

    // The 8 bit history is rounded stochastically: plain rounding would stop a slow blend short of its target, since the
    // current share of a small difference rounds to nothing. The noise averages out over frames.
    OutputMap[pixel] = float4(saturate(YCoCgToRGB(result) + (GetRoundingNoise(pixel) - 0.5f) / 255.0f), 1.0f);
}
