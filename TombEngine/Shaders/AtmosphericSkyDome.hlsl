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
// Aurora Borealis — raymarched curtain aurora spanning the full sky dome
// ---------------------------------------------------------------------------
// Converted from a Shadertoy GLSL aurora shader to HLSL. Uses triangle-wave
// noise with raymarching through horizontal planes to create curtain-like
// aurora structures that drape across the entire sky, similar to AltocumulusMid
// cloud coverage. Only the aurora effect is used (no stars / water reflection).
//
// Key visual elements:
//   - Raymarched volumetric curtains covering the full hemisphere
//   - Triangle-wave noise with rotational domain warping
//   - Per-step color from existing aurora color presets
//   - Smooth dithered sampling to reduce banding
//   - Height-dependent color gradient (green bottom → purple/red top)
// ---------------------------------------------------------------------------

// --- Aurora rotation matrix helper ---
float2x2 AuroraRotMat(float a)
{
    float c = cos(a);
    float s = sin(a);
    return float2x2(c, -s, s, c);
}

// Constant rotation matrix (≈17° rotation for noise decorrelation).
static const float2x2 AuroraM2 = float2x2(0.95534, -0.29552, 0.29552, 0.95534);

// --- Triangle wave: cheap periodic function with flat tops/bottoms ---
float AuroraTri(float x)
{
    return clamp(abs(frac(x) - 0.5), 0.01, 0.49);
}

// --- 2D triangle wave pair for domain warping ---
float2 AuroraTri2(float2 p)
{
    return float2(
        AuroraTri(p.x) + AuroraTri(p.y),
        AuroraTri(p.y + AuroraTri(p.x))
    );
}

// --- 2D triangle noise with rotational warping ---
// Produces the characteristic aurora curtain texture.
// animTime: pre-computed animation time (AuroraTime * speed).
float AuroraTriNoise2D(float2 p, float animTime)
{
    float z = 1.8;
    float z2 = 2.5;
    float rz = 0.0;
    p = mul(p, AuroraRotMat(p.x * 0.06));
    float2 bp = p;

    for (int i = 0; i < 5; i++)
    {
        float2 dg = AuroraTri2(bp * 1.85) * 0.75;
        dg = mul(dg, AuroraRotMat(animTime));
        p -= dg / z2;
        bp *= 1.3;
        z2 *= 0.45;
        z *= 0.42;
        p *= 1.21 + (rz - 1.0) * 0.02;
        rz += AuroraTri(p.x + AuroraTri(p.y)) * z;
        p = mul(p, -AuroraM2);
    }

    return clamp(1.0 / pow(rz * 29.0, 1.3), 0.0, 0.55);
}

// --- Simple 2D hash for per-pixel dithering ---
float AuroraHash21(float2 n)
{
    return frac(sin(dot(n, float2(12.9898, 4.1414))) * 43758.5453);
}

// Get aurora color based on preset index and height within the aurora band.
// heightFrac: 0 = bottom of aurora, 1 = top of aurora
// preset: 0=GreenClassic, 1=GreenPurple, 2=GreenRedTips, 3=BluePurple, 4=StrongMulticolor
float3 GetAuroraColor(float heightFrac, float preset)
{
    // Interpolate between integer presets for smooth transitions.
    float presetFrac = frac(preset);
    int presetA = (int)floor(preset) % 6;
    int presetB = (presetA + 1) % 6;

    // Each preset defines three color stops: bottom, mid (50 %), top.
    // For presets 0-4 the mid is exactly lerp(bot, top, 0.5), so the final
    // output is identical to the old two-stop formula.  Preset 5 uses a
    // distinct mid to produce the intended turquoise → blue → purple gradient.
    float3 botA, midA, topA;
    if      (presetA == 0) { botA = float3(0.1,  0.8,  0.2 ); midA = float3(0.075, 0.6,  0.15 ); topA = float3(0.05, 0.4,  0.1 ); }
    else if (presetA == 1) { botA = float3(0.1,  0.7,  0.3 ); midA = float3(0.3,   0.4,  0.45 ); topA = float3(0.5,  0.1,  0.6 ); }
    else if (presetA == 2) { botA = float3(0.1,  0.8,  0.2 ); midA = float3(0.4,   0.45, 0.125); topA = float3(0.7,  0.1,  0.05); }
    else if (presetA == 3) { botA = float3(0.15, 0.2,  0.7 ); midA = float3(0.275, 0.15, 0.6  ); topA = float3(0.4,  0.1,  0.5 ); }
    else if (presetA == 4) { botA = float3(0.1,  0.6,  0.3 ); midA = float3(0.35,  0.35, 0.4  ); topA = float3(0.6,  0.1,  0.5 ); }
    else                   { botA = float3(0.0,  0.8,  0.7 ); midA = float3(0.1,   0.3,  0.9  ); topA = float3(0.5,  0.1,  0.8 ); } // Turquoise / Blue / Purple

    float3 botB, midB, topB;
    if      (presetB == 0) { botB = float3(0.1,  0.8,  0.2 ); midB = float3(0.075, 0.6,  0.15 ); topB = float3(0.05, 0.4,  0.1 ); }
    else if (presetB == 1) { botB = float3(0.1,  0.7,  0.3 ); midB = float3(0.3,   0.4,  0.45 ); topB = float3(0.5,  0.1,  0.6 ); }
    else if (presetB == 2) { botB = float3(0.1,  0.8,  0.2 ); midB = float3(0.4,   0.45, 0.125); topB = float3(0.7,  0.1,  0.05); }
    else if (presetB == 3) { botB = float3(0.15, 0.2,  0.7 ); midB = float3(0.275, 0.15, 0.6  ); topB = float3(0.4,  0.1,  0.5 ); }
    else if (presetB == 4) { botB = float3(0.1,  0.6,  0.3 ); midB = float3(0.35,  0.35, 0.4  ); topB = float3(0.6,  0.1,  0.5 ); }
    else                   { botB = float3(0.0,  0.8,  0.7 ); midB = float3(0.1,   0.3,  0.9  ); topB = float3(0.5,  0.1,  0.8 ); } // Turquoise / Blue / Purple

    float3 bottomColor = lerp(botA, botB, presetFrac);
    float3 midColor    = lerp(midA, midB, presetFrac);
    float3 topColor    = lerp(topA, topB, presetFrac);

    // 3-stop piecewise gradient with the same power-bias as before.
    // t < 0.5  → bottom → mid;  t >= 0.5 → mid → top.
    // Continuity is guaranteed: both sides evaluate to midColor at t == 0.5.
    float t = pow(saturate(heightFrac), 1.5);
    float3 lowerHalf = lerp(bottomColor, midColor, saturate(t * 2.0));
    float3 upperHalf = lerp(midColor,    topColor,  saturate((t - 0.5) * 2.0));
    return t < 0.5 ? lowerHalf : upperHalf;
}

// ---------------------------------------------------------------------------
// Compute the aurora contribution for a given view direction.
//
// Raymarches through a series of horizontal planes above the viewer.
// At each step, the triangle-wave noise field (AuroraTriNoise2D) is sampled
// on the horizontal plane to produce curtain-like structures that span the
// entire sky dome — similar in coverage to AltocumulusMid clouds.
//
// screenPos: SV_POSITION.xy — used for per-pixel dithering to reduce banding.
// ---------------------------------------------------------------------------
float3 ComputeAurora(float3 viewDir, float2 screenPos)
{
    if (AuroraEnabled < 0.5 || AuroraVisibility < 0.001)
        return float3(0.0, 0.0, 0.0);

    // TEN is Y-down: negate so positive rdY = above horizon.
    float3 rd = normalize(float3(viewDir.x, -viewDir.y, viewDir.z));

    // Only render aurora above horizon.
    if (rd.y < -0.01)
        return float3(0.0, 0.0, 0.0);

    // Ray origin: camera sits below the aurora plane.
    float3 ro = float3(0.0, 0.0, -6.7);

    // Animation time: AuroraSpeed maps so that default (0.3) ≈ reference speed.
    float animTime = AuroraTime * AuroraSpeed * 0.2;

    // AuroraHeight controls the base altitude of the aurora plane.
    // Default 0.45 → baseH ≈ 0.9  (reference used 0.8).
    float baseH = AuroraHeight * 2.0;

    // Denominator factor for plane intersection — controls vertical curtain stretch.
    // AuroraVerticalStretch default 3.0 → 3.0 × 0.667 ≈ 2.0 (matches reference).
    // Higher = more stretched/elongated curtains, lower = shorter.
    float vStretch = AuroraVerticalStretch * 0.667;

    // Accumulation.
    float4 col    = float4(0.0, 0.0, 0.0, 0.0);
    float4 avgCol = float4(0.0, 0.0, 0.0, 0.0);

    // Number of raymarching steps — more steps = finer curtains.
    // AuroraLayerCount (1–5) scales the step count: 1→25, 3→50, 5→75.
    int numSteps = clamp((int)(AuroraLayerCount * 16.67), 25, 75);

    // AuroraSoftness [0,1]: 0 = crisp step transitions, 1 = very soft blending.
    float blendWeight = lerp(0.7, 0.2, AuroraSoftness);

    for (int i = 0; i < numSteps; i++)
    {
        float fi = (float)i;

        // Per-pixel dither offset to break banding.
        float of = 0.006 * AuroraHash21(screenPos) * smoothstep(0.0, 15.0, fi);

        // Intersect horizontal plane at increasing heights above the camera.
        // vStretch controls the vertical elongation of curtain features.
        float pt = ((baseH + pow(fi, 1.4) * 0.002) - ro.y) / (rd.y * vStretch + 0.4);
        pt -= of;

        // World-space hit position on the plane.
        float3 bpos = ro + pt * rd;

        // Horizontal sample position.
        // AuroraSpread scales coverage width; AuroraNoiseScale controls feature size.
        float2 p = bpos.zx * AuroraNoiseScale * AuroraSpread;

        // Domain warp — AuroraDistortionStr adds swirling ripple motion to curtains.
        p += float2(
            sin(p.y * 3.0 + animTime * 0.7),
            cos(p.x * 2.0 + animTime * 0.5)
        ) * AuroraDistortionStr * 0.12;

        float rzt = AuroraTriNoise2D(p, animTime);

        // AuroraBandSharpness: higher = narrower, more intense curtain bands.
        rzt = pow(saturate(rzt / 0.55), AuroraBandSharpness) * 0.55;

        // Per-step color from the existing preset system.
        // heightFrac maps step index to a position in the color gradient.
        float heightFrac = saturate(fi / (float)numSteps);
        float4 col2 = float4(0.0, 0.0, 0.0, rzt);
        col2.rgb = GetAuroraColor(heightFrac, AuroraColorPreset) * rzt * AuroraColorIntensity;

        // Running average — blendWeight controls crispness vs. softness.
        avgCol = lerp(avgCol, col2, blendWeight);

        // Accumulate with exponential falloff and smooth fade-in.
        col += avgCol * exp2(-fi * 0.065 - 2.5) * smoothstep(0.0, 5.0, fi);
    }

    // Horizon fade: aurora fades softly toward the horizon (similar to alto cloud HorizonFade).
    // AuroraHorizonFade [0,2] controls the width of the fade band above the horizon.
    //   0   → no fade (sharp cutoff at horizon)
    //   1.0 → default: full visibility reached ~9° above horizon, gone at horizon
    //   2.0 → very gradual: full visibility reached ~17° above horizon
    float horizonT    = saturate(rd.y / max(AuroraHorizonFade * 0.15, 0.001));
    float horizonMask = horizonT * horizonT * (3.0 - 2.0 * horizonT); // smoothstep
    col *= horizonMask;

    // Apply brightness, intensity, and night visibility.
    float3 result = col.rgb * AuroraBrightness * AuroraIntensity * AuroraVisibility * 1.8;

    // Saturation control.
    float luminance = dot(result, float3(0.2126, 0.7152, 0.0722));
    result = lerp(float3(luminance, luminance, luminance), result, AuroraSaturation);

    return max(result, float3(0.0, 0.0, 0.0));
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

// ---------------------------------------------------------------------------
// Aurora pass — separate additive fullscreen draw
// ---------------------------------------------------------------------------
// Allows aurora to be rendered independently of the sky dome, as an additive
// layer on top of whatever sky is currently rendered. The CB (b10) is filled
// each frame by UpdateAtmosphericSkyBuffer on the C++ side.

VSOutput VSAurora(VSInput input)
{
    return VSAtmosphericSky(input);
}

float4 PSAurora(VSOutput input) : SV_TARGET
{
    float3 viewDir = GetViewDirection(input.UV);
    float3 aurora = ComputeAurora(viewDir, input.Position.xy);
    // Output additive color — alpha is unused in additive blend mode.
    return float4(aurora, 1.0);
}
