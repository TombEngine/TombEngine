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

    // Fade all sun-dependent effects when the sun is below the horizon.
    // Matches the sun sprite fade and god ray fade for visual consistency.
    // sunY = 0 (horizon): 1.0 (full), sunY ≈ -0.125 (~-7°): ~zero.
    float sunBelowFade = saturate(1.0f + sunY * 8.0f);
    sunBelowFade = sunBelowFade * sunBelowFade * (3.0f - 2.0f * sunBelowFade);

    // Mix between absorbed sky and brighter sky based on sun influence.
    // When the sun is higher, the sky transitions from deeply absorbed to brighter.
    float3 totalSky = lerp(sky * absorption, sky / (sky + 0.5f), sunPointDistMult * sunBelowFade);
    totalSky += mie * sunBelowFade;
    totalSky += sunGlow * sunBelowFade;

    // Apply sun absorption from the sun's path through the atmosphere.
    totalSky *= sunAbsorption * 0.5f + 0.5f * length(sunAbsorption);

    // Tint by sun color for warm sunrise/sunset.
    // sunY = 0: sun at horizon (max warm tint), sunY = 1: sun at zenith (no tint = white).
    // AtmoSunElevationRampSpeed: how quickly the warm tint fades as the sun rises.
    //   low value (0.5) = white zone is narrow, warm tint persists high up.
    //   high value (3.0) = warm tint only very close to the horizon.
    // sunBelowFade ensures the warm tint is fully removed when the sun is below the horizon.
    float sunInfluence = saturate(1.0f - sunY * AtmoSunElevationRampSpeed);
    totalSky *= lerp(float3(1.0f, 1.0f, 1.0f), AtmoSunColor, sunInfluence * AtmoSunWarmInfluence * sunBelowFade);

    // Shader sun disk — replaces the billboard sprite so the disk is subject to the
    // same horizon darkening below. The bottom half naturally fades into the dark band,
    // giving a physically correct half-set appearance.
    // AtmoSunDiskCosRadius = cos(half_angle), precomputed on CPU.
    // Edge softness: 15% of disk radius for a smooth limb.
    float sunCosAngle  = dot(viewDir, sunDir);
    float sunEdgeWidth = (1.0f - AtmoSunDiskCosRadius) * 0.15f;
    float sunDisk = smoothstep(AtmoSunDiskCosRadius - sunEdgeWidth,
                               AtmoSunDiskCosRadius + sunEdgeWidth,
                               sunCosAngle);
    totalSky += sunDisk * AtmoSunColor * AtmoSunDiskIntensity * sunBelowFade;

    // Horizon darkening: darken the sky near and below the horizon.
    // viewDir.y near 0 or negative = near/below horizon.
    // The sun disk is added BEFORE this so its bottom half is darkened identically.
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
// Moon surface noise — adapted from IQ's Shadertoy planet shader
// ---------------------------------------------------------------------------

float MoonNoise3D(float3 p)
{
    return frac(sin(dot(p, float3(12.9898f, 78.233f, 128.852f))) * 43758.5453f) * 2.0f - 1.0f;
}

float MoonSimplex3D(float3 p)
{
    const float F3 = 1.0f / 3.0f;
    const float G3 = 1.0f / 6.0f;

    float s = (p.x + p.y + p.z) * F3;
    int i = (int)floor(p.x + s);
    int j = (int)floor(p.y + s);
    int k = (int)floor(p.z + s);

    float t  = (float)(i + j + k) * G3;
    float x0 = p.x - ((float)i - t);
    float y0 = p.y - ((float)j - t);
    float z0 = p.z - ((float)k - t);

    int i1, j1, k1, i2, j2, k2;
    if (x0 >= y0)
    {
        if      (y0 >= z0) { i1=1; j1=0; k1=0; i2=1; j2=1; k2=0; }
        else if (x0 >= z0) { i1=1; j1=0; k1=0; i2=1; j2=0; k2=1; }
        else               { i1=0; j1=0; k1=1; i2=1; j2=0; k2=1; }
    }
    else
    {
        if      (y0 < z0)  { i1=0; j1=0; k1=1; i2=0; j2=1; k2=1; }
        else if (x0 < z0)  { i1=0; j1=1; k1=0; i2=0; j2=1; k2=1; }
        else               { i1=0; j1=1; k1=0; i2=1; j2=1; k2=0; }
    }

    float x1 = x0 - (float)i1 + G3,        y1 = y0 - (float)j1 + G3,        z1 = z0 - (float)k1 + G3;
    float x2 = x0 - (float)i2 + 2.0f*G3,   y2 = y0 - (float)j2 + 2.0f*G3,   z2 = z0 - (float)k2 + 2.0f*G3;
    float x3 = x0 - 1.0f       + 3.0f*G3,  y3 = y0 - 1.0f       + 3.0f*G3,  z3 = z0 - 1.0f       + 3.0f*G3;

    float3 ijk0 = float3((float)i,       (float)j,       (float)k      );
    float3 ijk1 = float3((float)(i+i1),  (float)(j+j1),  (float)(k+k1) );
    float3 ijk2 = float3((float)(i+i2),  (float)(j+j2),  (float)(k+k2) );
    float3 ijk3 = float3((float)(i+1),   (float)(j+1),   (float)(k+1)  );

    float3 gr0 = normalize(float3(MoonNoise3D(ijk0), MoonNoise3D(ijk0*2.01f), MoonNoise3D(ijk0*2.02f)));
    float3 gr1 = normalize(float3(MoonNoise3D(ijk1), MoonNoise3D(ijk1*2.01f), MoonNoise3D(ijk1*2.02f)));
    float3 gr2 = normalize(float3(MoonNoise3D(ijk2), MoonNoise3D(ijk2*2.01f), MoonNoise3D(ijk2*2.02f)));
    float3 gr3 = normalize(float3(MoonNoise3D(ijk3), MoonNoise3D(ijk3*2.01f), MoonNoise3D(ijk3*2.02f)));

    float n0=0, n1=0, n2=0, n3=0;
    float t0 = 0.5f-x0*x0-y0*y0-z0*z0; if (t0>=0) { t0*=t0; n0=t0*t0*dot(gr0,float3(x0,y0,z0)); }
    float t1 = 0.5f-x1*x1-y1*y1-z1*z1; if (t1>=0) { t1*=t1; n1=t1*t1*dot(gr1,float3(x1,y1,z1)); }
    float t2 = 0.5f-x2*x2-y2*y2-z2*z2; if (t2>=0) { t2*=t2; n2=t2*t2*dot(gr2,float3(x2,y2,z2)); }
    float t3 = 0.5f-x3*x3-y3*y3-z3*z3; if (t3>=0) { t3*=t3; n3=t3*t3*dot(gr3,float3(x3,y3,z3)); }

    return 96.0f * (n0+n1+n2+n3);
}

float MoonFBM(float3 p)
{
    float f = 0.0f;
    f += 0.500000f * MoonSimplex3D(p); p *= 2.01f;
    f += 0.250000f * MoonSimplex3D(p); p *= 2.02f;
    f += 0.125000f * MoonSimplex3D(p); p *= 2.03f;
    f += 0.062500f * MoonSimplex3D(p); p *= 2.04f;
    f += 0.031250f * MoonSimplex3D(p); p *= 2.05f;
    f += 0.015625f * MoonSimplex3D(p);
    return f;
}

// ---------------------------------------------------------------------------
// Moon rendering — sphere with FBM surface + smooth diffuse sun illumination
// ---------------------------------------------------------------------------

// Renders the moon as a 3D sphere with:
//   - Procedural surface texture (FBM noise simulating highlands/maria)
//   - Bump-mapped normals for terrain detail
//   - Smooth Lambertian diffuse lighting from the sun direction
//   - Limb brightening (inner atmosphere rim effect)
//
// Phase behaviour:
//   sunDir ≈ -moonDir  →  full moon  (lit hemisphere faces viewer)
//   sunDir ≈  moonDir  →  new moon   (dark hemisphere faces viewer)
//   sunDir ⊥  moonDir  →  quarter moon (terminator through center)
float3 ComputeMoonDisk(float3 viewDir, float3 moonDir, float3 sunDir)
{
    if (AtmoMoonEnabled < 0.5f)
        return float3(0.0f, 0.0f, 0.0f);

    float moonCosAngle = dot(viewDir, moonDir);

    // Disk boundary with soft fringe.
    float moonEdgeWidth = (1.0f - AtmoMoonDiskCosRadius) * 0.15f;
    float diskMask = smoothstep(AtmoMoonDiskCosRadius - moonEdgeWidth,
                                AtmoMoonDiskCosRadius + moonEdgeWidth,
                                moonCosAngle);
    if (diskMask < 0.001f)
        return float3(0.0f, 0.0f, 0.0f);

    // Build orthonormal moon-disk frame.
    // TEN Y-down: world up = (0, -1, 0).
    float3 worldUp = float3(0.0f, -1.0f, 0.0f);
    float3 moonRight;
    if (abs(dot(moonDir, worldUp)) > 0.99f)
        moonRight = normalize(cross(moonDir, float3(1.0f, 0.0f, 0.0f)));
    else
        moonRight = normalize(cross(moonDir, worldUp));
    float3 moonUpVec = cross(moonRight, moonDir);

    // Disk radius in tangent space: sin(halfAngle).
    float diskRadius = sqrt(max(1e-6f, 1.0f - AtmoMoonDiskCosRadius * AtmoMoonDiskCosRadius));

    // Normalized 2D position on the disk face: [-1, 1] at the disk edge.
    float3 viewTangent = viewDir - moonDir * moonCosAngle;
    float px = dot(viewTangent, moonRight)  / diskRadius;
    float py = dot(viewTangent, moonUpVec)  / diskRadius;

    // Sphere geometry: pz > 0 is the hemisphere facing the viewer.
    float pz = sqrt(max(0.0f, 1.0f - px*px - py*py));

    // Surface normal in world space: outward from sphere, pointing toward viewer at center.
    // -moonDir is the "toward viewer" axis at the disk center.
    // Result is already unit-length because px² + py² + pz² = 1.
    float3 sphereNorm = px * moonRight + py * moonUpVec - pz * moonDir;

    // Bump mapping: displace sphere normal components via FBM to simulate terrain.
    const float e = 0.05f;
    float nx = MoonFBM(sphereNorm + float3(e, 0.0f, 0.0f)) * 0.5f + 0.5f;
    float ny = MoonFBM(sphereNorm + float3(0.0f, e, 0.0f)) * 0.5f + 0.5f;
    float nz = MoonFBM(sphereNorm + float3(0.0f, 0.0f, e)) * 0.5f + 0.5f;
    float3 bumpNorm = normalize(float3(sphereNorm.x * nx, sphereNorm.y * ny, sphereNorm.z * nz));

    // Surface albedo: bright lunar highlands vs dark maria.
    float surfaceAlbedo = 1.0f - (MoonFBM(sphereNorm) * 0.5f + 0.5f);

    // Lambertian diffuse from sun direction.
    // max(0) ensures only the lit hemisphere contributes.
    float diffuse = max(0.0f, dot(bumpNorm, sunDir));

    // Earthshine: keep the dark side faintly visible.
    const float earthshine = 0.03f;
    float illumination = surfaceAlbedo * max(diffuse, earthshine);

    // Horizon darkening (same as sun disk).
    float viewY = -viewDir.y;
    float horizonFade = saturate(viewY * 4.0f + 0.1f);
    horizonFade = pow(horizonFade, AtmoHorizonDarkeningStr);

    return diskMask * illumination * AtmoMoonColor * AtmoMoonDiskIntensity
         * AtmoMoonVisibility * horizonFade;
}

// Moon glow — subtle halo illuminating the local sky around the moon.
float3 ComputeMoonGlow(float3 viewDir, float3 moonDir)
{
    if (AtmoMoonEnabled < 0.5f || AtmoMoonGlowIntensity < 0.001f)
        return float3(0.0f, 0.0f, 0.0f);

    float cosAngle = dot(viewDir, moonDir);
    // Glow falloff: power-based, wider than the disk.
    float glow = pow(saturate(cosAngle), AtmoMoonGlowFalloff) * AtmoMoonGlowIntensity;

    // Scale by phase brightness and visibility.
    glow *= AtmoMoonPhaseBrightness * AtmoMoonVisibility;

    // Horizon darkening.
    float viewY = -viewDir.y;
    float horizonFade = saturate(viewY * 4.0f + 0.1f);
    horizonFade = pow(horizonFade, AtmoHorizonDarkeningStr * 0.5f);

    return glow * AtmoMoonColor * horizonFade;
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

    // Moon direction from the moon system (passed via CB).
    float3 moonDir = normalize(AtmoMoonDirection);

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

    // Add moon glow to the night sky (local sky illumination).
    nightSky += ComputeMoonGlow(viewDir, moonDir);

    // --- Blend day/night ---
    // AtmoDayNightBlend: 0 = full day, 1 = full night.
    float nightFactor = AtmoDayNightBlend;

    float3 finalColor = lerp(daySky, nightSky, nightFactor);

    // --- Moon disk (rendered on top of the sky blend, behind clouds) ---
    // The moon can be faintly visible during the day but becomes prominent at night.
    finalColor += ComputeMoonDisk(viewDir, moonDir, sunDir);

    // Output with alpha = 1 (opaque sky background).
    // Stars and sun sprite render separately in their own passes.
    return float4(finalColor, 1.0f);
}
