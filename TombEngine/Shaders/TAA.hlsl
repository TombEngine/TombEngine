// TEN_SHADER: Vertex VS samplers=0 storage_textures=0 storage_buffers=0 uniform_buffers=0
// TEN_SHADER: Pixel PS samplers=4 storage_textures=0 storage_buffers=0 uniform_buffers=3

// Temporal antialiasing resolve (SceneRendererTAA.cpp), the "TAA" pass after the scene pass. The scene is rendered with
// a sub-pixel jitter that changes every frame; this pass blends it with the history of the previous frames, fetched
// where the prepass motion vectors (or the camera motion, for texels without geometry) say the texel was. The history
// is clamped to the color range of the current neighborhood, so stale history (disocclusions, transparent and
// animated surfaces without motion vectors) cannot ghost. Writes the antialiased scene and the next history.

#include "./ShaderInterop.hlsli"
#include "./Fullscreen.hlsli"

// Slot 2, so the frame uniforms in slots 0 and 1 stay valid for the scene shaders.
TEN_PS_UNIFORMS(TemporalUniforms, 2)
{
    float4x4 ReprojectionMatrix; // Current unjittered clip space to the previous frame, for texels without geometry.
    float2   TexelSize;
    float    CurrentWeight;      // Share of the current frame in the blend.
    uint     IsHistoryValid;     // False after a camera cut or a target change.
};

TEN_PS_TEXTURE(Texture2D, CurrentMap, 0);  // Copy of the scene target.
TEN_PS_TEXTURE(Texture2D, HistoryMap, 1);  // Antialiased scene of the previous frame, linear filtered.
TEN_PS_TEXTURE(Texture2D, VelocityMap, 2); // Prepass motion in UV units.
TEN_PS_TEXTURE(Texture2D, DepthMap, 3);    // Prepass depth copy, 1 where no geometry was drawn.

typedef FullscreenPixelInput PixelInput;

struct TemporalOutput
{
    float4 Color   : SV_Target0;
    float4 History : SV_Target1;
};

PixelInput VS(uint vertexId : SV_VertexID)
{
    return FullscreenVS(vertexId);
}

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

// Motion of the nearest texel of the 3x3 neighborhood: the edges of a moving object, where the texel itself may belong
// to the background, follow the object, which keeps them from trailing. Texels without geometry are at the far plane:
// their motion comes from the camera motion alone.
float2 GetMotion(float2 uv, int2 pixel, int2 lastPixel)
{
    int2 nearestPixel = pixel;
    float depth = DepthMap.Load(int3(pixel, 0)).x;

    [unroll]
    for (int y = -1; y <= 1; y++)
    {
        [unroll]
        for (int x = -1; x <= 1; x++)
        {
            int2 neighborPixel = clamp(pixel + int2(x, y), int2(0, 0), lastPixel);
            float neighborDepth = DepthMap.Load(int3(neighborPixel, 0)).x;
            if (neighborDepth < depth)
            {
                depth = neighborDepth;
                nearestPixel = neighborPixel;
            }
        }
    }

    if (depth < 1.0f)
        return VelocityMap.Load(int3(nearestPixel, 0)).xy;

    float2 ndc = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
    float4 previousClip = mul(float4(ndc, depth, 1.0f), ReprojectionMatrix);
    if (previousClip.w <= 0.0f)
        return float2(0.0f, 0.0f);

    return (ndc - previousClip.xy / previousClip.w) * float2(0.5f, -0.5f);
}

TemporalOutput PS(PixelInput input)
{
    int2 pixel = int2(input.Position.xy);
    int2 lastPixel = int2(1.0f / TexelSize) - 1;
    float4 current = CurrentMap.Load(int3(pixel, 0));

    // Mean and deviation of the 3x3 neighborhood bound the accepted history (variance clipping).
    float3 moment1 = float3(0.0f, 0.0f, 0.0f);
    float3 moment2 = float3(0.0f, 0.0f, 0.0f);

    [unroll]
    for (int y = -1; y <= 1; y++)
    {
        [unroll]
        for (int x = -1; x <= 1; x++)
        {
            float3 neighbor = RGBToYCoCg(CurrentMap.Load(int3(clamp(pixel + int2(x, y), int2(0, 0), lastPixel), 0)).rgb);
            moment1 += neighbor;
            moment2 += neighbor * neighbor;
        }
    }

    float3 mean = moment1 / 9.0f;
    float3 deviation = sqrt(max(moment2 / 9.0f - mean * mean, 0.0f));
    float3 boxMin = mean - deviation;
    float3 boxMax = mean + deviation;

    float3 result = current.rgb;
    float2 historyUV = input.UV - GetMotion(input.UV, pixel, lastPixel);
    if (IsHistoryValid != 0 && all(historyUV >= 0.0f) && all(historyUV <= 1.0f))
    {
        float3 currentColor = RGBToYCoCg(current.rgb);
        float3 history = ClipToBox(RGBToYCoCg(SampleHistoryCatmullRom(historyUV)), boxMin, boxMax);

        // Weights divided by the luma keep single bright texels from flickering through the blend.
        float currentWeight = CurrentWeight / (1.0f + currentColor.x);
        float historyWeight = (1.0f - CurrentWeight) / (1.0f + history.x);
        result = YCoCgToRGB((currentColor * currentWeight + history * historyWeight) / (currentWeight + historyWeight));
    }

    TemporalOutput output;
    output.Color = float4(saturate(result), current.a);
    output.History = float4(saturate(result), 1.0f);
    return output;
}
