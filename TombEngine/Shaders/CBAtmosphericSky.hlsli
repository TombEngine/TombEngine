// CBAtmosphericSky.hlsli — Constant buffer for the atmospheric sky dome shader.
// Bound to register b10 (shared with HUD — different render pass).
// Must match CAtmosphericSkyBuffer in C++ exactly.

#ifndef CB_ATMOSPHERIC_SKY_HLSLI
#define CB_ATMOSPHERIC_SKY_HLSLI

cbuffer CBAtmosphericSky : register(b10)
{
    // Row 0 — Sun direction and elevation
    float3 AtmoSunDirection;        // Normalized world-space sun direction.
    float  AtmoSunElevation;        // sin(pitch): +1 = zenith, 0 = horizon, -1 = nadir.
    //--
    // Row 1 — Sun color and day/night blend
    float3 AtmoSunColor;            // Effective sun color from lens flare system.
    float  AtmoDayNightBlend;       // [0,1] 0 = full day, 1 = full night.
    //--
    // Row 2 — Sky base color and density
    float3 AtmoSkyColor;            // Base Rayleigh-like sky tint.
    float  AtmoDensity;             // Atmospheric density factor.
    //--
    // Row 3 — Scattering parameters
    float  AtmoZenithOffset;        // Zenith density vertical offset.
    float  AtmoMultiScatterPhase;   // Multi-scatter sun elevation effect.
    float  AtmoAnisotropicIntensity;// Mie anisotropic intensity.
    float  AtmoMieIntensity;        // Mie glow multiplier.
    //--
    // Row 4 — Rayleigh and glow
    float  AtmoRayleighIntensity;   // Rayleigh brightness multiplier.
    float  AtmoSunGlowIntensity;    // Sun glow field intensity.
    float  AtmoHorizonDarkeningStr; // Horizon darkening strength.
    float  AtmoExposureMultiplier;  // Tone mapping exposure control.
    //--
    // Row 5 — Night sky parameters
    float  AtmoNightSkyBrightness;  // Base night sky brightness.
    float  AtmoStarfieldVisibility; // [0,1] starfield fade factor.
    float  AtmoTwilightOffset;      // Sun angle for twilight start (radians).
    float  AtmoNightBlendSpeed;     // Night blend speed factor.
    //--
    // Row 6 — Viewport info
    float2 AtmoViewSize;            // Render target size.
    float2 AtmoInvViewSize;         // 1.0 / AtmoViewSize.
    //--
    // Row 7 — Sun elevation color ramp + shader sun disk
    float  AtmoSunElevationRampSpeed; // How quickly warm tint fades as sun rises.
    float  AtmoSunWarmInfluence;      // Max blend weight toward sun color at horizon.
    float  AtmoSunDiskCosRadius;      // cos(half_angle): threshold for sun disk.
    float  AtmoSunDiskIntensity;      // Sun disk brightness before tone mapping.
    //--
    // Row 8 — Moon direction and elevation
    float3 AtmoMoonDirection;         // Normalized world-space moon direction.
    float  AtmoMoonElevation;         // sin(pitch): +1 = zenith, 0 = horizon, -1 = nadir.
    //--
    // Row 9 — Moon color and phase
    float3 AtmoMoonColor;             // Moon surface color (tinted by sun illumination).
    float  AtmoMoonPhase;             // [0,1] 0 = new moon, 0.5 = full moon, 1 = new moon.
    //--
    // Row 10 — Moon disk and glow
    float  AtmoMoonDiskCosRadius;     // cos(half_angle): threshold for moon disk.
    float  AtmoMoonDiskIntensity;     // Moon disk brightness before tone mapping.
    float  AtmoMoonGlowIntensity;     // Halo/glow brightness around moon in sky.
    float  AtmoMoonGlowFalloff;       // How quickly glow fades from moon center.
    //--
    // Row 11 — Moon enable/visibility + phase illumination
    float  AtmoMoonEnabled;           // 0 or 1.
    float  AtmoMoonPhaseBrightness;   // [0,1] brightness from moon phase.
    float  AtmoMoonVisibility;        // [0,1] fades in as sky darkens.
    float  AtmoMoonPad0;
};

#endif // CB_ATMOSPHERIC_SKY_HLSLI
