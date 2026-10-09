// ============================================================================
// DustStorm.hlsl - Screen-space volumetric dust / sand storm.
//
// The pass runs after all
// opaque + transparent geometry; depth from the linear depth render target is
// used both to clamp marching to the closest surface and to blend the dust
// with the engine's distance fog.
//
// Algorithm:
//   1. For every pixel reconstruct the world-space ray from the camera through
//      the pixel's far-plane projection.
//   2. Sample linear depth and convert to a march distance "mt".
//   3. Step from a near distance outward in geometric increments (d *= growth),
//      sampling 3D triangle noise modulated by a height window (MinHeight ..
//      MaxHeight), accumulating the dust color via the same lerp() pattern as
//      the reference shader.
//   4. Output the accumulated dust as RGBA with premultiplied alpha so the C++
//      side can simply blend with SrcAlpha / InvSrcAlpha.
//
// Performance:
//   - StepCount is driven from the CB (5..8 by default, matching the reference).
//   - Distance-LOD: noise scale shrinks with distance, allowing low step counts
//     without visible seams.
//   - No shadow march (the reference's shadow() depended on a scene SDF that
//     does not exist in this engine - a cheap height-based attenuation is used
//     in its place).
// ============================================================================

#include "./CBDustStorm.hlsli"

// Linear depth target (R32_FLOAT). 0 = near plane, FarPlane = beyond far plane.
Texture2D    DepthTexture      : register(t0);
// Outdoor mask (R8_UNORM): 1.0 = outdoor pixel, 0.0 = indoor pixel.
Texture2D    OutdoorMaskTexture : register(t1);
SamplerState LinearSamp        : register(s3); // LinearClamp
SamplerState PointSamp         : register(s1); // PointWrap
#define DUST_STORM_SAMPLER(texture) PointSamp
#define DUST_STORM_OUTDOOR_CHANNEL r

// ---------------------------------------------------------------------------
// Fullscreen-triangle vertex shader (same layout as God Ray / PostProcess).
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

VSOutput VSDustStorm(VSInput input)
{
    VSOutput output;
    output.Position     = float4(input.Position, 1.0f);
    output.UV           = input.UV;
    output.PositionCopy = output.Position;
    return output;
}
#include "./DustStormCore.hlsli"
