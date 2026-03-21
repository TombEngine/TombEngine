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
// Constants
// ---------------------------------------------------------------------------

static const float PI_SKY = 3.14159265359f;
static const float INV_PI_SKY = 1.0f / PI_SKY;

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

// ---------------------------------------------------------------------------
// Reconstruct world-space view direction from screen UV
// ---------------------------------------------------------------------------

float3 GetViewDirection(float2 uv)
{
    // Convert UV [0,1] to NDC [-1,1]
    float2 ndc = uv * 2.0f - 1.0f;

    // Unproject through inverse projection
    float4 clipPos = float4(ndc.x, -ndc.y, 1.0f, 1.0f);
    float4 viewPos = mul(clipPos, InverseProjection);
    viewPos.xyz /= viewPos.w;

    // Transform to world space
    float3 worldDir = mul(float4(viewPos.xyz, 0.0f), InverseView).xyz;
    return normalize(worldDir);
}

// ---------------------------------------------------------------------------
// Atmospheric scattering model (adapted from reference shader)
// ---------------------------------------------------------------------------

// Zenith density: controls how dense the atmosphere appears at different
// vertical angles. Higher values = thicker atmosphere near horizon.
float ZenithDensity(float y)
{
    return AtmoDensity / pow(max(y - AtmoZenithOffset, 0.0035f), 0.75f);
}

// Sky absorption: how much light is absorbed passing through the atmosphere.
// Based on the sky color (Rayleigh-like wavelength dependence).
float3 GetSkyAbsorption(float3 skyCol, float zenithDens)
{
    float3 absorption = skyCol * (-zenithDens);
    absorption = exp2(absorption) * 2.0f;
    return absorption;
}

// Rayleigh-like brightening around the sun direction.
// Objects near the sun get brighter due to forward scattering.
float GetRayleighMultiplier(float3 viewDir, float3 sunDir)
{
    float cosAngle = dot(viewDir, sunDir);
    float dist = 1.0f - saturate((cosAngle + 1.0f) * 0.5f); // [0,1] distance from sun
    return 1.0f + pow(1.0f - dist, 2.0f) * PI_SKY * 0.5f * AtmoRayleighIntensity;
}

// Mie-like glow around the sun — broad, smooth glow field.
// This is the atmospheric glow, NOT the sun disk itself.
float GetMie(float3 viewDir, float3 sunDir)
{
    float cosAngle = dot(viewDir, sunDir);
    float dist = 1.0f - saturate((cosAngle + 1.0f) * 0.5f);
    float disk = saturate(1.0f - pow(dist, 0.1f));
    // Smooth Hermite for soft falloff.
    disk = disk * disk * (3.0f - 2.0f * disk);
    return disk * 2.0f * PI_SKY * AtmoMieIntensity;
}

// Sun glow field — additional bright glow near the sun direction.
// Broader and softer than the Mie term, adds atmosphere around sun.
float GetSunGlow(float3 viewDir, float3 sunDir)
{
    float cosAngle = dot(viewDir, sunDir);
    float glow = pow(saturate(cosAngle), 8.0f) * AtmoSunGlowIntensity;
    return glow;
}

// ---------------------------------------------------------------------------
// Complete atmospheric scattering calculation
// ---------------------------------------------------------------------------

float3 ComputeAtmosphericScattering(float3 viewDir, float3 sunDir)
{
    // Effective sky color with anisotropic brightening.
    float3 skyCol = AtmoSkyColor * (1.0f + AtmoAnisotropicIntensity);

    // Vertical component for zenith density.
    // TEN world space is Y-down (negative Y = up).  Negate so the
    // scattering math works with the conventional Y-up convention:
    //   viewY > 0  =  looking toward zenith
    //   viewY < 0  =  looking toward ground
    float viewY = -viewDir.y;

    // Sun vertical for absorption along the sun's path.
    float sunY = -sunDir.y;

    // Zenith density at the view direction's vertical angle.
    float zenith = ZenithDensity(viewY);

    // Sun point distance multiplier — influences how much the sun's position
    // affects overall sky brightness. Based on sun elevation.
    float sunPointDistMult = saturate(max(sunY + AtmoMultiScatterPhase - AtmoZenithOffset, 0.0f));

    // Rayleigh brightening near sun.
    float rayleighMult = GetRayleighMultiplier(viewDir, sunDir);

    // Atmospheric absorption at the view angle.
    float3 absorption = GetSkyAbsorption(skyCol, zenith);

    // Atmospheric absorption along the sun's path.
    float3 sunAbsorption = GetSkyAbsorption(skyCol, ZenithDensity(sunY + AtmoMultiScatterPhase));

    // Base sky color: sky tint * zenith density * Rayleigh brightening.
    float3 sky = skyCol * zenith * rayleighMult;

    // Mie glow around the sun (atmospheric glow, NOT the sun disk).
    float3 mie = GetMie(viewDir, sunDir) * sunAbsorption;

    // Additional sun glow field.
    float3 sunGlow = GetSunGlow(viewDir, sunDir) * sunAbsorption * AtmoSunColor;

    // Mix between absorbed sky and brighter sky based on sun influence.
    // When the sun is higher, the sky transitions from deeply absorbed to brighter.
    float3 totalSky = lerp(sky * absorption, sky / (sky + 0.5f), sunPointDistMult);
    totalSky += mie;
    totalSky += sunGlow;

    // Apply sun absorption from the sun's path through the atmosphere.
    totalSky *= sunAbsorption * 0.5f + 0.5f * length(sunAbsorption);

    // Tint by sun color for warm sunrise/sunset.
    // sunY = 0: sun at horizon (max warm tint), sunY = 1: sun at zenith (no tint = white).
    // AtmoSunElevationRampSpeed: how quickly the warm tint fades as the sun rises.
    //   low value (0.5) = white zone is narrow, warm tint persists high up.
    //   high value (3.0) = warm tint only very close to the horizon.
    // AtmoSunWarmInfluence: max blend weight at horizon (0 = always white, 1 = full sun color).
    float sunInfluence = saturate(1.0f - sunY * AtmoSunElevationRampSpeed);
    totalSky *= lerp(float3(1.0f, 1.0f, 1.0f), AtmoSunColor, sunInfluence * AtmoSunWarmInfluence);

    // Horizon darkening: darken the sky near and below the horizon.
    // viewDir.y near 0 or negative = near/below horizon.
    float horizonFactor = saturate(viewY * 4.0f + 0.1f); // Ramps from dark at horizon to full above.
    horizonFactor = pow(horizonFactor, AtmoHorizonDarkeningStr);
    totalSky *= horizonFactor;

    return totalSky;
}

// ---------------------------------------------------------------------------
// Day/night transition
// ---------------------------------------------------------------------------

float3 ComputeNightSky(float3 viewDir)
{
    // Simple dark blue gradient for night sky background.
    // TEN is Y-down, so negate to get conventional up direction.
    float upFactor = saturate(-viewDir.y * 0.5f + 0.5f);
    float3 nightZenith  = float3(0.005f, 0.008f, 0.025f) * AtmoNightSkyBrightness;
    float3 nightHorizon = float3(0.002f, 0.003f, 0.008f) * AtmoNightSkyBrightness;
    return lerp(nightHorizon, nightZenith, upFactor * upFactor);
}

// ---------------------------------------------------------------------------
// Tone mapping — Jodie-Reinhard (from reference shader)
// ---------------------------------------------------------------------------

float3 JodieReinhardTonemap(float3 c)
{
    float l = dot(c, float3(0.2126f, 0.7152f, 0.0722f));
    float3 tc = c / (c + 1.0f);
    return lerp(c / (l + 1.0f), tc, tc);
}

// ---------------------------------------------------------------------------
// Pixel shader — atmospheric sky dome
// ---------------------------------------------------------------------------

float4 PSAtmosphericSky(VSOutput input) : SV_TARGET
{
    // Reconstruct world-space view direction from screen UV.
    float3 viewDir = GetViewDirection(input.UV);

    // Sun direction from the existing lens flare system (passed via CB).
    float3 sunDir = normalize(AtmoSunDirection);

    // --- Day sky ---
    float3 daySky = ComputeAtmosphericScattering(viewDir, sunDir);

    // Apply PI scale and exposure (like the reference shader).
    daySky *= PI_SKY * AtmoExposureMultiplier;

    // Tone map the day sky.
    daySky = JodieReinhardTonemap(daySky);

    // Convert back to linear (reference shader does pow 2.2 after tonemap).
    daySky = pow(max(daySky, 0.0f), 2.2f);

    // --- Night sky ---
    float3 nightSky = ComputeNightSky(viewDir);

    // --- Blend day/night ---
    // AtmoDayNightBlend: 0 = full day, 1 = full night.
    float nightFactor = AtmoDayNightBlend;

    float3 finalColor = lerp(daySky, nightSky, nightFactor);

    // Output with alpha = 1 (opaque sky background).
    // Stars and sun sprite render separately in their own passes.
    return float4(finalColor, 1.0f);
}
