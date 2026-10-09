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
#define GOD_RAY_SAMPLER(texture) LinearSamp

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

#include "./GodRayCore.hlsli"
