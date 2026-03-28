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

// ---------------------------------------------------------------------------
// Samplers (reuse existing engine samplers)
// ---------------------------------------------------------------------------

Texture2D SceneColorTexture : register(t0);  // Half-res cloud RGBA (from cloud pass)
Texture2D CloudTexture      : register(t1);  // (unused in composite)
Texture2D DepthTexture      : register(t2);  // Scene depth (for composite masking)
Texture2D SceneBackgroundTex : register(t3); // Full-res scene before cloud composite
SamplerState PointSamp	  : register(s1);  // Point sampler
SamplerState LinearSamp	 : register(s2);  // Linear sampler

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

// ===========================================================================
// Noise functions purely procedural, no texture lookups
// ===========================================================================

// Hash function for 3D -> 1D (good distribution, fast).
float Hash31(float3 p)
{
    p = frac(p * float3(0.1031f, 0.1030f, 0.0973f));
    p += dot(p, p.yzx + 33.33f);
    return frac((p.x + p.y) * p.z);
}

// Smooth 3D value noise.
float ValueNoise3D(float3 p)
{
    float3 ip = floor(p);
    float3 f  = frac(p);

    // Quintic Hermite for smooth derivatives.
    float3 u = f * f * f * (f * (f * 6.0f - 15.0f) + 10.0f);

    // 8 corner hashes.
    float n000 = Hash31(ip + float3(0, 0, 0));
    float n100 = Hash31(ip + float3(1, 0, 0));
    float n010 = Hash31(ip + float3(0, 1, 0));
    float n110 = Hash31(ip + float3(1, 1, 0));
    float n001 = Hash31(ip + float3(0, 0, 1));
    float n101 = Hash31(ip + float3(1, 0, 1));
    float n011 = Hash31(ip + float3(0, 1, 1));
    float n111 = Hash31(ip + float3(1, 1, 1));

    // Trilinear interpolation.
    float n00 = lerp(n000, n100, u.x);
    float n01 = lerp(n001, n101, u.x);
    float n10 = lerp(n010, n110, u.x);
    float n11 = lerp(n011, n111, u.x);
    float n0  = lerp(n00, n10, u.y);
    float n1  = lerp(n01, n11, u.y);

    return lerp(n0, n1, u.z);
}

// Billow noise: absolute-value form of value noise.
// Converts [0,1] range into a ridge-like [0,1] form peaked at 0, mirroring
// abs(snoise()) from the reference volumetric cloud shader. The resulting FBM
// produces rounded cauliflower-like bumps the defining basis for the
// cotton-ball altocumulus cloud look of the reference shader.
float BillowNoise3D(float3 p)
{
    return abs(ValueNoise3D(p) * 2.0f - 1.0f);
}

// Gradient hash: maps a 3D integer grid point to a pseudorandom unit gradient vector.
// Used by PerlinNoise3D.
float3 GradHash33(float3 p)
{
    p = frac(p * float3(0.1031f, 0.1030f, 0.0973f));
    p += dot(p, p.yzx + 33.33f);
    float3 h = frac(float3(
        (p.x + p.y) * p.z,
        (p.x + p.z) * p.y,
        (p.y + p.z) * p.x
    )) * 2.0f - 1.0f;
    // Normalize to unit sphere so all gradient magnitudes are equal.
    // length(h) >= eps is guaranteed because the hash distributes away from zero.
    return normalize(h);
}

// Classic Perlin gradient noise, output remapped to approximately [0, 1].
//
// WHY this matters for Alto clouds:
//   ValueNoise3D has iso-contours that are grid-axis-aligned rectangles.
//   At low octave count (distant clouds, large AltoCloudSize), those rectangular
//   iso-contours dominate and produce jigsaw shapes regardless of curl warp /
//   Worley erosion, because those tools only reshape existing iso-contour topology.
//
//   Perlin gradient noise is constructed via dot(gradient, offset) rather than
//   interpolated scalar values, so its iso-contours are roughly spherical and
//   rotationally symmetric - matching the reference shader s abs(snoise()) whose
//   snoise is Simplex noise (another gradient variant). Near and far, at any
//   octave count, gradient-noise iso-contours remain organic.
//
//   abs(PerlinNoise3D * 2 - 1) = Perlin billow = direct equivalent of abs(snoise).
float PerlinNoise3D(float3 p)
{
    float3 ip = floor(p);
    float3 f  = frac(p);
    float3 u  = f * f * f * (f * (f * 6.0f - 15.0f) + 10.0f); // quintic

    float n000 = dot(GradHash33(ip + float3(0,0,0)), f - float3(0,0,0));
    float n100 = dot(GradHash33(ip + float3(1,0,0)), f - float3(1,0,0));
    float n010 = dot(GradHash33(ip + float3(0,1,0)), f - float3(0,1,0));
    float n110 = dot(GradHash33(ip + float3(1,1,0)), f - float3(1,1,0));
    float n001 = dot(GradHash33(ip + float3(0,0,1)), f - float3(0,0,1));
    float n101 = dot(GradHash33(ip + float3(1,0,1)), f - float3(1,0,1));
    float n011 = dot(GradHash33(ip + float3(0,1,1)), f - float3(0,1,1));
    float n111 = dot(GradHash33(ip + float3(1,1,1)), f - float3(1,1,1));

    float n00 = lerp(n000, n100, u.x);
    float n01 = lerp(n001, n101, u.x);
    float n10 = lerp(n010, n110, u.x);
    float n11 = lerp(n011, n111, u.x);
    float n0  = lerp(n00,  n10,  u.y);
    float n1  = lerp(n01,  n11,  u.y);
    // Perlin output range is approximately [-0.5, 0.5] for unit gradients in 3D.
    // Clamp with saturate after remapping to avoid sub-zero values at corners.
    return saturate(lerp(n0, n1, u.z) + 0.5f);
}

// Curl noise: computes the 2D curl of ValueNoise3D used as a scalar potential.
// Returns a divergence-free displacement vector in the XZ plane.
// Because curl(F) has zero divergence, no flow convergence points exist,
// so domain-warped iso-contours form smooth organic curves instead of
// L/T/jigsaw corners. This is the standard AAA domain-warping technique
// (Guerrilla Games, Frostbite, RDR2 cloud systems).
float2 CurlNoise2D(float3 p, float eps)
{
    float nx0 = ValueNoise3D(p + float3( eps, 0.0f,  0.0f));
    float nx1 = ValueNoise3D(p + float3(-eps, 0.0f,  0.0f));
    float nz0 = ValueNoise3D(p + float3( 0.0f, 0.0f,  eps));
    float nz1 = ValueNoise3D(p + float3( 0.0f, 0.0f, -eps));
    // 2D curl in XZ plane: curl_x = dN/dz,  curl_z = -dN/dx.
    return float2((nz0 - nz1) / (2.0f * eps), -(nx0 - nx1) / (2.0f * eps));
}

// Worley (cellular) noise in the XZ plane.
// Returns the normalized minimum distance to the nearest random point in a
// cell grid. Inverted (1 - worley) yields smooth rounded blobs - natural
// cloud-puff shapes. Altocumulus is physically a cellular cloud type, so
// Worley noise matches its real morphology (Andrew Schneider, Siggraph 2015:
// "The Real-Time Volumetric Cloudscapes of Horizon Zero Dawn").
float WorleyNoise2D(float2 p)
{
    float2 ip = floor(p);
    float minDist = 8.0f;
    for (int dy = -1; dy <= 1; dy++)
    for (int dx = -1; dx <= 1; dx++)
    {
        float2 cell = ip + float2(dx, dy);
        float2 pt   = cell + frac(sin(float2(
            dot(cell, float2(127.1f, 311.7f)),
            dot(cell, float2(269.5f, 183.3f))
        )) * 43758.5453123f);
        minDist = min(minDist, length(p - pt));
    }
    return saturate(minDist); // [0, ~1.0]
}

// Low-frequency FBM (3 octaves) " cloud shape.
// Persistence reduced to 0.38 so higher octaves barely affect the silhouette;
// only the first (dominant) octave drives large stable cloud masses.
//
// lod [0,1]: distance-based LOD for Moir prevention.
//   When a cloud sample is far from the camera, the step size of the primary
//   ray march becomes comparable to the wavelength of the finest octave.
//   That sub-step-size noise aliases into shimmering interference bands.
//   At lod=0 (near): all three octaves at full weight.
//   At lod=0.5:	  oct=2 (finest, 5.62x freq) fully silent.
//   At lod=1.0:	  oct=1 (2.37x freq) also silent; only the first
//					(coarsest) octave drives cloud shapes.
float FBMLowFreq(float3 p, float lod)
{
    float v  = 0.0f;
    float a  = 0.5f;
    float3 s = p;

    [unroll]
    for (int oct = 0; oct < 3; oct++)
    {
        // oct=0 (coarsest, ~1/ShapeScale wavelength): always full.
        // oct=1 (2.37x freq): fades to zero at lod=1.0 (far cloud regions).
        // oct=2 (5.62x freq): fades to zero at lod=0.5 (medium distance).
        float octWeight;
        if	  (oct == 0) octWeight = 1.0f;
        else if (oct == 1) octWeight = saturate(1.0f - lod);
        else			   octWeight = saturate(1.0f - lod * 2.0f);
        v += a * octWeight * ValueNoise3D(s);
        s *= 2.37f;  // Lacunarity (non-power-of-2 avoids tiling artifacts)
        a *= 0.38f;  // Low persistence: each octave contributes much less than the previous
    }
    return v;
}

// Billow FBM 3-octave FBM using BillowNoise3D as basis function.
// Lacunarity 2.032 matches the reference volumetric cloud shader (vs 2.37 for
// regular FBM), giving less spectral compression and more distinct scale
// separation. Gain 0.5 matches reference shader init_gain / gain = 0.5.
// Distance LOD suppresses fine octaves at range, same as FBMLowFreq.
float FBMLowFreqBillow(float3 p, float lod)
{
    float v  = 0.0f;
    float a  = 0.5f;
    float3 s = p;

    [unroll]
    for (int oct = 0; oct < 3; oct++)
    {
        float octWeight;
        if	  (oct == 0) octWeight = 1.0f;
        else if (oct == 1) octWeight = saturate(1.0f - lod);
        else			   octWeight = saturate(1.0f - lod * 2.0f);
        v += a * octWeight * BillowNoise3D(s);
        s *= 2.032f;  // Reference shader lacunarity
        a *= 0.5f;	// Reference shader gain
    }
    return v;
}

// Altocumulus 5-octave FBM using Perlin gradient noise.
// Replaces the former value-noise implementation to match the reference
// shader s abs(snoise()) whose snoise is a gradient-noise variant.
//
// Value noise: iso-contours are grid-axis-aligned rectangles (jigsaw).
// Gradient (Perlin) noise: iso-contours are roughly spherical = organic.
// This difference is the primary cause of the angular/jigsaw appearance at
// distance and large AltoCloudSize, where curl warp / Worley erosion cannot
// fully compensate for the underlying grid topology of value noise.
//
// billowBlend [0,1]: 0 = smooth Perlin, 1 = abs(Perlin) = Perlin billow.
// Default AltoBillowStrength=0.75 blends 75% billow for the cauliflower look.
// Lacunarity and gain are configurable (reference: 2.6434, 0.5).
// Distance LOD progressively reduces fine octaves with a minimum floor on
// oct2/oct3 to ensure frequency diversity at all distances.
float FBMAlto5(float3 p, float lacunarity, float gain, float billowBlend, float lod)
{
    float v  = 0.0f;
    float a  = 0.5f;
    float3 s = p;

    [unroll]
    for (int oct = 0; oct < 5; oct++)
    {
        // Distance LOD: reduce finer octaves at range.
        // Oct 0-1 (coarsest): always full weight.
        // Oct 2: floor at 0.35 - keeps 3+ active octaves at all distances.
        // Oct 3: floor at 0.15 - small mid-freq contribution at distance.
        // Oct 4 (finest): fully suppressible - highest moire risk.
        float octWeight;
        if      (oct <= 1) octWeight = 1.0f;
        else if (oct == 2) octWeight = max(saturate(1.0f - lod),        0.35f);
        else if (oct == 3) octWeight = max(saturate(1.0f - lod * 1.5f), 0.15f);
        else               octWeight = saturate(1.0f - lod * 2.0f);

        float pN   = PerlinNoise3D(s);
        float valN = pN;
        float bilN = abs(pN * 2.0f - 1.0f); // Perlin billow = abs(snoise) equivalent

        v += a * octWeight * lerp(valN, bilN, billowBlend);
        s *= lacunarity;
        a *= gain;
    }
    return v;
}

// High-frequency FBM (2 octaves) detail erosion.
// Persistence reduced to 0.35 (was 0.5).
//
// lod [0,1]: same distance LOD as FBMLowFreq but more aggressive.
//   Detail noise runs at a much finer scale than shape noise, so it aliases
//   even at shorter distances. The second (finest) octave is suppressed by
//   lod=0.5; the first detail octave fades fully by lod=1.
//   At medium viewing distances (lod???0.4) both octaves are already partially
//   muted, removing the high-contrast detail bands that produce Moir??.
float FBMDetail(float3 p, float lod)
{
    float v  = 0.0f;
    float a  = 0.5f;
    float3 s = p;

    [unroll]
    for (int oct = 0; oct < 2; oct++)
    {
        // oct=0 (coarser): fades to 0 at lod=1.
        // oct=1 (finer):   fades to 0 at lod=0.5 (2x faster).
        float octWeight = saturate(1.0f - lod * (1.0f + (float)oct));
        v += a * octWeight * ValueNoise3D(s);
        s *= 2.73f;
        a *= 0.35f;  // Persistence: lower second-octave weight reduces aliasing shimmer
    }
    return v;
}

// ===========================================================================
// Utility functions
// ===========================================================================

// Remap a value from [low1, high1] to [low2, high2], clamped.
float Remap(float value, float low1, float high1, float low2, float high2)
{
    float t = saturate((value - low1) / max(high1 - low1, 0.0001f));
    return lerp(low2, high2, t);
}

// Height fraction [0,1] within cloud layer.
// In TEN's Y-down space: bottomY is the slab's lower face (highest Y = nearest ground).
// Returns 0 at the bottom face, 1 at the top face.
float HeightFraction(float worldY, float bottomY, float thickness)
{
    return saturate((bottomY - worldY) / max(thickness, 1.0f));
}

// Height-based density gradient: rounded-bottom cumulus-like profile.
// Produces soft ramp-in at bottom, plateau in middle, smooth fade at top.
// Now dispatches per CloudType for type-authentic vertical density envelopes.
//
// CloudType 0 (None):				  generic cumulus fallback
// CloudType 1 (AltocumulusMid):		rounded cumulus, slight bottom bias
// Evaluate height gradient with per-column noise offsets so that the
// visible bottom and top boundaries of the cloud vary organically.
// bShift [-0.18..+0.18]: raises/lowers the start of the bottom fade-in.
// tShift [-0.12..+0.12]: raises/lowers the start of the top fade-out.
float HeightGradient(float heightFrac, float bShift, float tShift)
{
    if (CloudType == 1) // AltocumulusMid
    {
        // Mid-layer patch field: softer rounded cloudlets with moderate thickness.
        float bottom = smoothstep(0.06f + bShift * 0.7f, 0.28f + bShift * 0.6f, heightFrac);
        float top	= 1.0f - smoothstep(0.58f + tShift * 0.8f, 0.90f + tShift * 0.5f, heightFrac);
        return bottom * top;
    }
    else // None / default
    {
        float bottom = smoothstep(0.0f + bShift, 0.22f + bShift, heightFrac);
        float top	= 1.0f - smoothstep(0.70f + tShift, 1.0f + tShift * 0.4f, heightFrac);
        return bottom * top;
    }
}

// ===========================================================================
// Cloud density sampling
// ===========================================================================

float CloudDensityAtWorldPos(float3 worldPos, float heightFrac, bool useDetail, float skyH)
{
    // --- Sky-space coordinates for stable cloud anchoring ---
    // The cloud slab follows the camera, so worldPos (= CamPos + rayDir*t)
    // shifts with camera movement.  Subtracting the camera position gives
    // a position that depends only on the ray direction and intersection
    // distance, making the noise pattern behave like an infinitely distant
    // sky dome stable during camera translation.
    float3 skyPos = worldPos - CamPositionWS.xyz;

    // --- Distance LOD (computed early needed for distance-based softening) ---
    // Metric: horizontal (XZ-plane) distance from camera to sample.
    //   At CloudBottomHeight * 1.0 (???45?deg elevation) ??? lod = 0, full quality.
    //   At CloudBottomHeight * 6.0 (??? 9?deg elevation)  ??? lod = 1, fine octaves silent.
    // lodNear lowered from 1.5 to 1.0 so distance softening kicks in earlier:
    //   At 20?deg elevation distLOD ??? 0.35 (was 0.28) ??? medium-distance clouds
    //   already receive meaningful detail reduction and edge softening.
    float horizDist = length(skyPos.xz);
    float lodNear   = max(CloudBottomHeight * 1.0f, 1.0f);
    float lodFar	= max(CloudBottomHeight * 6.0f, lodNear + 1.0f);
    float distLOD   = saturate((horizDist - lodNear) / (lodFar - lodNear));

    // Squared distance factor: gentle at medium range, strong at far range.
    // Used by edge-width, minVisible, and coverage softening below.
    float distLOD2  = distLOD * distLOD;

    // ===================================================================
    // AltocumulusMid FULLY SELF-CONTAINED density path.
    // Ported from reference shader. Uses ONLY Alto-specific CB parameters.
    // Shared params used: CloudBottomHeight (slab position), DistanceFade,
    // WindDirection/WindSpeed/CloudTime (animation infrastructure).
    // Does NOT use: Coverage, CloudDensity, CloudThickness, Absorption,
    // ShapeScale, DetailScale, WeatherScale, AmbientContrib, SilverliningStr,
    // HeightGradient, or any other shared cloud-type parameters.
    // ===================================================================
    if (CloudType == 1)
    {
        // AltoCloudSize=1.0 ??? reference scale (pos*0.001). <1=bigger, >1=smaller.
        // --- Sky-height redistribution (bias-based) ---
        // biasFactor: Hermite ramp within the ACTIVE cloud zone [horizonEdge, zenithEdge].
        //   skyH <= horizonEdge -> biasFactor = -1  (horizon plateau)
        //   skyH >= zenithEdge  -> biasFactor = +1  (zenith plateau)
        //   between             -> smooth S-curve transition
        //
        // horizonEdge: derived from HorizonFade and DistanceFade, which together define
        // the outermost visible cloud boundary. The atmospheric horizon fade covers
        // elevation [0, ~0.10] and DistanceFade fades the outermost (lowest-elevation)
        // clouds first. Both parameters push the active boundary inward.
        //   HorizonFade=0, DistanceFade=0 -> horizonEdge=0 (starts right at the horizon)
        //   HorizonFade=1, DistanceFade=1 -> horizonEdge=0.16 (~9deg elevation)
        //
        // zenithEdge: a flat plateau so the 'zenith' is a broad zone, not a single point.
        // 0.65 in skyH-space corresponds to ~40deg elevation above the horizon, giving a
        // wide central dome that the bias can push clouds toward or away from.
        float horizonEdge = saturate(HorizonFade * 0.10f + DistanceFade * 0.06f);
        static const float zenithEdge = 0.65f;
        float activeRange = max(zenithEdge - horizonEdge, 0.05f);
        float biasFactor;
        {
            float t = saturate((skyH - horizonEdge) / activeRange);
            float s = t * t * (3.0f - 2.0f * t); // smoothstep
            biasFactor = s * 2.0f - 1.0f;         // remap [0,1] -> [-1,+1]
        }

        // Density redistribution via coverage threshold shift.
        // AltoCloudAmount is the global base; bias shifts where clouds are denser/sparser.
        // bias=0: effectiveAmount = AltoCloudAmount everywhere (uniform, no change).
        // bias>0: denser near horizon, sparser near center.
        // bias<0: sparser near horizon, denser near center.
        //
        // NOTE: sizeRatio / noise-space scaling is intentionally NOT used here.
        // Sampling the noise field at a position-dependent zoom level creates a
        // visible ring artifact at the point where the zoom crosses 1.0, because
        // adjacent pixels suddenly sample from incompatible frequency levels.
        // The visual "size" difference between horizon and center clouds already
        // exists naturally through perspective (distant = small, close = large).
        // All distribution effects are therefore expressed as threshold shifts
        // (densityShift, covSoft) on the same uniformly-sampled noise field.
        float densityShift = -AltoZenithBias * biasFactor * 0.4f;

        float baseScale = 0.001f * AltoCloudSize;
        // Wind drift in noise-space. WindSpeed in the CB carries the pre-integrated
        // wind offset (accumulated on the CPU each frame as WindSpeed * dt).
        float3 windOfs  = float3(WindDirection.x, 0.0f, WindDirection.y) * WindSpeed;
        float3 p        = skyPos * baseScale + windOfs;

        // Domain warping via curl noise (divergence-free) eliminates the
        // grid-aligned jigsaw artifacts that value-noise displacement produces.
        // Because curl(F) has zero divergence, iso-contour deformation never
        // creates convergence points where edges pinch into L/T/puzzle corners.
        //
        // Three passes target three distinct frequency bands:
        //   Pass 1 (coarse, 0.41x): large organic sweep of each cloud mass.
        //   Pass 2 (medium, 0.80x): billow detail within each mass.
        //   Pass 3 (fine,   1.60x): targets cell-to-cell boundary iso-contours
        //     specifically the frequency band where value-noise grid alignment
        //     is most visible when AltoCloudSize is large and many cells fill
        //     the screen. Small eps gives a higher-frequency curl field so
        //     individual cell edges are broken into short curved segments.
        {
            float3 pW  = float3(p.x, 0.0f, p.z) * 0.41f;
            float2 c1  = CurlNoise2D(pW  + float3(3.17f, 0.0f, 7.63f), 0.15f);
            p.x += c1.x * 0.22f;
            p.z += c1.y * 0.22f;

            float3 pW2 = float3(p.x, 0.0f, p.z) * 0.80f;
            float2 c2  = CurlNoise2D(pW2 + float3(0.59f, 0.0f, 2.44f), 0.10f);
            p.x += c2.x * 0.09f;
            p.z += c2.y * 0.09f;

            float3 pW3 = float3(p.x, 0.0f, p.z) * 1.60f;
            float2 c3  = CurlNoise2D(pW3 + float3(5.33f, 0.0f, 1.88f), 0.06f);
            p.x += c3.x * 0.035f;
            p.z += c3.y * 0.035f;
        }

        // Reference: float dens = fbm_clouds(p * 2.032, 2.6434, .5, .5);
        // 5 octaves of abs(snoise(p)) billow FBM.
        float dens = FBMAlto5(p * 2.032f, AltoFbmLacunarity, AltoFbmGain,
                              AltoBillowStrength, distLOD);

        // Dual-scale Worley cellular erosion (Guerrilla "Horizon Zero Dawn", Siggraph 2015).
        // Two scales are needed because a single scale only shapes ONE frequency of cloud:
        //
        // Scale A (0.55x FBM) -> coarse puff cells: carves large cloud masses into
        //   individual billowing mounds. Covers large-coverage regions well.
        //
        // Scale B (1.05x FBM) -> fine fragment cells: small cloud fragments near the
        //   coverage threshold have very little density excess above covThresh, so the
        //   coarse Worley barely touches them. A finer scale Worley erosion specifically
        //   breaks those thin fragments into small rounded puffs instead of flat slabs.
        //   Strength scales with distLOD: at distance the surviving FBM octaves are fewer
        //   (even with the floor weights) so grid-aligned edges are relatively more
        //   prominent -> a stronger Worley B compensates by increasing cellular erosion.
        //
        // Remap lower-bound shift: positive invWorley -> more density at cell centers
        // (creates rounded mounds), near-zero invWorley at cell walls -> thin separating
        // gaps between puffs. Applied BEFORE the coverage smoothstep so the shapes are
        // carved before the threshold cut.
        {
            float2 wPos     = float2(p.x, p.z) * 2.032f;
            float worleyA   = WorleyNoise2D(wPos * 0.55f);
            float invWA     = 1.0f - saturate(worleyA * 1.3f);
            dens = saturate(Remap(dens, -(invWA * 0.30f), 1.0f, 0.0f, 1.0f));

            float worleyB   = WorleyNoise2D(wPos * 1.05f);
            float invWB     = 1.0f - saturate(worleyB * 1.4f);
            // Constant strength (no distLOD dependence): distLOD was used here as a
            // Value-Noise workaround but caused measurable coverage drop at distance
            // even at AltoZenithBias=0, breaking the uniform-at-0 guarantee.
            // With Perlin gradient noise the iso-contour shape is already organic at
            // all distances, so a flat 0.15 gives equal erosion near/far.
            dens = saturate(Remap(dens, -(invWB * 0.15f), 1.0f, 0.0f, 1.0f));
        }

        // Reference: dens *= smoothstep(cld_coverage, cld_coverage + .035, dens);
        // Self-referential smoothstep: THE signature look of this shader.
        // AltoCloudAmount controls fill: 0=sparse, 1=overcast.
        // covThreshold = 1.0 - amount: higher amount -> lower threshold -> more clouds.
        // Default amount=0.6875 -> thresh=0.3125 -> matches reference cld_coverage.
        // AltoCloudAmount always controls global density; bias only shifts the spatial balance.
        float effectiveAmount = saturate(AltoCloudAmount + densityShift);
        float covThresh = saturate(1.0f - effectiveAmount);
        // Edge-softness evolution: cloud edges widen where clouds are building/forming
        // and tighten where they are dissipating/breaking apart.
        // evoBias: +1 = this sky region is favored by the current distribution bias
        //          -1 = unfavored     0 = neutral (bias == 0 or at mid-sky)
        //  evoBias=+1 -> exp2(+0.5) ~ 1.41x covSoft -> diffuse, spread-out edges
        //  evoBias=-1 -> exp2(-0.5) ~ 0.71x covSoft -> crisp, tight edges
        //  evoBias= 0 -> exp2(0)    = 1.00x covSoft -> unchanged (bias=0 parity)
        // Clamped to [0.3, 3.0] to prevent degenerate coverage response.
        float evoBias = -AltoZenithBias * biasFactor;
        float covSoft = max(AltoCovSoftWidth, 0.001f) * clamp(exp2(evoBias * 0.5f), 0.3f, 3.0f);

        // Evolution / pulsing: slowly oscillate the coverage threshold so cloud
        // masses gently puff up and deflate over time.
        // EvolutionSpeed=0 ??? static. Higher values ??? faster / stronger pulsing.
        // A coarse spatial noise gives each region its own phase so puffing is
        // locally independent (not a global in/out sync).
        if (EvolutionSpeed > 0.001f)
        {
            float spatialPhase = ValueNoise3D(p * 0.4f) * 6.2832f;
            float swellPhase   = CloudTime * EvolutionSpeed * 0.04f + spatialPhase;
            float swellAmp	 = 0.08f * saturate(EvolutionSpeed * 0.5f + 0.1f);
            // No distLOD damping: damping created a near/far coverage difference at
            // AltoZenithBias=0 because near clouds expanded more during swell peaks.
            // Evolution speed is already a global parameter; uniform swell amplitude
            // keeps cloud "presence" consistent at all distances at bias=0.
            covThresh = saturate(covThresh - sin(swellPhase) * swellAmp);
        }

        dens *= smoothstep(covThresh, covThresh + covSoft, dens);

        // Reference shader does NOT use height gradient in density.
        // Height-dependent behavior is in illumination (exp(h)/1.95 + dark/bright blend).

        // --- Organic bottom shaping ---
        // AltoBottomSoftness [0,1]: 0 = flat slab bottom (no change),
        // 1 = fully organic, irregular underside sculpted by coarse noise.
        // Two-scale noise: coarse clusters set per-region depth, fine detail
        // adds local variation within each cluster so different cloud groups
        // clearly hang at different heights rather than all equally deep.
        if (AltoBottomSoftness > 0.001f)
        {
            float3 basePos	= skyPos + windOfs / baseScale;
            // Coarse scale: large cluster-level depth variation (~every few km).
            float3 coarsePos  = basePos * 0.000045f;
            // Fine scale: column-level detail within each cluster.
            float3 detailPos  = basePos * 0.00012f;
            float coarseNoise = ValueNoise3D(float3(coarsePos.x,  0.0f, coarsePos.z));
            float detailNoise = ValueNoise3D(float3(detailPos.x,  0.0f, detailPos.z));

            // Coarse dominates so whole cloud clusters are visibly deep or shallow.
            float combinedNoise = saturate(coarseNoise * 0.65f + detailNoise * 0.35f);

            // Per-column bottom threshold in heightFrac space [0=slab floor, 1=top].
            // combinedNoise=0  deep (threshold near depthBase),
            // combinedNoise=1  shallow (threshold raised by depthRange).
            // depthBase > 0 ensures even the deepest clouds stay slightly above the
            // hard slab floor  "raising the deepest point a little" as requested.
            float depthBase  = 0.05f;
            float depthRange = AltoBottomSoftness * 0.35f;
            float threshold  = depthBase + combinedNoise * depthRange;

            float fadeRange  = lerp(0.03f, 0.30f, AltoBottomSoftness);
            float bottomFade = smoothstep(threshold, threshold + fadeRange, heightFrac);
            dens *= bottomFade;
        }

        // --- Smooth crown (top fade) ---
        // Without a top-fade the slab has a hard cutoff at heightFrac=1.0: when AltoThickness
        // is large the iso-contours of the flat density field become visible as jigsaw edges.
        // A smoothstep crown fade mirrors AltoBottomSoftness for the top, giving each cloud
        // mass a naturally tapering, puffy upper boundary regardless of AltoThickness.
        // Crown fade starts at 65% height and reaches zero at the slab top.
        dens *= 1.0f - smoothstep(0.65f, 1.0f, heightFrac);

        if (dens <= 0.0001f)
            return 0.0f;

        // AltoHorizonWidth zenith cap (disabled in bleed pass — bleed clouds must be
        // visible from near-horizontal rays that point toward the mountains).
        if (AltoHorizonWidth > 0.001f && CloudIsBleedPass < 0.001f)
        {
            float altoCapEdge = AltoHorizonWidth * 0.90f;
            dens *= smoothstep(altoCapEdge - 0.08f, altoCapEdge + 0.08f, skyH);
        }

        // --- Wind-directional drift-out dissolution ---
        // When DriftOutProgress > 0, a soft boundary sweeps from the upwind side
        // (where new clouds would scroll in from) toward the downwind side.
        // Upwind clouds dissolve first; downwind clouds linger and fade via
        // the CPU-side coverage/density reduction over the drift-out duration.
        //
        // Cloud motion: p = skyPos*scale + windDir*accum  →  as accum grows,
        // noise patterns move in -windDir in sky-space.  Therefore the +windDir
        // side of the sky is "upwind" (source of new clouds).
        if (DriftOutProgress > 0.001f)
        {
            float2 windDir2D = normalize(WindDirection);
            // Project sky position onto wind axis: positive = upwind (cloud source).
            float windProjSky = dot(skyPos.xz, windDir2D);
            // Normalize to roughly [-1, +1] across the visible cloud field.
            float fieldExtent = max(CloudBottomHeight * 4.0f, 1.0f);
            float normalizedProj = windProjSky / fieldExtent;

            // Boundary sweeps from far downwind (-1.5) toward far upwind (+1.5).
            // At progress=0: boundary at -1.5 → nothing suppressed.
            // At progress=1: boundary at +1.5 → everything suppressed.
            float boundary = lerp(-1.5f, 1.5f, DriftOutProgress);
            float softness = 0.4f;
            // Suppress everything UPWIND of the boundary.
            float suppress = smoothstep(boundary - softness, boundary + softness, normalizedProj);
            dens *= (1.0f - suppress);
        }

        return dens;
    }

    // --- Coverage / Weather noise ---
    // Large-scale weather map controls where clouds exist.
    float2 weatherUV = skyPos.xz * WeatherScale 
                      + WindDirection * WindSpeed * 0.3f;
    float weatherNoise = ValueNoise3D(float3(weatherUV, 0.0f));

    // Remap weather noise with coverage parameter.
    // Higher coverage -> more clouds, but always with variation.
    float coverageMask = Remap(weatherNoise, 1.0f - Coverage, 1.0f, 0.0f, 1.0f);
    // Distance-softened coverage exponent: at close range the exponent shapes
    // the cloud boundary normally.  At far range (distLOD???1) the exponent is
    // reduced to 60% of its base value, widening the density gradient across
    // the weather boundary so far cloud edges are broader and softer.
    float coverageExp = lerp(1.2f, 0.85f, Coverage);
    coverageExp *= lerp(1.0f, 0.6f, distLOD2);
    coverageMask = pow(saturate(coverageMask), coverageExp);

    // --- Height-dependent coverage narrowing ---
    // Real cumulus clouds have a broad base and a narrowing rising body.
    // Reducing effective coverage with heightFrac creates natural tapering:
    // the base footprint is maximum, the crown is smaller and more rounded.
    // Stratus (3) and Cirrus (1) are horizontal sheets  no tapering.
    // Amounts increased from previous pass: stronger tapering makes the
    // lateral shape difference between base and crown more visible.
    {
        float heightTaper = 0.0f;
        if (CloudType == 0) heightTaper = heightFrac * 0.30f;			  // Default/generic
        coverageMask = saturate(coverageMask * (1.0f - heightTaper));
    }

    if (coverageMask <= 0.001f)
        return 0.0f;

    // --- Soft-cap ShapeScale ---
    // Linear below 0.000135 (confirmed "good" range by artist).
    // Above that, sqrt-compress so doubling the UI value only gives sqrt(2)x
    // more noise frequency   prevents the pattern from becoming unpleasantly
    // tiny at high values while preserving fine-tuning at low values.
    static const float refShapeScale = 0.000135f;
    float effectiveShapeScale = (ShapeScale <= refShapeScale)
        ? ShapeScale
        : refShapeScale * sqrt(ShapeScale / refShapeScale);

    // --- Organic height gradient ---
    // Sample two very coarse, stable noise values from the horizontal sky
    // position (independent of ShapeScale and wind) to offset the visible
    // bottom and top boundaries per cloud column.
    //
    // Scale 0.000065/wu is much coarser than even the coarsest shape noise,
    // so the boundary variation is broad and gentle (hundreds of world-units
    // per noise period), giving the impression of large-scale cloud billows
    // rather than fine-grained distortion.
    //
    // bShift in [-0.18, +0.18]: negative = bottom starts lower (cloud extends
    //   further down in that column), positive = bottom is higher (more tapered).
    // tShift in [-0.12, +0.12]: negative = top fades earlier (rounded crown),
    //   positive = top extends a bit higher (swelling upward).
    //
    // Both use seed offsets (+7.3, +13.1) so bottom and top vary independently.
    float2 hgUV	= skyPos.xz * 0.000065f;
    float  bNoise  = ValueNoise3D(float3(hgUV.x,		hgUV.y,		7.3f));
    float  tNoise  = ValueNoise3D(float3(hgUV.x + 5.7f, hgUV.y + 3.2f, 13.1f));
    float  bShift  = (bNoise - 0.5f) * 0.36f;   // [-0.18, +0.18]
    float  tShift  = (tNoise - 0.5f) * 0.24f;   // [-0.12, +0.12]
    float hGrad = HeightGradient(heightFrac, bShift, tShift);

    // --- Base shape noise ---
    // Sampling position is purely driven by horizontal wind drift.
    // No Y-displacement here   any 3D shift of shapePos produces twisting artifacts
    // because the noise has structure in all three axes.
    //
    // Per-CloudType noise distortion applied to the shape sampling position.
    //   Default: isotropic (no distortion).
    //   (AltocumulusMid returns early above   never reaches this code.)
    float3 noiseScale = float3(1.0f, 1.0f, 1.0f);

    // Normalize the Y component of noise coordinates so the vertical noise
    // range is independent of CloudThickness. Without this, a thick slab
    // causes the noise to cycle multiple periods vertically, producing
    // distinct horizontal density layers (visible "stacked slices").
    // Clamping to 2500 (reference design thickness) fixes the Y range
    // while leaving XZ sampling   and all horizontal cloud structure   unchanged.
    float shapeY = -CloudBottomHeight - heightFrac * min(CloudThickness, 2500.0f);

    // --- Height-dependent horizontal deformation ---
    // A vertical ray traverses the cloud slab sampling shapePos at the same
    // (x, z) in noise-space at every heightFrac. When ShapeScale is small or
    // the density field is spatially smooth, every height returns nearly the
    // same cross-section   a "Minecraft column" / vertical-extrusion artifact.
    //
    // Root cause of the previous fix's failure:
    //   driftN = ValueNoise3D(float3(driftUV, 4.17f))   4.17 is CONSTANT.
    //   Every height level got the same drift angle; only the amplitude varied.
    //   The cloud leaned in one static direction rather than organically deforming.
    //
    // Fix: two independent noise channels (driftX, driftZ) both include
    // heightFrac in their sample domain, so the deformation DIRECTION rotates
    // as height increases.  The cloud leans one way at h=0.3, a different way
    // at h=0.6, and yet another at h=0.9   genuinely different cross-sections.
    //
    // Amplitude ~1.0 noise-unit RMS at crown (h=1): far enough that the crown
    // samples a clearly different region of the density field than the base,
    // giving distinct silhouettes even with very small ShapeScale.
    //
    // Scale: 0.000115/wu is fixed, independent of ShapeScale, so the
    // deformation works for any cloud scale setting.
    float2 driftUV   = skyPos.xz * 0.000115f;
    float  distAtt   = 1.0f - distLOD * 0.5f;
    float  driftX	= ValueNoise3D(float3(driftUV.x,		 driftUV.y,		 heightFrac * 2.1f + 0.73f)) - 0.5f;
    float  driftZ	= ValueNoise3D(float3(driftUV.x + 4.83f, driftUV.y + 2.31f, heightFrac * 2.1f		)) - 0.5f;
    float  driftAmt  = heightFrac * 2.8f * distAtt;
    float2 driftXZ   = float2(driftX, driftZ) * driftAmt;

    float2 shapeXZScale = noiseScale.xz;

    float3 shapePos = float3(skyPos.x * effectiveShapeScale * shapeXZScale.x + driftXZ.x,
                             shapeY   * effectiveShapeScale * noiseScale.y,
                             skyPos.z * effectiveShapeScale * shapeXZScale.y + driftXZ.y)
                     + float3(WindDirection.x, 0.0f, WindDirection.y) 
                       * WindSpeed;

    // CloudType == 1 (AltocumulusMid) returns early from the self-contained
    // density path above, so the code below is only reached by other types.

    float baseShape = FBMLowFreq(shapePos, distLOD);

    // First octave of the shape noise: the smoothest, lowest-frequency
    // component. This alone defines the stable cloud silhouette.
    // Weight 0.5 matches oct=0's amplitude in FBMLowFreq.
    float oct0Shape = 0.5f * ValueNoise3D(shapePos);

    // --- Billowing: threshold modulation (no sampling displacement) ---
    // Billowing is achieved by slowly oscillating the remap lower threshold.
    // This makes cloud masses expand and contract without moving the noise field,
    // avoiding all twisting / distortion artifacts.
    // Different regions puff independently via a coarse spatially-varying phase.
    float remapBase = 0.22f;
    float remapLow = remapBase;
    if (EvolutionSpeed > 0.001f)
    {
        // Phase noise sampled at much coarser scale than shape   one puff region
        // covers many cloud masses, giving a coherent swell without fragmentation.
        // Use the same Y-normalized position to keep billowing consistent.
        float phaseNoise = ValueNoise3D(float3(skyPos.x, shapeY, skyPos.z) * effectiveShapeScale * 0.25f);
        float swellPhase = CloudTime * EvolutionSpeed * 0.04f + phaseNoise * 6.2832f;

        // Max threshold shift: ??0.26 at EvolutionSpeed=1, ??0.42 at EvolutionSpeed=5.
        // Clamped so clouds never fully vanish or fully merge.
        // Scaled down at distance: far cloud regions have a much smaller sweep range.
        // This is critical   the billowing sweep moves shapeDensity through the
        // S-curve's steep zone (0.3??"0.7) repeatedly, creating bright/dark banding
        // at medium/far distance. Reducing swellAmp there avoids that oscillation.
        float swellAmp = 0.26f * saturate(EvolutionSpeed * 0.2f + 0.1f);
        // Quadratic damping: at medium distance (lod=0.5) swell is 25% of normal;
        // at far (lod=1.0) swell is zero.  Previous linear damping (1-lod*0.85)
        // left 57% at lod=0.5, which was enough to push the boundary density
        // back and forth between frames ??? temporal instability at far edges.
        swellAmp *= pow(1.0f - distLOD, 2.0f);
        remapLow = clamp(remapBase - sin(swellPhase) * swellAmp, 0.001f, 0.46f);
    }

    // --- Height-dependent crown erosion ---
    // Near the top of the cloud raise the remap-low threshold so the density
    // field is compressed more aggressively at the crown.  This produces the
    // cauliflower / broken-top effect that real tall clouds exhibit: the base
    // body is dense and continuous, but the crown dissolves into ragged puffs.
    // Skipped for Cirrus (already wispy, no crown) and Stratus (uniform slab).
    {
        float crownBias = heightFrac * heightFrac * 0.09f;
        remapLow = clamp(remapLow + crownBias, 0.001f, 0.46f);
    }

    // --- Stable silhouette blend ---
    // The visible cloud BOUNDARY is defined exclusively by octave 0   the
    // smoothest, lowest-frequency noise component. Higher FBM octaves add fine
    // interior structure but must never extend or retract the visible edge,
    // as that produces per-pixel crawling and noisy silhouettes during motion.
    //
    // oct0Density: where oct0 says "cloud exists". Smooth, stable boundary.
    // fullDensity: full multi-octave FBM. Rich interior detail.
    // edgeMask:	smoothstep ramp [0, 0.15] of oct0Density   controls how
    //			 much higher-octave detail is permitted at each point.
    //
    // Near the boundary (edgeMask ??? 0): density = oct0Density (smooth).
    // Deep inside (edgeMask = 1): density = fullDensity (full detail).
    // The wide transition band (15% of the remapped oct0 range) prevents any
    // visible seam where higher octaves suddenly appear.
    // During wind and billowing, the edge moves with oct0's smooth gradient,
    // not with noisy higher-octave features.
    float oct0Density  = Remap(oct0Shape, remapLow, 1.0f, 0.0f, 1.0f);
    float fullDensity  = Remap(baseShape, remapLow, 1.0f, 0.0f, 1.0f);

    // --- Absorption-widened silhouette zone ---
    // At the cloud boundary, oct0Density is very small (0.001??"0.05 range).
    // At low absorption these tiny values produce negligible opacity per step.
    // At high absorption, exp(-d * A * step) turns them into visible per-pixel
    // speckle because even d=0.02 at Abs=5 gives ~10% opacity per step.
    //
    // Fix: widen the oct0-only (smooth) boundary zone proportionally to
    // absorption.  At high Abs the smooth-edge region extends deeper into
    // the cloud body before higher octaves are introduced.
    //   edgeWidth = 0.25 (default) ??? 0.50 (at Abs???3.0)
    //
    // Widened from 0.15/0.35 ??? 0.25/0.50: the previous narrow zone (0.15)
    // allowed detail octaves to influence the silhouette too close to the
    // boundary, contributing high-frequency noise to the outer contour that
    // appeared as stairstepping and dithering when rendered at half resolution.
    // The wider zone (0.25 base) ensures the outer ~25% of oct0 range is always
    // driven by the smooth low-frequency oct0 alone, giving a wide feathered fade.
    float absEdgeWiden = saturate((Absorption - 0.5f) * 0.4f);
    float baseEdgeWidth = lerp(0.25f, 0.50f, absEdgeWiden);
    // Distance-widen the oct0-only smooth zone: at far range (distLOD???1),
    // edgeWidth approaches 0.80, meaning 80% of the oct0 density range uses
    // only the smooth first-octave component.  This is the primary fix for
    // jagged far silhouettes   higher FBM octaves are suppressed over a much
    // wider boundary band at distance, so the visible contour is driven
    // exclusively by the lowest-frequency, spatially-smooth noise field.
    float edgeWidth	= lerp(baseEdgeWidth, 0.80f, distLOD2);
    float edgeMask	 = smoothstep(0.0f, edgeWidth, oct0Density);
    float shapeDensity = lerp(oct0Density, fullDensity, edgeMask);

    // --- Distance- and absorption-softened S-curve ---
    // smoothstep(0,1,x) amplifies contrast (peak slope 1.5 at x=0.5).
    // At distance or high absorption, blend toward a linear ramp to prevent
    // the steep mid-range from creating banding during motion.
    {
        float absorpFade  = saturate((Absorption - 0.5f) * 0.4f);
        float sCurveFade  = max(distLOD, absorpFade);
        float ssValue	 = smoothstep(0.0f, 1.0f, shapeDensity);
        shapeDensity	  = lerp(ssValue, shapeDensity, sCurveFade);
    }

    shapeDensity *= coverageMask * hGrad;

    if (shapeDensity <= 0.0001f)
        return 0.0f;

    // --- Detail erosion (interior only) ---
    if (useDetail && DetailNoiseEnabled != 0)
    {
        // Apply same per-type noise distortion to detail sampling for consistency.
        // No Y-drift: vertical wobble was the primary source of edge warping.
        // Apply proportional drift (0.45x): keeps interior erosion patterns
        // consistent with the shape drift so eroded pockets also evolve with height.
        float3 detailPos = float3(skyPos.x * DetailScale * noiseScale.x + driftXZ.x * 0.45f,
                                  shapeY   * DetailScale * noiseScale.y,
                                  skyPos.z * DetailScale * noiseScale.z + driftXZ.y * 0.45f)
                          + float3(WindDirection.x, 0.0f, WindDirection.y) 
                            * CloudTime * EvolutionSpeed;
        // Pass distLOD: at medium/far distance FBMDetail progressively mutes its
        // octaves, so detail returns 0 at lod=1 without needing a separate branch.
        float detail = FBMDetail(detailPos, distLOD);

        // Erosion is strongest INSIDE cloud bodies and near-zero at the silhouette.
        // This preserves stable edges while allowing internal billowing texture.
        //
        // Two guards control how far inward the erosion zone begins:
        //   (a) distLOD via erosionWeight scalar   already applied
        //   (b) Absorption   when Abs is high, even a small density value becomes
        //	   opaque, so boundary noise that erosion carves into looks like hard
        //	   holes.  We move the interior mask threshold upward with absorption
        //	   so only genuinely dense core regions are ever eroded.
        //
        //   absorpEdgeGuard: 0 at Abs???0.5, 1 at Abs???3.0
        //   maskLow  moves from 0.35 (default) ??? 0.60 (high absorption)
        //   maskHigh moves from 0.65			??? 0.85
        //
        // Additionally, total erosion depth scales down with absorption via
        // absorpErosionScale, preventing detail noise from carving visible holes
        // in cloud silhouettes that the Beer-Lambert curve makes 
        // disproportionately dark.
        float absorpEdgeGuard  = saturate((Absorption - 0.5f) * 0.4f);
        float maskLow		  = lerp(0.35f, 0.60f, absorpEdgeGuard);
        float maskHigh		 = lerp(0.65f, 0.85f, absorpEdgeGuard);
        float interiorMask	 = smoothstep(maskLow, maskHigh, shapeDensity);
        float absorpErosionScale = saturate(1.0f - (Absorption - 0.5f) * 0.35f);

        // Per-CloudType erosion weight multiplier.
        float typeErosionMul = 1.0f;

        float erosionWeight	= interiorMask * DetailStrength * 0.40f
                                 * (1.0f - distLOD) * absorpErosionScale * typeErosionMul;
        shapeDensity = Remap(shapeDensity, erosionWeight * detail, 1.0f, 0.0f, 1.0f);
    }

    // --- Absorption-proportional soft density floor ---
    // At high absorption, Beer-Lambert turns even tiny density values into
    // visible per-pixel opacity spots (dithering / speckle at cloud edges).
    // Example: d=0.02, Abs=5.0, stepSize=500 ??? extinction=50 ??? fully opaque
    // from what should be an imperceptible boundary wisp.
    //
    // Fix: apply a soft threshold that smoothly fades very low density
    // values to zero. The threshold scales with Absorption so the
    // "clean" zone around zero widens as absorption increases.
    //   At Abs=0.5:  minVisible ??? 0.015 (small but meaningful suppression zone).
    //   At Abs=3.0+: minVisible ??? 0.10  (wider band ??? no speckle at edges).
    //
    // Widened from 0.005/0.06 ??? 0.015/0.10: the previous values left a narrow
    // "almost zero" density band visible at the silhouette. With a low step count
    // (12 steps) and large step size, each edge pixel either catches 1??"2 of these
    // near-zero samples or misses them entirely depending on jitter offset, which
    // creates the classic dithered / posterized boundary pattern. A wider floor
    // ensures only genuinely contributing density survives to the Beer-Lambert
    // integral, so the silhouette transition is smooth rather than speckled.
    // The smoothstep transition band is also widened (2x minVisible) rather than
    // equal to minVisible, for an even gentler fade-to-zero curve.
    float finalDensity = max(shapeDensity * CloudDensity, 0.0f);


    // Distance-widened density floor: at far range the minimum visible density
    // is raised so the thin boundary fringe that individual ray-march steps
    // either catch or miss is suppressed to zero.
    // Uses a linear ramp (saturate(d/threshold)) instead of smoothstep to avoid
    // introducing contour-band quantization from the S-curve plateau.
    float baseMinVisible = lerp(0.015f, 0.10f, saturate((Absorption - 0.5f) * 0.4f));
    float minVisible	 = lerp(baseMinVisible, max(baseMinVisible, 0.12f), distLOD2);
    finalDensity		*= saturate(finalDensity / max(minVisible, 0.0001f));

    // --- Wind-directional drift-out dissolution ---
    if (DriftOutProgress > 0.001f)
    {
        float2 windDir2D = normalize(WindDirection);
        float windProjSky = dot(skyPos.xz, windDir2D);
        float fieldExtent = max(CloudBottomHeight * 4.0f, 1.0f);
        float normalizedProj = windProjSky / fieldExtent;
        float boundary = lerp(-1.5f, 1.5f, DriftOutProgress);
        float softness = 0.4f;
        float suppress = smoothstep(boundary - softness, boundary + softness, normalizedProj);
        finalDensity *= (1.0f - suppress);
    }

    return finalDensity;
}

// ===========================================================================
// Cloud slab intersection   TEN uses Y-down coordinates
// ===========================================================================

// Intersect a ray with a flat horizontal cloud slab of given thickness.
// In TEN: Y increases downward, so clouds ABOVE the camera have lower Y values.
//   Slab bottom face (nearest ground)  = CamPositionWS.y - CloudBottomHeight
//   Slab top face   (furthest from ground) = bottom - thickness
// Returns (tEntry, tExit), or (-1, -1) on miss.
float2 IntersectCloudVolumeEx(float3 rayOrigin, float3 rayDir, float thickness)
{
    float slabBottom = CamPositionWS.y - CloudBottomHeight;
    float slabTop	= slabBottom - thickness;

    // Horizontal rays never cross the horizontal slab.
    if (abs(rayDir.y) < 0.0001f)
        return float2(-1.0f, -1.0f);

    float t0 = (slabBottom - rayOrigin.y) / rayDir.y;
    float t1 = (slabTop	- rayOrigin.y) / rayDir.y;

    float tNear = min(t0, t1);
    float tFar  = max(t0, t1);

    // Miss: slab entirely behind camera, or degenerate interval.
    if (tFar < 0.0f || tNear >= tFar)
        return float2(-1.0f, -1.0f);

    return float2(max(tNear, 0.0f), tFar);
}

float2 IntersectCloudVolume(float3 rayOrigin, float3 rayDir)
{
    return IntersectCloudVolumeEx(rayOrigin, rayDir, CloudThickness);
}

// ===========================================================================
// Henyey-Greenstein phase function (dual-lobe)
// ===========================================================================

float HenyeyGreenstein(float cosTheta, float g)
{
    float g2 = g * g;
    return (1.0f - g2) / (4.0f * PI * pow(1.0f + g2 - 2.0f * g * cosTheta, 1.5f));
}

float DualLobePhase(float cosTheta)
{
    // Per-CloudType phase function tuning:
    //   AltocumulusMid: standard water droplets   balanced dual-lobe.
    //   Default: use CB PhaseForward/PhaseBackward directly.

    float fwd	   = PhaseForward;
    float bk		= PhaseBackward;
    float fwdWeight = 0.7f;

    float forward  = HenyeyGreenstein(cosTheta, fwd);
    float backward = HenyeyGreenstein(cosTheta, -bk);
    return lerp(backward, forward, fwdWeight);
}

// ===========================================================================
// Cheap in-scatter / lighting approximation
// ===========================================================================

// Evaluate light transmittance toward the sun at a sample point.
// Uses very few steps along the light direction.
float LightTransmittance(float3 pos, float heightFrac)
{
    if (ShadowStepCount <= 0)
        return 1.0f;

    // March a short distance toward the light.
    float lightMarchDist = CloudThickness * 0.5f;
    float stepSize = lightMarchDist / (float)ShadowStepCount;
    float3 lightStep = normalize(CloudLightDirection) * stepSize;

    float accumDensity = 0.0f;
    float3 lightPos = pos;

    [loop]
    for (int shadowStep = 0; shadowStep < ShadowStepCount; shadowStep++)
    {
        lightPos += lightStep;
        float lh = HeightFraction(lightPos.y,
                   CamPositionWS.y - CloudBottomHeight, CloudThickness);

        if (lh < 0.0f || lh > 1.0f)
            break;

        // Coarse density only (no detail) for shadow samples.
        float d = CloudDensityAtWorldPos(lightPos, lh, false, 0.0f);
        accumDensity += d * stepSize;
    }

    // Beer-Lambert.
    float opticalDepth  = accumDensity * Absorption;
    float transmittance = exp(-opticalDepth);

    // Powder / silver lining approximation:
    // Bright edge when looking toward light through thin cloud.
    //
    // The powder term darkens thin regions: 1 - exp(-d*A*2) is near-zero when
    // accumDensity is tiny.  multiplying transmittance (???1) by a near-zero
    // powder makes the boundary near-black.  As noise makes density wobble
    // frame-to-frame, that black flickers bright??"dark.
    //
    // Edge-color preservation for low Absorption:
    // At low absorption, thin regions should become more translucent but should
    // not collapse to dark gray outlines. Keep a higher powder floor for low Abs,
    // then lift it further in very thin optical-depth regions.
    // This preserves bright cloud color in fading boundaries while still allowing
    // deep interiors to shade normally.
    float absNorm	   = saturate((Absorption - 0.5f) * 0.4f);
    float baseFloor	 = lerp(0.55f, 0.68f, absNorm);
    float thinEdge	  = 1.0f - saturate(opticalDepth / 0.18f);
    float edgeFloorLift = thinEdge * 0.25f;
    float powderFloor   = min(baseFloor + edgeFloorLift, 0.95f);

    float powder = 1.0f - exp(-opticalDepth * 2.0f);
    powder = lerp(1.0f, max(powder, powderFloor), SilverliningStr);

    return transmittance * powder;
}

// ===========================================================================
// Simple blue-noise-like jitter from screen position + frame
// ===========================================================================

float ScreenJitter(float2 screenPos)
{
    // Interleaved gradient noise (Jimenez 2014)   static per-pixel.
    // No per-frame shift: without TAA, frame-varying jitter causes shimmer.
    // Static IGN gives spatial decorrelation without temporal instability.
    float3 magic = float3(0.06711056f, 0.00583715f, 52.9829189f);
    float rawJitter = frac(magic.z * frac(dot(screenPos, magic.xy)));
    float thickBandGuard = saturate((CloudThickness - 2600.0f) / 2200.0f);
    float jitterMin	  = lerp(0.35f, 0.85f, thickBandGuard);
    float jitterAbsDamp  = lerp(jitterMin, 1.0f, saturate((Absorption - 0.2f) * 2.5f));
    // Uniform [0,1] distribution   no bell-shaping.
    // The smoothstep bell was compressing most pixels toward jitter ??? 0.5,
    // which re-aligned the first-step plane across adjacent pixels and made
    // height-gradient transition bands coherent (visible horizontal slabs).
    // With uniform distribution each pixel starts at a truly decorrelated
    // offset; the 3x3 bilateral composite filters any resulting edge noise.
    // At very low absorption, full jitter manifests as visible pepper noise
    // because cloud extinction is weak and many stochastic edge samples remain.
    // Damping jitter there keeps silhouettes smooth without reintroducing bands.
    return rawJitter * JitterStrength * jitterAbsDamp;
}

// ===========================================================================
// Helper: minimum distance from view ray (ro, rd) to a 3D line segment (A, B).
// Used by the lightning bolt arc renderer.
//
// Based on the standard two-line closest-point formulation.
// Returns a very large value for degenerate cases (segment too short, or
// ray nearly parallel to segment pointing toward camera).
// ===========================================================================
float RayToSegmentMinDist(float3 ro, float3 rd, float3 A, float3 B)
{
    float3 AB = B - A;
    float3 AO = ro - A;

    float ABlenSq = dot(AB, AB);
    if (ABlenSq < 0.01f)
    {
        // Degenerate: treat as point at A
        float tRay = max(dot(A - ro, rd), 0.0f);
        return length(ro + rd * tRay - A);
    }

    // a = dot(rd, rd) = 1 (rd is normalised)
    float b = dot(rd, AB);
    float c = ABlenSq;
    float d = dot(rd, AO);
    float e = dot(AB, AO);

    float denom = c - b * b; // = |AB|^2 - (rd.AB)^2  >=0

    float t, s;
    if (denom < 0.01f)
    {
        // Nearly parallel � bolt pointing straight at camera; no visible arc
        return 1e6f;
    }
    else
    {
        t = (b * e - c * d) / denom;
        s = (e - b * d) / denom;
    }

    t = max(t, 0.0f);        // ray only goes forward
    s = clamp(s, 0.0f, 1.0f); // clamp to segment endpoints

    float3 P = ro + rd * t;
    float3 Q = A + AB * s;
    return length(P - Q);
}

// Correct 3D distance from view ray (ro, rd normalised) to a bolt segment [A,B].
//
// For skew lines the standard closest-approach formula is used.
// For near-parallel lines (vertical bolt + upward view ray) the formula degenerates;
// instead we return  length(cross(rd, ro-A))  which is the perpendicular distance
// from the infinite ray LINE to the infinite segment LINE � constant along the whole
// segment, so the bolt glows uniformly even when viewed from directly below.
float BoltSegDist(float3 ro, float3 rd, float3 A, float3 B)
{
    float3 AB  = B - A;
    float3 rA  = ro - A;          // vector from A to ray origin
    float  ab2 = dot(AB, AB);

    if (ab2 < 0.01f)
        return length(cross(rd, rA)); // degenerate segment � point A

    float  rdab  = dot(rd, AB);
    float  denom = ab2 - rdab * rdab; // ab2 * sin�(angle between rd and AB)

    if (denom < 0.0001f * ab2)
    {
        // Nearly parallel: perpendicular distance from ray line to segment line.
        // cross(rd, rA) magnitude = |rA| * sin(angle between rd and rA) = distance
        // from point A to the infinite ray line �  constant for all points along AB.
        return length(cross(rd, rA));
    }

    // Unconstrained closest-approach parameter on segment, then clamp.
    float  s  = clamp((dot(AB, rA) - rdab * dot(rd, rA)) / denom, 0.0f, 1.0f);
    float3 Qs = A + AB * s;

    // Closest point on ray to Qs.
    float  t  = max(dot(rd, Qs - ro), 0.0f);
    return length(ro + rd * t - Qs);
}

// ---------------------------------------------------------------------------
// 1-D smooth value noise + 4-octave FBM for continuous bolt path deflection.
// Matches the GLSL reference shader's fbm-deflected cylinder approach:
//   bolt centre at height s = (boltX + FBM(s), Y, boltZ + FBM(s))
// giving smooth curvature with no endpoint "bead" artefacts.
// ---------------------------------------------------------------------------

float SmoothNoise1D(float x)
{
    float i = floor(x);
    float f = frac(x);
    float u = f * f * (3.0f - 2.0f * f);   // Hermite smoothstep
    float h0 = frac(sin(i          * 127.1f) * 43758.5453f) * 2.0f - 1.0f;
    float h1 = frac(sin((i + 1.0f) * 127.1f) * 43758.5453f) * 2.0f - 1.0f;
    return lerp(h0, h1, u);
}

float BoltFBM(float p)
{
    float v = 0.0f, a = 1.0f, fq = 1.0f;
    [unroll]
    for (int k = 0; k < 4; k++)
    {
        v  += SmoothNoise1D(p * fq + (float)k * 31.71f) * a;
        a  *= 0.5f;
        fq *= 2.0f;
    }
    return v;   // approximately in [-1, 1]
}

// ===========================================================================
// Main cloud raymarch
// ===========================================================================

float4 RaymarchClouds(float3 rayOrigin, float3 rayDir, float2 screenPos)
{
    // AltocumulusMid uses its own thickness and absorption   fully self-contained.
    float effThickness  = (CloudType == 1) ? AltoThickness  : CloudThickness;
    float effAbsorption = (CloudType == 1) ? AltoAbsorption : Absorption;

    // Intersect cloud volume using the effective thickness.
    // Bleed pass: extend the Alto cloud slab DOWNWARD (toward camera) so clouds appear
    // to pour below their natural base height and flow over mountain tops.
    // AltoBleedDepth [0,100] controls how far below the base they extend:
    // value * 0.01 * CloudBottomHeight = world-unit extension (100 = full CloudBottomHeight).
    // Wind streaming drifts deep samples in the wind direction so
    // the clouds appear to flow from their source direction.
    float bleedExtent = 0.0f;
    if (CloudIsBleedPass > 0.001f && CloudType == 1)
        bleedExtent = (AltoBleedDepth * 0.01f) * CloudBottomHeight; // purely driven by AltoBleedDepth, not CloudIsBleedPass

    // Intersect cloud volume. For the bleed pass the slab bottom is extended downward.
    float2 tRange;
    if (bleedExtent > 0.001f && abs(rayDir.y) > 0.0001f)
    {
        // Extended slab: bottom shifted toward camera (higher Y in TEN Y-down).
        float slabBtm = CamPositionWS.y - CloudBottomHeight + bleedExtent;
        float slabTop = CamPositionWS.y - CloudBottomHeight - effThickness;
        float t0 = (slabBtm - rayOrigin.y) / rayDir.y;
        float t1 = (slabTop - rayOrigin.y) / rayDir.y;
        float tNear = min(t0, t1);
        float tFar  = max(t0, t1);
        tRange = (tFar < 0.0f || tNear >= tFar)
               ? float2(-1.0f, -1.0f)
               : float2(max(tNear, 0.0f), tFar);
    }
    else
    {
        tRange = IntersectCloudVolumeEx(rayOrigin, rayDir, effThickness);
    }

    if (tRange.x < 0.0f)
        return float4(0.0f, 0.0f, 0.0f, 0.0f); // No intersection   fully transparent.

    // Clamp max march distance to avoid wasting steps on very long grazing rays.
    float maxDist = min(tRange.y - tRange.x, (effThickness + bleedExtent) * 6.0f);

    // Adaptive step count: for tall cloud volumes the nominal step size
    // (maxDist / PrimaryStepCount) can become large enough that each step
    // produces a visible extinction slab   especially at height-gradient
    // transition zones (bottom/top fade) where density changes sharply.
    // Guarantee stepSize ??? effThickness/24 so that transition zones in tall
    // clouds are sampled densely enough to avoid visible layered march slabs.
    float minStepsF	  = ceil(maxDist / max(effThickness / 24.0f, 1.0f));
    int   effectiveSteps = clamp((int)max((float)PrimaryStepCount, minStepsF), 1, 64);
    // Low-absorption boost: when absorption is very small, each sample carries
    // little extinction and stochastic sampling noise is more visible.
    // Increase effective steps in that regime to average out point noise.
    float lowAbsStepBoost = lerp(2.0f, 1.0f, saturate((effAbsorption - 0.2f) * 2.5f));
    effectiveSteps		= clamp((int)ceil((float)effectiveSteps * lowAbsStepBoost), 1, 64);
    float stepSize	   = maxDist / (float)effectiveSteps;
    float thickBandGuard = saturate((effThickness - 2600.0f) / 2200.0f);

    // Per-pixel start jitter: uniform [0,1] (see ScreenJitter comments).
    float jitter = ScreenJitter(screenPos);
    float t = tRange.x + stepSize * jitter;

    // Secondary per-pixel hash for per-step sub-jitter decorrelation.
    // Different seed from ScreenJitter to avoid start/sub correlation.
    float3 magic2	 = float3(0.19881f, 0.04679f, 41.731f);
    float  rawJitter2 = frac(magic2.z * frac(dot(screenPos * 0.81f + float2(1.3f, 2.7f), magic2.xy)));

    // Phase function for light scattering.
    float cosTheta = dot(rayDir, normalize(CloudLightDirection));
    float phase = DualLobePhase(cosTheta);

    // Flat-disk sky metric for AltocumulusMid distribution bias (constant per ray).
    // 0.0 at the far visible edge of the slab, 1.0 directly overhead (slab center).
    //
    // The cloud slab is a FLAT HORIZONTAL PLANE, not a hemisphere. The correct
    // metric is the horizontal distance from the camera's overhead point to where
    // the ray hits the slab, NOT the elevation angle alone.
    //
    // For a normalized ray in TEN Y-down space (abs(rayDir.y) = sin(elevation)):
    //   d_horiz = CloudBottomHeight * cos(elev) / sin(elev)
    // Normalized by the far-edge distance (6 * CloudBottomHeight = lodFar):
    //   flatSkyH = 1 - saturate(cos(elev) / (sin(elev) * 6))
    //            = 1 - saturate(length(rayDir.xz) / (abs(rayDir.y) * 6))
    //
    //   flatSkyH = 1  ->  directly overhead     (inner circle of the flat disk)
    //   flatSkyH = 0  ->  far visible edge (~9deg elevation, where horizon fade cuts in)
    //
    // This creates flat circular isolines in the slab, matching the geometry of the
    // actual flat cloud layer (right diagram), NOT a dome (wrong diagram).
    float altoSkyH;
    if (CloudType == 1)
    {
        float sinElev  = max(abs(rayDir.y), 0.001f);
        float cosElev  = length(rayDir.xz);
        float flatSkyH = 1.0f - saturate(cosElev / (sinElev * 6.0f));
        altoSkyH = pow(flatSkyH, max(AltoHeightBlendPower, 0.001f));
    }
    else
    {
        altoSkyH = 0.0f;
    }

    // Bleed-zone lower bound: how far below 0 (in heightFrac units) the extended
    // slab reaches. Samples with heightFrac in [-bleedFracLimit, 0) are in the
    // pour-down zone and get wind-streamed XZ offsets with a depth-based fade.
    float bleedFracLimit = (bleedExtent > 0.001f && effThickness > 0.001f)
                         ? (bleedExtent / effThickness) : 0.0f;

    // Accumulation.
    float  transmittance = 1.0f;
    float3 scatteredLight = float3(0.0f, 0.0f, 0.0f);
    int	steps = 0;

    [loop]
    for (int step = 0; step < effectiveSteps; step++)
    {
        if (transmittance < 0.01f)
            break; // Early out   cloud is opaque.

        // Per-step golden-ratio sub-jitter: offsets each sample within its
        // step interval so the march planes are not perfectly periodic.
        // rawJitter2 provides per-pixel phase; step * 0.618 (golden ratio) gives
        // maximally equidistant offsets across all steps, guaranteeing that
        // adjacent steps never accidentally re-align into coherent horizontal slabs.
        // Amplitude ??0.35 * stepSize keeps samples within each step's sub-domain.
        float subJitterMin = lerp(0.22f, 0.55f, thickBandGuard);
        float subJitterAmp = lerp(subJitterMin, 0.7f, saturate((effAbsorption - 0.2f) * 2.5f));
        float subOff	 = (frac(rawJitter2 + (float)step * 0.61803398875f) - 0.5f) * stepSize * subJitterAmp;
        float3 samplePos = rayOrigin + rayDir * (t + subOff);
        float  heightFrac = HeightFraction(samplePos.y,
                            rayOrigin.y - CloudBottomHeight, effThickness);

        // Skip samples outside the cloud layer boundary.
        // In the bleed pass the lower bound is extended by bleedFracLimit.
        if (heightFrac >= -bleedFracLimit && heightFrac <= 1.0f)
        {
            // Bleed zone: samples below the original slab base (heightFrac < 0).
            // Project onto the slab base and stream the XZ position in the wind
            // direction — the deeper the sample, the more it has drifted.
            // This gives the appearance of cloud mass pouring off the mountain
            // top and flowing downward in the prevailing wind direction.
            float3 effectiveSamplePos = samplePos;
            float  effectiveHF        = max(heightFrac, 0.0f);
            float  bleedDensityFade   = 1.0f;
            if (heightFrac < 0.0f && bleedFracLimit > 0.001f)
            {
                // depthT: 0=at slab base, 1=at maximum bleed depth.
                float depthT = -heightFrac / bleedFracLimit;
                // Stream: 40% of CloudBottomHeight lateral drift at full depth.
                float2 wDir = (dot(WindDirection, WindDirection) > 0.001f)
                            ? normalize(WindDirection) : float2(1.0f, 0.0f);
                effectiveSamplePos.xz += wDir * (depthT * CloudBottomHeight * 0.4f);
                // Project onto slab base for the density lookup.
                effectiveHF = 0.0f;
                // Quadratic fade: fully opaque at base, transparent at max depth.
                bleedDensityFade = (1.0f - depthT) * (1.0f - depthT);
            }

            // Sample density (use detail noise if available).
            bool useDetail = (DetailNoiseEnabled != 0);
            float density = CloudDensityAtWorldPos(effectiveSamplePos, effectiveHF, useDetail, altoSkyH);
            density *= bleedDensityFade;

            if (density > 0.0001f)
            {
                // Extinction for this step.
                float extinction = density * effAbsorption * stepSize;

                // Total in-scattered light at this sample.
                float3 sampleLight;
                if (CloudType == 1)
                {
                    // === AltocumulusMid: fully self-contained lighting ===
                    // The dark-base shading (lerp toward AltoCloudColorDark at low
                    // heightFrac) models self-shadowing by the cloud mass above —
                    // physically correct only when there IS a thick cloud roof.
                    //
                    // For thin wisps and cloud edges that same code forces the color
                    // dark even though the optical depth is too small to shadow
                    // anything — producing the visible dark outlines.
                    //
                    // Fix: thin wisps and cloud edges have low accumulated optical
                    // depth and should appear bright (they can't self-shadow).
                    //
                    // Use extinction (density * absorption * stepSize) as the
                    // per-sample optical thickness. A sample is "thin" when its
                    // own extinction is small — regardless of what other samples
                    // in the same ray have already contributed.
                    //   extinction → 0   : optically transparent sample (edge/wisp)
                    //                      → force toward bright cloud color, no dark base
                    //   extinction → large: optically thick sample (cloud interior)
                    //                      → preserve natural height gradient
                    //
                    // This is strictly per-sample: it doesn't flatten the overall
                    // dark/bright gradient of thick clouds (which accumulate large
                    // extinction in their interior samples) and doesn't cause a
                    // warm tint (the warm lerp only applies after color is finalized).
                    float sampleOptDepth = density * effAbsorption * stepSize;
                    float thinEdge = 1.0f - saturate(sampleOptDepth * 12.0f);
                    float heightBlend = saturate(heightFrac + thinEdge * 1.0f);
                    float3 cloudColor = lerp(AltoCloudColorDark, AltoCloudColor,
                        heightBlend);
                    float  heightIllum = exp(heightFrac) / 1.95f;
                    heightIllum = lerp(heightIllum, 1.0f, thinEdge * 0.5f);

                    // Sun-elevation modulation for Altocumulus:
                    float altoSunFade = saturate(CloudSunElevation * 6.0f + 0.5f);
                    altoSunFade = altoSunFade * altoSunFade;
                    float altoTwilightBoost = saturate(1.0f - abs(CloudSunElevation) * 8.0f) * CloudTwilightAmbient;
                    float altoNightBase = lerp(CloudNightAmbient, 0.0f, saturate(CloudSunElevation * 4.0f));
                    float altoDirectFactor = AltoCloudBrightness * heightIllum * altoSunFade * CloudSunLightIntensity;
                    float altoAmbientFactor = (altoTwilightBoost + altoNightBase) * max(CloudAmbientIntensity, 0.001f);

                    // Silverlining: top-edge brightening toward the sun.
                    float altoSilver  = CloudSilverliningStrength * 0.25f * heightFrac * altoSunFade * CloudSunLightIntensity;
                    // Forward scatter: broad sun-aligned hazy glow boost.
                    float altoForward = CloudForwardScatterStrength * 0.12f * altoSunFade * CloudSunLightIntensity;
                    // Sun warmth: tint cloud illumination toward golden tone at low sun angles.
                    float3 altoWarmTint = float3(1.0f, 0.88f, 0.65f);
                    float3 altoLitColor = lerp(CloudLightColor, CloudLightColor * altoWarmTint, CloudSunWarmthInfluence * altoSunFade);

                    sampleLight = altoLitColor * cloudColor
                        * (altoDirectFactor + altoAmbientFactor + altoSilver + altoForward);

                    // === Lightning / internal flash illumination ===
                    // Positions are computed in world-space, centered on the camera (rayOrigin.xz)
                    // so that all distance calculations (samplePos - source) remain correct
                    // regardless of the player's world-space XZ position.
                    // Each flash/bolt is spread across a sky-scale horizontal extent derived from
                    // CloudBottomHeight (how far away the cloud layer is horizontally).
                    // Coverage biasing suppresses lightning in clear-sky regions.
                    if (LightningEnabled != 0)
                    {
                        float3 lightningContrib = float3(0.0f, 0.0f, 0.0f);

                        // Sky-scale horizontal extent of the Alto cloud layer.
                        // effThickness (~1800) is the VERTICAL slab depth and far too small for XZ.
                        // CloudBottomHeight = camera-to-cloud-base distance, a good proxy for the
                        // radius of the visible cloud field at that altitude.
                        float altoExtentXZ = max(CloudBottomHeight * 0.8f, 25000.0f);
                        float slabCenterY = rayOrigin.y - CloudBottomHeight + effThickness * 0.5f;

                        // --- Internal cloud flash ---
                        // Source position varies every cycle, spread over the full cloud field.
                        float flashCycle = floor(CloudTime * LightningInternalSpeed);
                        float flashRand = frac(sin(flashCycle * 127.1f + 311.7f) * 43758.5453f);
                        if (flashRand < LightningInternalFreq)
                        {
                            float3 flashRand3 = float3(
                                frac(sin(flashCycle * 73.1f + 1.3f) * 43758.5453f),
                                frac(sin(flashCycle * 91.7f + 7.9f) * 43758.5453f),
                                frac(sin(flashCycle * 53.3f + 3.7f) * 43758.5453f)
                                );
                            // Sky-space XZ offset (camera-relative, not world-origin relative).
                            // Adding rayOrigin.xz converts to world-space so that
                            // (samplePos - flashSource) cancels the camera offset correctly.
                            float2 flashSkyXZ = float2(
                                (flashRand3.x - 0.5f) * altoExtentXZ,
                                (flashRand3.z - 0.5f) * altoExtentXZ
                                );
                            float3 flashSource = float3(
                                rayOrigin.x + flashSkyXZ.x,
                                slabCenterY + (flashRand3.y - 0.5f) * effThickness * 0.3f,
                                rayOrigin.z + flashSkyXZ.y
                                );
                            // Sample weather coverage at flash sky-position to suppress clear-sky flashes.
                            float2 flashWeatherUV = flashSkyXZ * WeatherScale + WindDirection * WindSpeed * 0.3f;
                            float  flashCoverage = saturate(Remap(ValueNoise3D(float3(flashWeatherUV, 0.0f)),
                                1.0f - Coverage, 1.0f, 0.0f, 1.0f));
                            // GLSL flicker: size = sin(45*frac(t)) + 5 ??? radius [4,6] * 1% effThickness.
                            float flickerSize = sin(45.0f * frac(CloudTime)) + 5.0f;
                            float flashRadius = effThickness * (flickerSize * 0.01f);
                            float flashDist = length(samplePos - flashSource);
                            float flashGlow = pow(saturate(flashRadius / max(flashDist, 0.001f)), 3.2f);
                            lightningContrib += LightningBoltColor * flashGlow
                                * LightningFlashIntensity * flashCoverage;
                        }

                        // --- Bolt-exit glow (3 bolts, staggered by 1/3 cycle) ---
                        // Each bolt lands at its own independently randomised sky-scale position,
                        // distributed across the full cloud field, biased toward covered regions.
                        float boltTime = CloudTime * LightningSpeed;
                        // In TEN, Y increases downward.  Cloud base (nearest ground) = slabBottom.
                        // boltBaseY is placed just below the slab base (bolt exits cloud upward).
                        float boltBaseY = rayOrigin.y - CloudBottomHeight + effThickness * 0.1f;
                        float boltRadius = effThickness * 0.08f;
                        [unroll]
                        for (int bi = 0; bi < 3; bi++)
                        {
                            float boltOffset = (float)bi * 0.33333f;
                            float boltCycle = floor(boltTime + boltOffset);
                            float boltRand = frac(sin(boltCycle * (311.7f + (float)bi * 83.5f) + 127.1f) * 43758.5453f);
                            if (boltRand < LightningStrikeFreq)
                            {
                                // Independent sky-scale XZ per bolt, centered on camera.
                                float boltRandX = frac(sin(boltCycle * (43.1f + (float)bi * 61.3f) + 91.3f) * 43758.5453f);
                                float boltRandZ = frac(sin(boltCycle * (71.9f + (float)bi * 47.9f) + 23.7f) * 43758.5453f);
                                float2 boltSkyXZ = float2(
                                    (boltRandX - 0.5f) * altoExtentXZ,
                                    (boltRandZ - 0.5f) * altoExtentXZ
                                    );
                                float3 boltPos = float3(
                                    rayOrigin.x + boltSkyXZ.x,
                                    boltBaseY,
                                    rayOrigin.z + boltSkyXZ.y
                                    );
                                // Suppress bolt in clear-sky regions.
                                float2 boltWeatherUV = boltSkyXZ * WeatherScale + WindDirection * WindSpeed * 0.3f;
                                float  boltCoverage = saturate(Remap(ValueNoise3D(float3(boltWeatherUV, 0.0f)),
                                    1.0f - Coverage, 1.0f, 0.0f, 1.0f));
                                float boltDist = length(samplePos - boltPos);
                                float boltGlow = pow(saturate(boltRadius / max(boltDist, 0.001f)), 2.2f);
                                float boltFrac = frac(boltTime + boltOffset);
                                float boltPulse = exp(-boltFrac * 6.0f);
                                lightningContrib += LightningBoltColor * boltGlow * boltPulse
                                    * LightningGlowIntensity * boltCoverage;
                            }
                        }

                        sampleLight += lightningContrib * LightningAmbientContrib;
                        sampleLight += lightningContrib * (1.0f - LightningAmbientContrib);
                    }
                }
                else
                {
                    // Standard lighting for non-Alto cloud types.
                    float lightT = LightTransmittance(samplePos, heightFrac);
                    float ambient = AmbientContrib * lerp(0.6f, 1.0f, heightFrac);
                    float thinExtinction = 1.0f - saturate(extinction / 0.12f);
                    float lowAbsFactor   = 1.0f - saturate(Absorption * 0.35f);
                    float edgeColorHold  = thinExtinction * lowAbsFactor * 0.45f;

                    // Sun-elevation-based lighting modulation:
                    // When the sun is below the horizon, direct light fades out
                    // and only ambient/twilight/night contribution remains.
                    float sunFade = saturate(CloudSunElevation * 6.0f + 0.5f); // 1.0 at day, fades to 0 below horizon.
                    sunFade = sunFade * sunFade; // Smooth falloff.
                    float twilightBoost = saturate(1.0f - abs(CloudSunElevation) * 8.0f) * CloudTwilightAmbient;
                    float nightAmbientBase = lerp(CloudNightAmbient, 0.0f, saturate(CloudSunElevation * 4.0f));
                    float effectiveAmbient = ambient * (1.0f + edgeColorHold) + twilightBoost + nightAmbientBase;

                    // Beer-Lambert absorption exponent on shadow transmittance.
                    float absLightT = pow(max(lightT, 0.001f), CloudLightAbsorption);
                    // Direct sun: HG phase boosted by forward-scatter strength.
                    float directSun = absLightT * phase * CloudForwardScatterStrength * sunFade * CloudSunLightIntensity;
                    // Silverlining: additive phase-squared forward glow (edge brightening toward sun).
                    float silverGlow = phase * phase * sunFade * CloudSilverliningStrength * 0.3f * CloudSunLightIntensity;
                    // Ambient scaled by CloudAmbientIntensity.
                    float ambLight = effectiveAmbient * max(CloudAmbientIntensity, 0.001f);
                    // Sun warmth: tint light toward warm golden tone.
                    float3 stdWarmTint = float3(1.0f, 0.88f, 0.65f);
                    float3 stdLitColor = lerp(CloudLightColor, CloudLightColor * stdWarmTint, CloudSunWarmthInfluence * sunFade);
                    sampleLight = stdLitColor * (directSun + silverGlow + ambLight);
                }

                float sampleTransmittance = exp(-extinction);

                // Energy-conserving integration (Frostbite technique).
                float3 integScatter = sampleLight * (1.0f - sampleTransmittance) / max(density * effAbsorption, 0.0001f);
                scatteredLight += transmittance * integScatter * density * effAbsorption;
                transmittance *= sampleTransmittance;
            }
        }

        t += stepSize;
        steps++;
    }

    float opacity = 1.0f - transmittance;
    return float4(scatteredLight, opacity);
}

// ===========================================================================
// Debug visualization helpers
// ===========================================================================

float4 DebugVisualization(float3 rayOrigin, float3 rayDir, float2 screenPos, float4 cloudResult, int stepsTaken)
{
    if (CloudDebugView == 1) // CoverageMask
    {
        float3 samplePos = rayOrigin + rayDir * (CloudBottomHeight + CloudThickness * 0.5f);
        float3 dbgSkyPos = samplePos - CamPositionWS.xyz;
        float2 wUV = dbgSkyPos.xz * WeatherScale + WindDirection * WindSpeed * 0.3f;
        float wn = ValueNoise3D(float3(wUV, 0.0f));
        float cm = Remap(wn, 1.0f - Coverage, 1.0f, 0.0f, 1.0f);
        return float4(cm.xxx, 1.0f);
    }
    if (CloudDebugView == 2) // DensityField
    {
        return float4(cloudResult.a, cloudResult.a, cloudResult.a, 1.0f);
    }
    if (CloudDebugView == 3) // StepCountHeatmap
    {
        float ratio = (float)stepsTaken / (float)PrimaryStepCount;
        float3 heat = float3(ratio, 1.0f - ratio, 0.0f);
        return float4(heat, 1.0f);
    }
    if (CloudDebugView == 4) // TransmittanceOnly
    {
        float t = 1.0f - cloudResult.a;
        return float4(t, t, t, 1.0f);
    }
    if (CloudDebugView == 6) // NoDetailNoise   re-march without detail
    {
        // For debug, we just show the shape noise contribution via alpha channel trick.
        return float4(cloudResult.rgb, cloudResult.a);
    }

    return cloudResult;
}

// ===========================================================================
// Atmospheric horizon fade
// ===========================================================================

// Returns a [0, 1] opacity multiplier that softly attenuates cloud opacity for
// rays that graze the horizon or see very distant cloud regions.
//
// In TEN's Y-down space: sky is -Y, horizon is Y ??? 0.
//   elevation = -rayDir.y   (0 at horizon, 1 straight up)
//
// Distance and elevation are geometrically equivalent for a flat cloud slab:
//   tEntry ??? CloudBottomHeight / elevation
// So a single elevation-based curve captures both the distance-fade and the
// horizon-haze fade simultaneously.
//
// Curve design:
//   elevation 0.00  (0?deg)   ??? 0%   (totally transparent   merges with horizon)
//   elevation 0.06  (~3?deg)  ??? 20%  (strongly faded   very distant fringe)
//   elevation 0.12  (~7?deg)  ??? 50%  (half strength   hazy transition region)
//   elevation 0.22  (~13?deg) ??? 85%  (nearly full   close-ish cloud masses)
//   elevation 0.30  (~17?deg) ??? 100% (fully opaque   nearby/overhead clouds)
//
// The sqrt push on the smoothstep result gives an exponential-feeling rolloff:
// opacity recovers quickly once a cloud is even a few degrees above the horizon,
// while the near-horizon region stays very faint and "airy".
float HorizonAtmosphericFade(float3 rayDir)
{
    // Elevation in [0,1]: 0 = horizontal, 1 = overhead.
    float elevation = saturate(-rayDir.y);

    // Soft fade band from 0?deg to ~17?deg above the horizon.
    float fade = smoothstep(0.0f, 0.10f, elevation);

    // sqrt push: makes the lower half of the transition feel gentle and
    // atmospheric (exponential character) rather than linear.
    float baseFade = sqrt(fade);

    // HorizonFade CB parameter: 0 = disable horizon fade, 1 = full fade.
    // Lerp between no fade (1.0) and the computed fade to allow per-layer control.
    return lerp(1.0f, baseFade, HorizonFade);
}

// ===========================================================================
// Reconstruct view-space ray direction from UV
// ===========================================================================

float3 GetViewRayDir(float2 uv)
{
    float2 ndc = uv * 2.0f - 1.0f;
    ndc.y = -ndc.y;

    // Reconstruct view-space direction using inverse projection.
    float4 clipPos = float4(ndc, 1.0f, 1.0f);
    float4 viewPos = mul(clipPos, InverseProjection);
    viewPos.xyz /= viewPos.w;

    // Transform to world space.
    float3 worldDir = mul(float4(viewPos.xyz, 0.0f), InverseView).xyz;
    return normalize(worldDir);
}

// ===========================================================================
// PS   Main cloud rendering pixel shader
// ===========================================================================

float4 PS(VSOutput input) : SV_TARGET
{
    float3 rayOrigin = CamPositionWS.xyz;
    float3 rayDir	= GetViewRayDir(input.UV);

    // In TEN Y-down: upward rays have rayDir.y < 0.
    // Discard downward-looking rays (into the ground).
    if (rayDir.y > 0.02f)
        return float4(0.0f, 0.0f, 0.0f, 0.0f);

    float4 cloudResult = RaymarchClouds(rayOrigin, rayDir, input.Position.xy);

    // --- Lightning bolt arc rendering (AltocumulusMid only) ---
    // Fully tied to the cloud flash: uses identical flashCycle + hash as the raymarcher.
    //
    // KNOT FIX: glow is applied ONCE using the minimum distance to the whole polyline,
    // not summed per-segment. This prevents the 2� brightness at shared endpoints that
    // caused bright knots at every segment junction.
    //
    // Hash quality: cycleN = frac(flashCycle * f) * 360 keeps sin() arguments in [0,~600].
    if (CloudType == 1 && LightningEnabled != 0)
    {
        float flashCycle = floor(CloudTime * LightningInternalSpeed);
        float flashFrac  = frac(CloudTime * LightningInternalSpeed);
        float flashRand  = frac(sin(flashCycle * 127.1f + 311.7f) * 43758.5453f);

        if (flashRand < LightningInternalFreq)
        {
            float boltGateRand = frac(sin(flashCycle * 179.3f + 43.7f) * 43758.5453f);
            if (boltGateRand < LightningStrikeFreq)
            {
                float altoExtentXZ = max(CloudBottomHeight * 0.8f, 25000.0f);

                // Flash XZ � bitwise-identical hashes to the raymarcher flash position.
                float flashR3x = frac(sin(flashCycle * 73.1f + 1.3f) * 43758.5453f);
                float flashR3z = frac(sin(flashCycle * 53.3f + 3.7f) * 43758.5453f);
                float boltX    = rayOrigin.x + (flashR3x - 0.5f) * altoExtentXZ;
                float boltZ    = rayOrigin.z + (flashR3z - 0.5f) * altoExtentXZ;

                float cloudBaseY = rayOrigin.y - CloudBottomHeight;
                float boltLength = CloudBottomHeight * 0.6f * LightningBoltLengthScale;

                // Radii clamped so bolt stays thin at all cloud heights.
                float outerR     = clamp(CloudBottomHeight * 0.018f * LightningBoltThicknessScale, 40.0f, 180.0f * LightningBoltThicknessScale);
                float midR       = outerR * 0.30f;
                float innerAuraR = outerR * 0.50f;
                float innerCoreR = outerR * 0.20f;

                float boltPulse = exp(-flashFrac * 5.0f);
                float cycleN    = frac(flashCycle * 0.6180339887f) * 360.0f;

                // ----------------------------------------------------------------
                // Build main bolt polyline.
                // Per-cycle: 40% ? complex (9 segs + 2 forks), 60% ? simple (6 segs + 1 fork).
                // BoltSegDist is used for all glow distances � it projects onto the
                // view plane, giving uniform brightness along segments regardless of angle.
                // ----------------------------------------------------------------
                bool  isComplex = (frac(sin(cycleN + 91.3f) * 43758.5453f) < 0.4f);

                // Per-cycle FBM seed offsets (golden-ratio and silver-ratio scramble).
                float seedX = frac(sin(cycleN * 0.6180339887f + 13.7f) * 43758.5453f) * 47.3f;
                float seedZ = frac(sin(cycleN * 0.7548776662f + 27.1f) * 43758.5453f) * 47.3f;


                // FBM bolt path: build array of curve points, then use BoltSegDist on
                // consecutive segments. Segments fill gaps perfectly (no bead artefact)
                // while the FBM curvature prevents the "all parallel" case that used to
                // plague straight bolts.
                const int BOLT_SAMPLES = 24;
                float3 boltPath[24];
                [loop]
                for (int si = 0; si < BOLT_SAMPLES; si++)
                {
                    float  s   = (float)si / (float)(BOLT_SAMPLES - 1);
                    float  amp = outerR * 1.5f * (1.0f - s * 0.4f);
                    boltPath[si] = float3(
                        boltX + BoltFBM(s * 3.0f + seedX) * amp,
                        cloudBaseY + s * boltLength,
                        boltZ + BoltFBM(s * 3.0f + seedZ) * amp);
                }

                float minDistMain = 1e6f;
                [loop]
                for (int si = 0; si < BOLT_SAMPLES - 1; si++)
                    minDistMain = min(minDistMain, BoltSegDist(rayOrigin, rayDir, boltPath[si], boltPath[si + 1]));

                // Fork origins: FBM evaluated at fixed height fractions on the main bolt.
                float  fo1s   = 0.33f;
                float  fo1amp = outerR * 1.5f * (1.0f - fo1s * 0.4f);
                float3 fork1Origin = float3(
                    boltX + BoltFBM(fo1s * 3.0f + seedX) * fo1amp,
                    cloudBaseY + fo1s * boltLength,
                    boltZ + BoltFBM(fo1s * 3.0f + seedZ) * fo1amp);

                // ----------------------------------------------------------------
                // Fork 1 - always present, branches from fork1Origin (~33% up the main bolt).
                // FBM path array + BoltSegDist segments - same approach as main bolt.
                // ----------------------------------------------------------------
                const int FORK_SAMPLES = 16;
                float fDX   = (frac(sin(cycleN + 53.1f) * 43758.5453f) - 0.5f) * boltLength * 0.55f;
                float fDZ   = (frac(sin(cycleN + 61.7f) * 43758.5453f) - 0.5f) * boltLength * 0.55f;
                float fLenY = boltLength * 0.45f;
                float fSeedX = frac(sin(cycleN * 0.5f + 19.1f) * 43758.5453f) * 47.3f;
                float fSeedZ = frac(sin(cycleN * 0.5f + 31.7f) * 43758.5453f) * 47.3f;

                float3 forkPath1[16];
                [loop]
                for (int fi = 0; fi < FORK_SAMPLES; fi++)
                {
                    float  ff   = (float)fi / (float)(FORK_SAMPLES - 1);
                    float  famp = outerR * 0.8f * (1.0f - ff * 0.5f);
                    forkPath1[fi] = float3(
                        fork1Origin.x + fDX * ff + BoltFBM(ff * 3.0f + fSeedX) * famp,
                        fork1Origin.y + fLenY * ff,
                        fork1Origin.z + fDZ * ff + BoltFBM(ff * 3.0f + fSeedZ) * famp);
                }

                float minDistFork1 = 1e6f;
                [loop]
                for (int fi = 0; fi < FORK_SAMPLES - 1; fi++)
                    minDistFork1 = min(minDistFork1, BoltSegDist(rayOrigin, rayDir, forkPath1[fi], forkPath1[fi + 1]));

                // ----------------------------------------------------------------
                // Fork 2 - complex bolts only; branches from ~56% up the main bolt.
                // FBM path array + BoltSegDist segments - masked by fork2Scale.
                // ----------------------------------------------------------------
                float  fo2s   = 0.56f;
                float  fo2amp = outerR * 1.5f * (1.0f - fo2s * 0.4f);
                float3 fork2Origin = float3(
                    boltX + BoltFBM(fo2s * 3.0f + seedX) * fo2amp,
                    cloudBaseY + fo2s * boltLength,
                    boltZ + BoltFBM(fo2s * 3.0f + seedZ) * fo2amp);
                float f2DX   = (frac(sin(cycleN + 83.1f) * 43758.5453f) - 0.5f) * boltLength * 0.4f;
                float f2DZ   = (frac(sin(cycleN + 97.3f) * 43758.5453f) - 0.5f) * boltLength * 0.4f;
                float f2LenY = boltLength * 0.35f;
                float f2SeedX = frac(sin(cycleN * 0.5f + 43.9f) * 43758.5453f) * 47.3f;
                float f2SeedZ = frac(sin(cycleN * 0.5f + 57.3f) * 43758.5453f) * 47.3f;

                float3 forkPath2[16];
                [loop]
                for (int fi = 0; fi < FORK_SAMPLES; fi++)
                {
                    float  ff   = (float)fi / (float)(FORK_SAMPLES - 1);
                    float  famp = outerR * 0.8f * (1.0f - ff * 0.5f);
                    forkPath2[fi] = float3(
                        fork2Origin.x + f2DX * ff + BoltFBM(ff * 3.0f + f2SeedX) * famp,
                        fork2Origin.y + f2LenY * ff,
                        fork2Origin.z + f2DZ * ff + BoltFBM(ff * 3.0f + f2SeedZ) * famp);
                }

                float minDistFork2 = 1e6f;
                [loop]
                for (int fi = 0; fi < FORK_SAMPLES - 1; fi++)
                    minDistFork2 = min(minDistFork2, BoltSegDist(rayOrigin, rayDir, forkPath2[fi], forkPath2[fi + 1]));

                float fork2Scale = isComplex ? 1.0f : 0.0f;

                // ----------------------------------------------------------------
                // Glow   applied ONCE per polyline using view-plane projected distance.
                // ----------------------------------------------------------------
                float3 boltAccumLight = float3(0.0f, 0.0f, 0.0f);
                float  boltAccumAlpha = 0.0f;

                // Main bolt glow.
                {
                    float d  = minDistMain;
                    float og = pow(saturate(outerR     / max(d, outerR     * 0.05f)), 0.8f);
                    float mg = pow(saturate(midR       / max(d, midR       * 0.03f)), 1.4f);
                    float ag = pow(saturate(innerAuraR / max(d, innerAuraR * 0.02f)), 2.0f);
                    float ig = pow(saturate(innerCoreR / max(d, innerCoreR * 0.01f)), 3.0f);

                    boltAccumLight += LightningBoltColor * og * LightningGlowIntensity;
                    boltAccumLight += lerp(LightningBoltColor, float3(0.88f, 0.95f, 1.0f), 0.6f) * mg * (LightningGlowIntensity * 3.5f);
                    boltAccumLight += float3(0.90f, 0.95f, 1.0f) * ag * (LightningGlowIntensity * 5.0f);
                    boltAccumLight += float3(0.96f, 0.98f, 1.0f) * ig * (LightningGlowIntensity * 14.0f);
                    boltAccumAlpha += og + mg * 1.5f + ag * 2.0f + ig * 5.0f;
                }

                // Fork 1 glow (dimmer, 65% radii).
                {
                    float fo = outerR     * 0.65f;
                    float fm = midR       * 0.65f;
                    float fa = innerAuraR * 0.65f;
                    float fc = innerCoreR * 0.65f;
                    float d  = minDistFork1;

                    float og = pow(saturate(fo / max(d, fo * 0.05f)), 0.8f);
                    float mg = pow(saturate(fm / max(d, fm * 0.03f)), 1.4f);
                    float ag = pow(saturate(fa / max(d, fa * 0.02f)), 2.0f);
                    float ig = pow(saturate(fc / max(d, fc * 0.01f)), 3.0f);

                    boltAccumLight += LightningBoltColor * og * LightningGlowIntensity * 0.6f;
                    boltAccumLight += lerp(LightningBoltColor, float3(0.88f, 0.95f, 1.0f), 0.6f) * mg * (LightningGlowIntensity * 2.5f);
                    boltAccumLight += float3(0.90f, 0.95f, 1.0f) * ag * (LightningGlowIntensity * 3.5f);
                    boltAccumLight += float3(0.96f, 0.98f, 1.0f) * ig * (LightningGlowIntensity * 9.0f);
                    boltAccumAlpha += og * 0.6f + mg + ag * 1.5f + ig * 3.0f;
                }

                // Fork 2 glow (complex bolts only, 55% radii, masked by fork2Scale).
                {
                    float fo = outerR     * 0.55f;
                    float fm = midR       * 0.55f;
                    float fa = innerAuraR * 0.55f;
                    float fc = innerCoreR * 0.55f;
                    float d  = minDistFork2;

                    float og = pow(saturate(fo / max(d, fo * 0.05f)), 0.8f);
                    float mg = pow(saturate(fm / max(d, fm * 0.03f)), 1.4f);
                    float ag = pow(saturate(fa / max(d, fa * 0.02f)), 2.0f);
                    float ig = pow(saturate(fc / max(d, fc * 0.01f)), 3.0f);

                    boltAccumLight += (LightningBoltColor * og * LightningGlowIntensity * 0.5f
                                    + lerp(LightningBoltColor, float3(0.88f, 0.95f, 1.0f), 0.6f) * mg * (LightningGlowIntensity * 2.0f)
                                    + float3(0.90f, 0.95f, 1.0f) * ag * (LightningGlowIntensity * 3.0f)
                                    + float3(0.96f, 0.98f, 1.0f) * ig * (LightningGlowIntensity * 7.0f)) * fork2Scale;
                    boltAccumAlpha += (og * 0.5f + mg * 0.8f + ag * 1.2f + ig * 2.5f) * fork2Scale;
                }

                boltAccumLight *= boltPulse;
                boltAccumAlpha *= boltPulse;

                // ----------------------------------------------------------------
                // Cloud illumination: tint nearby cloud voxels with LightningBoltColor.
                // The view ray hits the cloud slab at t = CloudBottomHeight / (-rayDir.y).
                // Horizontal distance from that hit point to the bolt axis drives a
                // large-radius exponential falloff, modulated by cloud density (alpha)
                // so only actual cloud pixels receive the illumination tint.
                // ----------------------------------------------------------------
                {
                    float  tCloud     = CloudBottomHeight / max(-rayDir.y, 0.001f);
                    float3 cloudHitPt = rayOrigin + rayDir * tCloud;
                    float2 toBot      = float2(cloudHitPt.x - boltX, cloudHitPt.z - boltZ);
                    float  distXZ     = length(toBot);
                    float  illumR     = CloudBottomHeight * 1.2f * (1.0f + 0.02f * (LightningBoltLengthScale - 1.0f));
                    float  illum      = exp(-distXZ / illumR);
                    cloudResult.rgb  += LightningBoltColor * illum * boltPulse
                                        * LightningGlowIntensity * 0.25f * cloudResult.a;
                }

                float boltAlphaContrib = saturate(boltAccumAlpha * 0.5f);
                cloudResult.rgb += boltAccumLight;
                cloudResult.a    = max(cloudResult.a, boltAlphaContrib);
            }
        }
    }

    // --- Universal thin-edge opacity suppression ---
    // Very low alpha values carry per-pixel jitter noise. Rather than using
    // smoothstep (which creates contour bands / quantized levels in the low-
    // alpha region), use a linear saturate ramp. This cleanly fades the
    // boundary fringe to zero without introducing any non-linear S-curve
    // that would make certain alpha ranges pile up into visible bands.
    // At far range (low elevation), the threshold rises to handle the
    // larger boundary noise of distant clouds.
    {
        float elevation  = saturate(-rayDir.y);
        float distFactor = saturate(1.0f - elevation * 5.0f);
        float thinThresh = lerp(0.03f, 0.12f, distFactor);
        if (thinThresh > 0.0f)
            cloudResult.a *= saturate(cloudResult.a / thinThresh);
    }

    // Debug views.
    if (CloudDebugView != 0)
        cloudResult = DebugVisualization(rayOrigin, rayDir, input.Position.xy, cloudResult, PrimaryStepCount);

    // NOTE: HorizonAtmosphericFade is NOT applied here — it is deferred to
    // PSCloudComposite so the cloud RT stores un-faded alpha.  This lets the
    // bleed-through-mountains re-composite use the full cloud opacity directly
    // instead of the lossy divide-out recovery that produced color artifacts.
    if (CloudDebugView == 0)
    {
        // Distance fade: attenuate clouds based on how far the cloud slab entry
        // point is from the camera. DistanceFade CB parameter controls strength
        // (0 = no distance fade, 1 = full fade). The fade band runs from 60% to
        // 100% of the max march distance (CloudThickness * 6).
        if (DistanceFade > 0.0f)
        {
            float2 tRange = IntersectCloudVolume(rayOrigin, rayDir);
            if (tRange.x > 0.0f)
            {
                float maxDist  = CloudThickness * 6.0f;
                float distFade = 1.0f - smoothstep(maxDist * 0.6f, maxDist, tRange.x);
                cloudResult.a *= lerp(1.0f, distFade, DistanceFade);
            }
        }

        // Post-fade thin-alpha cleanup: DistanceFade can reduce previously
        // solid alpha to very small values. Use a linear ramp (not smoothstep)
        // to avoid introducing contour bands at the fade boundary.
        cloudResult.a *= saturate(cloudResult.a / 0.025f);

        // AltocumulusMid global layer opacity via Coverage slider [0,1].
        // Applied AFTER DistanceFade so the fade gradient is preserved
        // at all opacity levels.
        if (CloudType == 1)
            cloudResult.a *= saturate(Coverage);
    }

    return cloudResult;
}

// ===========================================================================
// PSCloudComposite   Upscale half-res clouds and alpha-blend over scene.
//
// The cloud render target (RGBA: lit cloud color + opacity) is bound to t0.
// Hardware alpha blending (AlphaBlend) composites this over the existing
// framebuffer content, which may contain:
//   - Legacy sky bitmap layer(s)
//   - Horizon mesh (possibly closing overhead)
//   - Starfield
//   - Black void where nothing was drawn
//
// Upsampling: uses a 3x3 cross-bilateral filter (edge-aware Gaussian) to
// smooth out per-pixel jitter-based opacity noise at cloud silhouettes
// when upscaling from half resolution to full. Combined spatial Gaussian
// and bilateral alpha weighting produces a continuous, dither-free boundary.
// ===========================================================================

float4 PSCloudComposite(VSOutput input) : SV_TARGET
{
    float2 texelSize = InvCloudRenderSize;
    float2 uv = input.UV;

    // Centre sample   used as bilateral reference.
    float4 cCenter = SceneColorTexture.Sample(LinearSamp, uv);
    float  refAlpha = cCenter.a;

    // 3x3 bilateral upsampling filter.
    // The previous 4-tap bilateral with tight sigma (0.15) preserved per-pixel
    // jitter-based opacity noise at the cloud silhouette: adjacent pixels with
    // alpha 0.0 vs 0.04 (from jitter hit/miss) were NOT blended together
    // because the sigma rejected their alpha difference. This preserved the
    // structured dithering pattern in the upsampled output.
    //
    // Fix: 3x3 kernel (covers 3?--3 half-res texels = 6?--6 full-res pixels)
    // with wider bilateral sigma (0.4). The wider kernel spatially averages
    // the jitter noise pattern, and the larger sigma allows cross-boundary
    // blending so the dithered 0/non-zero alpha pixels are smoothed into a
    // continuous gradient. Spatial Gaussian weighting (sigma=1.0 texel)
    // ensures the center pixel dominates while neighbors contribute softly.
    const float spatialSigma2   = 2.0f;  // 2 * 1.0?? (spatial sigma = 1 texel)
    const float alphaSigma2RGB  = 0.32f; // 2 * 0.4?? (RGB bilateral sigma = 0.4)
    const float alphaSigma2Edge = 1.28f; // 2 * 0.8?? (alpha bilateral sigma = 0.8)

    float3 accumRGBPremul = float3(0.0f, 0.0f, 0.0f);
    float  accumAlpha	 = 0.0f;
    float  accumWeightRGB = 0.0f;
    float  accumWeightA   = 0.0f;

    [unroll]
    for (int oy = -1; oy <= 1; oy++)
    {
        [unroll]
        for (int ox = -1; ox <= 1; ox++)
        {
            float2 offset = float2((float)ox, (float)oy);
            float4 tap = SceneColorTexture.Sample(LinearSamp, uv + offset * texelSize);

            // Spatial Gaussian weight.
            float spatialDist2 = float(ox * ox + oy * oy);
            float wSpatial = exp(-spatialDist2 / spatialSigma2);

            // Bilateral alpha weights.
            float alphaDiff = tap.a - refAlpha;
            float wAlphaRGB  = exp(-(alphaDiff * alphaDiff) / alphaSigma2RGB);
            float wAlphaEdge = exp(-(alphaDiff * alphaDiff) / alphaSigma2Edge);

            float wRGB = wSpatial * wAlphaRGB;
            float wA   = wSpatial * wAlphaEdge;

            // Composite de-banding in premultiplied-alpha space.
            // In straight-alpha filtering, fully transparent texels (alpha=0)
            // still contribute their RGB (usually black) and create gray/dark
            // halos around thin cloud edges. Premultiplying RGB by alpha before
            // filtering removes that fringe contamination.
            float3 tapRGBPremul = tap.rgb * tap.a;
            accumRGBPremul += tapRGBPremul * wRGB;
            accumAlpha	 += tap.a * wA;
            accumWeightRGB += wRGB;
            accumWeightA   += wA;
        }
    }

    if (accumWeightRGB < 0.001f || accumWeightA < 0.001f)
        return cCenter;

    float outAlpha	  = accumAlpha / accumWeightA;
    float3 outRGBPremul = accumRGBPremul / accumWeightRGB;

    // Thin-alpha suppression: soft-threshold stochastic speckle noise at cloud
    // silhouettes. Applied to premultiplied RGB so stochastic single-pixel hits
    // don't create bright speckles at cloud edges.
    float tinyAlphaThresh    = lerp(0.035f, 0.02f, saturate((Absorption - 0.2f) * 2.5f));
    float alphaSuppress      = saturate(outAlpha / tinyAlphaThresh);
    float3 outRGBPremulFinal = outRGBPremul * alphaSuppress;
    float  finalAlpha        = outAlpha * alphaSuppress;

    // HorizonAtmosphericFade is applied here in the composite pass, NOT during
    // raymarching.  The cloud RT stores un-faded alpha with valid RGB so the
    // bilateral upsampler always has good signal — even near the horizon.
    //
    // Normal pass  (CloudIsBleedPass == 0): full HorizonAtmosphericFade, clouds
    //   dissolve naturally at the horizon.
    // Bleed pass   (CloudIsBleedPass > 0 == bleedStrength): no per-pixel
    //   elevation masking at all. Organic cloud shapes come entirely from the
    //   volumetric raymarcher (AltoBottomSoftness, cloud slab geometry). Any
    //   screen-space elevation fade would produce a flat horizontal cut that
    //   ignores the mountain silhouette — exactly what we don't want.
    float3 compRayDir = GetViewRayDir(uv);
    float opacityScale;
    if (CloudIsBleedPass < 0.001f)
        opacityScale = HorizonAtmosphericFade(compRayDir) * CloudCompositeScale;
    else
        opacityScale = CloudIsBleedPass * CloudCompositeScale;

    float3 cloudContrib = saturate(outRGBPremulFinal * opacityScale);
    float  cloudAlpha   = saturate(finalAlpha * opacityScale);

    // Recover straight-alpha cloud color for thick-cloud alpha blending.
    float3 cloudStraight = (cloudAlpha > 0.001f)
                         ? (cloudContrib / cloudAlpha)
                         : float3(0.0f, 0.0f, 0.0f);

    // Read the scene background (sky) from the pre-cloud backup copy.
    float3 bg = SceneBackgroundTex.Sample(LinearSamp, uv).rgb;

    // === Hybrid screen / alpha composite ===
    //
    // Screen blend:  result = 1 - (1 - bg) * (1 - cloud)
    //   → Can only brighten. Black cloud = no effect. Perfect for thin bright clouds.
    //   → But dark clouds become transparent (bad for thunderstorms / rain).
    //
    // Alpha blend:   result = lerp(bg, cloudColor, alpha)
    //   → Can darken. Dark clouds properly absorb light.
    //   → But thin bright edges get dark halos.
    //
    // Solution: blend between both modes based on cloud brightness.
    //   Bright areas → screen blend (no halos).
    //   Dark areas   → alpha blend (proper absorption).
    //
    // The crossover threshold is ~27/255 ≈ 0.106 in linear space.
    // A smoothstep transition avoids any visible seam.

    // Screen blend result.
    float3 screenResult = 1.0f - (1.0f - bg) * (1.0f - cloudContrib);

    // Alpha blend result.
    float3 alphaResult = lerp(bg, cloudStraight, cloudAlpha);

    // Brightness-based blend factor: measured on straight-alpha cloud color,
    // NOT on the premultiplied contribution. The premultiplied value is small
    // for thin bright edges (bright * low alpha ≈ small), which would wrongly
    // classify them as dark and apply alpha blend → dark halos. cloudStraight
    // gives the intrinsic color brightness regardless of cloud density.
    float cloudLuma = dot(cloudStraight, float3(0.299f, 0.587f, 0.114f));

    // Dual-zone screen blend: screen blend at both the bright AND dark ends,
    // alpha blend only in the mid-luminance range.
    //   luma > BlendThresholdHigh  →  bright clouds / thin edges  →  screen blend (no halos)
    //   luma < BlendThresholdLow   →  very dark cloud edges        →  screen blend (no dark halos)
    //   BlendThresholdLow ≤ luma ≤ BlendThresholdHigh  →  alpha blend (dense clouds absorb properly)

    // Sunset / night occlusion modulation:
    // As the sun descends toward the horizon (CloudSunElevation → 0) the blend thresholds
    // shift so that clouds become fully opaque (alpha-blend dominated).  This causes the
    // sun, moon, and stars to disappear behind the cloud layer instead of bleeding through.
    // Transition window: elevation [kSunsetStart … 0.0].  Below the horizon (night) the
    // fully-shifted values are kept (sunsetFactor clamped to 1).
    const float kSunsetStart = 0.25f;   // elevation (sin) at which sunset modulation begins
    const float kSunsetHigh  = 0.199f;  // target BlendThresholdHigh  at the horizon
    const float kSunsetWidth = 0.400f;  // target BlendThresholdHighWidth at the horizon
    const float kSunsetLow   = 0.000f;  // target BlendThresholdLow   at the horizon
    float sunsetFactor  = saturate(1.0f - CloudSunElevation / kSunsetStart);
    float effectiveHigh  = lerp(BlendThresholdHigh,      kSunsetHigh,  sunsetFactor);
    float effectiveWidth = lerp(BlendThresholdHighWidth,  kSunsetWidth, sunsetFactor);
    float effectiveLow   = lerp(BlendThresholdLow,        kSunsetLow,   sunsetFactor);

    // Bright side: transition width comes from the per-preset BlendThresholdHighWidth
    // (runtime-blended toward the sunset target above).
    // Dark side: use a small fixed transition (0.025).
    const float darkTransW = 0.025f;
    float brightScreen = smoothstep(effectiveHigh - effectiveWidth,
                                    effectiveHigh + effectiveWidth, cloudLuma);
    float darkScreen   = 1.0f - smoothstep(effectiveLow - darkTransW,
                                           effectiveLow + darkTransW, cloudLuma);
    float screenFactor = max(brightScreen, darkScreen);

    // Final hybrid composite.
    float3 finalColor = lerp(alphaResult, screenResult, screenFactor);

    // Where there is no cloud at all, preserve the background exactly.
    // cloudAlpha acts as the master presence signal.
    finalColor = lerp(bg, finalColor, saturate(cloudAlpha * 10.0f));

    return float4(finalColor, 1.0f);
}

// ===========================================================================
// PSCloudOcclusion   Multi-sample cloud transmittance for sun occlusion.
//
// Marches transmittance along the sun direction and 4 ring offsets around it,
// approximating partial disk coverage when a cloud only partially obscures
// the visible sun disk. Returns averaged visibility in [0,1] (red channel).
//   1.0 = sun fully unoccluded
//   0.0 = sun completely hidden
// ===========================================================================

// March Beer-Lambert transmittance along one direction through the cloud volume.
float MarchSunTransmittance(float3 rayOrigin, float3 rayDir)
{
    // Intersect cloud slab. Negative tRange.x means no intersection.
    float2 tRange = IntersectCloudVolume(rayOrigin, rayDir);
    if (tRange.x < 0.0f)
        return 1.0f; // Clear path   fully visible.

    int   occSteps = max(PrimaryStepCount / 2, 4);
    float maxDist  = min(tRange.y - tRange.x, CloudThickness * 4.0f);
    float stepSize = maxDist / (float)occSteps;
    float t		= tRange.x;
    float transm   = 1.0f;

    [loop]
    for (int i = 0; i < occSteps; i++)
    {
        if (transm < 0.01f)
            break;

        float3 samplePos  = rayOrigin + rayDir * t;
        float  heightFrac = HeightFraction(samplePos.y,
                            rayOrigin.y - CloudBottomHeight, CloudThickness);

        if (heightFrac >= 0.0f && heightFrac <= 1.0f)
        {
            float density = CloudDensityAtWorldPos(samplePos, heightFrac, false, 0.0f);
            transm *= exp(-density * Absorption * stepSize);
        }

        t += stepSize;
    }

    return transm;
}

float4 PSCloudOcclusion(VSOutput input) : SV_TARGET
{
    // Angular half-radius of the sun disk used for the volumetric ray-march offsets.
    static const float SUN_DISK_HALF_ANGLE = 0.05f;

    float3 rayOrigin = CamPositionWS.xyz;
    float3 sunDir	= normalize(CloudLightDirection);

    // Build an orthonormal tangent frame perpendicular to the sun direction.
    float3 refUp = abs(sunDir.y) < 0.9f ? float3(0.0f, 1.0f, 0.0f) : float3(1.0f, 0.0f, 0.0f);
    float3 right = normalize(cross(sunDir, refUp));
    float3 up	= cross(sunDir, right);

    // 5-tap volumetric transmittance: center + 4 cardinal ring offsets.
    float3 dirs[5];
    dirs[0] = sunDir;
    dirs[1] = normalize(sunDir + right * SUN_DISK_HALF_ANGLE);
    dirs[2] = normalize(sunDir - right * SUN_DISK_HALF_ANGLE);
    dirs[3] = normalize(sunDir + up	* SUN_DISK_HALF_ANGLE);
    dirs[4] = normalize(sunDir - up	* SUN_DISK_HALF_ANGLE);

    float totalTransmittance = 0.0f;

    [unroll]
    for (int s = 0; s < 5; s++)
        totalTransmittance += MarchSunTransmittance(rayOrigin, dirs[s]);

    float rayVisibility = totalTransmittance * 0.2f; // averaged across 5 taps

    // ---------------------------------------------------------------------------
    // Screen-space cloud coverage around the sun.
    //
    // The volumetric ray-march only samples along the exact sun direction and a
    // few close neighbours. When the cloud density field has a natural gap right
    // at the sun direction (common in procedural noise), all ray taps fall inside
    // the gap and report near-zero density even though the surrounding rendered
    // clouds look dense. The screen-space sampling below fixes this by measuring
    // the actual cloud alpha in the cloud half-res render target (bound to t0)
    // across a wider "corona" area around the sun's projected screen position.
    //
    // CORONA_RADIUS: sample ring offset in half-res cloud RT pixels.
    // 8 pixels at typical half-res (960x540) ??? 16 full-res pixels from sun center.
    // ---------------------------------------------------------------------------
    static const float CORONA_RADIUS = 8.0f;

    bool sunOnScreen = (SunScreenUV.x > 0.01f && SunScreenUV.x < 0.99f &&
                        SunScreenUV.y > 0.01f && SunScreenUV.y < 0.99f);

    float visibility = rayVisibility;

    if (sunOnScreen)
    {
        // Offset step per ring sample in cloud RT UV space.
        float2 stepXY = InvCloudRenderSize * CORONA_RADIUS;
        // 0.7071 = 1/sqrt(2): diagonal ring samples at equal radius.
        float2 diagXY = stepXY * 0.7071f;

        // 9-tap pattern: center + 4 cardinal + 4 diagonal.
        float cloudAlpha = 0.0f;
        cloudAlpha += SceneColorTexture.Sample(LinearSamp, SunScreenUV).a;
        cloudAlpha += SceneColorTexture.Sample(LinearSamp, SunScreenUV + float2( stepXY.x,  0.0f)).a;
        cloudAlpha += SceneColorTexture.Sample(LinearSamp, SunScreenUV + float2(-stepXY.x,  0.0f)).a;
        cloudAlpha += SceneColorTexture.Sample(LinearSamp, SunScreenUV + float2( 0.0f,  stepXY.y)).a;
        cloudAlpha += SceneColorTexture.Sample(LinearSamp, SunScreenUV + float2( 0.0f, -stepXY.y)).a;
        cloudAlpha += SceneColorTexture.Sample(LinearSamp, SunScreenUV + float2( diagXY.x,  diagXY.y)).a;
        cloudAlpha += SceneColorTexture.Sample(LinearSamp, SunScreenUV + float2(-diagXY.x,  diagXY.y)).a;
        cloudAlpha += SceneColorTexture.Sample(LinearSamp, SunScreenUV + float2( diagXY.x, -diagXY.y)).a;
        cloudAlpha += SceneColorTexture.Sample(LinearSamp, SunScreenUV + float2(-diagXY.x, -diagXY.y)).a;
        cloudAlpha /= 9.0f;

        // Screen-space visibility: 1 = no cloud near sun, 0 = sun area fully covered.
        float screenVisibility = 1.0f - cloudAlpha;

        // Use the most-occluding estimate from either method.
        visibility = min(rayVisibility, screenVisibility);
    }

    return float4(visibility, 0.0f, 0.0f, 1.0f);
}
