// CBGodRay.hlsli — Constant buffer for the god ray shader.
// Bound to register b12 (reuses the AtmosphericSky slot; safe since god rays
// render between sky dome and HUD passes).
// Must match CGodRayBuffer in C++ exactly.

#ifndef CB_GOD_RAY_HLSLI
#define CB_GOD_RAY_HLSLI

cbuffer CBGodRay : register(b12)
{
    // Row 0 — Sun position and ray parameters
    float2 GodRaySunScreenPos;    // Projected sun UV [0,1]x[0,1].
    float  GodRayLength;          // Max ray reach in UV space.
    float  GodRayIntensity;       // Overall brightness multiplier.
    //--
    // Row 1 — Sampling and auto strength
    float  GodRayDecay;           // Per-sample exponential decay.
    int    GodRaySampleCount;     // Number of radial samples.
    float  GodRaySunElevation;    // sin(pitch) of sun.
    float  GodRayAutoStrength;    // Computed automatic strength [0,1].
    //--
    // Row 2 — Sun color and softness
    float3 GodRaySunColor;        // Sun color for ray tinting.
    float  GodRaySoftness;        // Sun-glow falloff multiplier.
    //--
    // Row 3 — View info
    float2 GodRayViewSize;        // Full render target size.
    float2 GodRayInvViewSize;     // 1.0 / GodRayViewSize.
    //--
    // Row 4 — Underwater shaft mode
    float  GodRayUnderwaterActive;    // 0 = normal, 1 = procedural underwater mask.
    float  GodRayUnderwaterBrightness;// Overall multiplier for procedural mask.
    float  GodRayUnderwaterTime;      // Accumulated time for drift animation.
    float  GodRayUnderwaterSharpness; // Mask power: 1 = soft, 4+ = thin shafts.
    //--
    // Row 5 — Underwater wind drift direction and ray params
    float  GodRayUnderwaterWindX;        // Normalized X wind component.
    float  GodRayUnderwaterWindY;        // Normalized Y wind component.
    float  GodRayUnderwaterRayLength;    // Underwater-specific march reach in UV space.
    float  GodRayUnderwaterRayDecay;     // Underwater-specific per-sample exponential decay.
    //--
    // Row 6 — Underwater ray appearance
    float  GodRayUnderwaterRayIntensity; // Final brightness multiplier for underwater god rays.
    float  _godRayPad5;
    float  _godRayPad6;
    int    GodRayUnderwaterSampleCount;  // Radial sample count for the underwater march.
};

#endif // CB_GOD_RAY_HLSLI
