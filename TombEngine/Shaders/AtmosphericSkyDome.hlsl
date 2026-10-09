// AtmosphericSkyDome.hlsl — Atmospheric scattering sky dome for TombEngine.
//
// Visual model adapted from a Shadertoy atmospheric scattering reference:
//   - Rayleigh-like sky color with zenith density shaping
//   - Mie-like sun glow field around the sun position
//   - Horizon darkening / black transition
//   - Sun absorption influenced by sun elevation
//   - Day-to-night blending based on sun elevation
//   - Jodie-Reinhard tone mapping
//
// Integration rules:
//   - Does NOT render a sun disk. The existing lens flare sun system handles that.
//   - Does NOT render stars. The existing starfield system handles that.
//   - Uses the existing sun direction/color from the lens flare system.
//   - Outputs sky color only; composited BEHIND sun sprites and BEHIND clouds.
//
// Entry points:
//   VSAtmosphericSky — fullscreen-triangle vertex shader
//   PSAtmosphericSky — atmospheric sky dome pixel shader

#include "./CBCamera.hlsli"
#include "./CBAtmosphericSky.hlsli"
#include "./Math.hlsli"

// ---------------------------------------------------------------------------
// Vertex shader — fullscreen triangle (same pattern as VolumetricClouds.hlsl)
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

VSOutput VSAtmosphericSky(VSInput input)
{
    VSOutput output;
    output.Position     = float4(input.Position, 1.0f);
    output.UV           = input.UV;
    output.PositionCopy = output.Position;
    return output;
}

#define SKY_PIXEL_INPUT VSOutput

// Copy of the composited scene, with the cloud coverage in its alpha channel, read by the aurora and the sun and
// moon disc passes.
Texture2D SceneAfterClouds : register(t0);
SamplerState SunMoonLinearSamp : register(s2);

#include "./AtmosphericSkyCore.hlsli"

VSOutput VSAurora(VSInput input)
{
    return VSAtmosphericSky(input);
}

VSOutput VSSunMoonDisc(VSInput input)
{
    VSOutput output;
    output.Position     = float4(input.Position, 1.0f);
    output.UV           = input.UV;
    output.PositionCopy = output.Position;
    return output;
}
