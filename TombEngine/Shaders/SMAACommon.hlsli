#ifndef TEN_SMAA_COMMON
#define TEN_SMAA_COMMON

// SMAA 1x of the frontend (Renderer::ApplySMAA) on the shared SMAA library (Shaders/SMAA.hlsli). Every texture
// carries its own sampler, so the library macros pass texture and sampler together. The preset is defined by the
// including shader. Vertex offsets are computed in the pixel shaders, which use the shared fullscreen triangle.

#include "./ShaderInterop.hlsli"
#include "./Fullscreen.hlsli"

TEN_PS_UNIFORMS(SmaaUniforms, 2)
{
    float4 SmaaMetrics; // 1 / width, 1 / height, width, height.
};

#define SMAA_RT_METRICS SmaaMetrics
#define SMAA_CUSTOM_SL

#define SMAATexture2D(tex) Texture2D tex, SamplerState tex##Sampler
#define SMAATexturePass2D(tex) tex, tex##Sampler
#define SMAASampleLevelZero(tex, coord) tex.SampleLevel(tex##Sampler, coord, 0)
#define SMAASampleLevelZeroPoint(tex, coord) tex.SampleLevel(tex##Sampler, coord, 0)
#define SMAASampleLevelZeroOffset(tex, coord, offset) tex.SampleLevel(tex##Sampler, coord, 0, offset)
#define SMAASample(tex, coord) tex.Sample(tex##Sampler, coord)
#define SMAASamplePoint(tex, coord) tex.Sample(tex##Sampler, coord)
#define SMAASampleOffset(tex, coord, offset) tex.Sample(tex##Sampler, coord, offset)
#define SMAA_FLATTEN [flatten]
#define SMAA_BRANCH [branch]
#define SMAATexture2DMS2(tex) Texture2DMS<float4, 2> tex
#define SMAALoad(tex, pos, sample) tex.Load(pos, sample)

#include "SMAA.hlsli"

// Edge detection: Map0 is the image (point sampled). Weights: Map0 edges, Map1 area, Map2 search (linear sampled).
// Neighborhood blending: Map0 is the image, Map1 the weights (linear sampled).
TEN_PS_TEXTURE(Texture2D, Map0, 0);
TEN_PS_TEXTURE(Texture2D, Map1, 1);
TEN_PS_TEXTURE(Texture2D, Map2, 2);

FullscreenPixelInput VS(uint vertexId : SV_VertexID)
{
    return FullscreenVS(vertexId);
}

float4 PSEdgeDetection(FullscreenPixelInput input) : SV_Target
{
    float4 offset[3];
    SMAAEdgeDetectionVS(input.UV, offset);
    return float4(SMAAColorEdgeDetectionPS(input.UV, offset, SMAATexturePass2D(Map0)), 0.0f, 0.0f);
}

float4 PSBlendingWeight(FullscreenPixelInput input) : SV_Target
{
    float2 pixelCoord;
    float4 offset[3];
    SMAABlendingWeightCalculationVS(input.UV, pixelCoord, offset);
    return SMAABlendingWeightCalculationPS(input.UV, pixelCoord, offset, SMAATexturePass2D(Map0), SMAATexturePass2D(Map1), SMAATexturePass2D(Map2), 0.0f);
}

float4 PSNeighborhoodBlending(FullscreenPixelInput input) : SV_Target
{
    float4 offset;
    SMAANeighborhoodBlendingVS(input.UV, offset);
    return SMAANeighborhoodBlendingPS(input.UV, offset, SMAATexturePass2D(Map0), SMAATexturePass2D(Map1));
}

#endif // TEN_SMAA_COMMON
