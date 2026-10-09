// VolumetricClouds.hlsl Bounded-volume procedural volumetric cloud renderer.
//
// Architecture:
//   - Rendered as a fullscreen pass AFTER the sky bitmap, BEFORE world geometry.
//   - Raymarches through a spherical-shell cloud volume around the planet.
//   - Uses purely procedural noise (no 3D textures) for maximum portability.
//   - Outputs RGBA: RGB = lit cloud color, A = cloud opacity.
//   - A separate entry point provides lens flare occlusion transmittance.
//
// Shader entry points:
//   VS			   shared fullscreen-triangle vertex shader
//   PSClouds		 main cloud rendering pixel shader
//   PSCloudOcclusion lens flare occlusion evaluation (single-pixel)
//   PSCloudComposite upsamples half-res cloud result and composites over scene

#include "./CBCamera.hlsli"
#include "./CBVolumetricCloud.hlsli"
#include "./Math.hlsli"
#include "./Samplers.hlsli"

// ---------------------------------------------------------------------------
// Samplers (reuse existing engine samplers)
// ---------------------------------------------------------------------------

Texture2D SceneColorTexture : register(t0);  // Half-res cloud RGBA (from cloud pass)
Texture2D CloudTexture      : register(t1);  // (unused in composite)
Texture2D DepthTexture      : register(t2);  // Scene depth (for composite masking)
Texture2D SceneBackgroundTex : register(t3); // Full-res scene before cloud composite

// Pre-baked noise textures â€” replace per-sample procedural noise.
Texture3D<float4> CloudNoise3D  : register(t5);  // 128^3 RGBA8: R=Perlin, G=Value, B=CurlX, A=CurlZ
Texture2D<float2> CloudWorley2D : register(t6);  // 256^2 RG8:   R=Worley seed1, G=Worley seed2

// Samplers are bound once per frame to fixed slots (see Samplers.hlsli).
#define SceneColorSamp      LinearClampSampler
#define CloudHistorySamp    LinearClampSampler
#define SceneBackgroundSamp LinearClampSampler
#define NoiseSamp3D         LinearWrapSampler  // Noise must tile.
#define NoiseSamp2D         LinearWrapSampler  // Noise must tile.

// ---------------------------------------------------------------------------
// Vertex shader fullscreen triangle (reads from vertex buffer like PostProcess)
// ---------------------------------------------------------------------------

struct VSInput
{
    float3 Position : POSITION0;
    float2 UV	   : TEXCOORD0;
    float4 Color	: COLOR0;
};

struct VSOutput
{
    float4 Position	 : SV_POSITION;
    float2 UV		   : TEXCOORD0;
    float4 PositionCopy : TEXCOORD1;
};

VSOutput VS(VSInput input)
{
    VSOutput output;

    output.Position	 = float4(input.Position, 1.0f);
    output.UV		   = input.UV;
    output.PositionCopy = output.Position;

    return output;
}

#include "./VolumetricCloudsCore.hlsli"
