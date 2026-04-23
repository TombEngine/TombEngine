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

// Pre-baked noise textures — replace per-sample procedural noise.
Texture3D<float4> CloudNoise3D  : register(t5);  // 128^3 RGBA8: R=Perlin, G=Value, B=CurlX, A=CurlZ
Texture2D<float2> CloudWorley2D : register(t6);  // 256^2 RG8:   R=Worley seed1, G=Worley seed2

SamplerState PointSamp	  : register(s1);  // Point sampler
SamplerState LinearSamp	 : register(s2);  // Linear sampler (WRAP addressing — used for noise)

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

// ===========================================================================
// Texture-based noise wrappers (replaces procedural noise on the hot path).
//
// The 3D/2D textures are tileable with a period of NoiseTilePeriod noise
// cells.  UV = noiseCoord * NoiseUVScale maps noise-space coordinates into
// [0,1] UV space, and D3D11_TEXTURE_ADDRESS_WRAP makes them tile seamlessly.
// ===========================================================================

static const float NoiseTilePeriod = 16.0f;
static const float NoiseUVScale    = 1.0f / NoiseTilePeriod;   // 0.0625
static const float WorleyTilePeriod = 16.0f;
static const float WorleyUVScale    = 1.0f / WorleyTilePeriod;  // 0.0625

// Sample Perlin gradient noise from pre-baked 3D texture (channel R).
float TexPerlin3D(float3 p)
{
    return CloudNoise3D.SampleLevel(LinearSamp, p * NoiseUVScale, 0).r;
}

// Sample value noise from pre-baked 3D texture (channel G).
float TexValue3D(float3 p)
{
    return CloudNoise3D.SampleLevel(LinearSamp, p * NoiseUVScale, 0).g;
}

// Sample pre-computed curl vector from 3D texture (channels B,A).
// Returns the 2D curl displacement in the XZ plane (divergence-free).
// The texture stores (dN/dz, -dN/dx) remapped from [-1,1] to [0,1].
float2 TexCurl2D(float3 p)
{
    float2 ba = CloudNoise3D.SampleLevel(LinearSamp, p * NoiseUVScale, 0).ba;
    return ba * 2.0f - 1.0f;
}

// Sample Worley F1 distance from pre-baked 2D texture (channel R).
float TexWorley2D(float2 p)
{
    return CloudWorley2D.SampleLevel(LinearSamp, p * WorleyUVScale, 0).r;
}

// Sample second Worley pattern from pre-baked 2D texture (channel G).
float TexWorley2D_B(float2 p)
{
    return CloudWorley2D.SampleLevel(LinearSamp, p * WorleyUVScale, 0).g;
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
        v += a * octWeight * TexValue3D(s);
        s *= 2.37f;  // Lacunarity (non-power-of-2 avoids tiling artifacts)
        a *= 0.38f;  // Low persistence: each octave contributes much less than the previous
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
//
// advect: wind/evo translation in the same pre-scaled space as p.
//   This offset is added identically to every octave and does NOT accumulate
//   the lacunarity factor. Without this separation the wind displacement is
//   scaled by lacunarity^oct per octave, so octave 4 moves ~49x faster in
//   feature-space than octave 0 — causing the high-gain frayed edges to
//   flicker every frame the cloud translates. By keeping advect constant,
//   all octaves translate as one coherent mass through space.
// p        : curl-deformed position (used for coarse octaves 0-2, organic shape).
// p_stable : pre-curl position    (used for fine  octaves 3-4, no shimmer).
// advect   : wind+evo translation added identically to every octave so all
//            scales translate at the same absolute speed (no differential flicker).
//
// The split between curled/stable is the fix for thin-edge flickering:
// curl noise evolves at frame-visible rates, and when scaled by lacunarity^3,4
// small position changes cause rapid noise-value changes in fine octaves —
// pushing thin cloud edges across the coverage threshold every few frames.
// Coarse octaves are unaffected (they have large wavelengths that don't alias).
float FBMAlto5(float3 p, float3 p_stable, float3 advect, float lacunarity, float gain, float billowBlend, float lod)
{
    float v          = 0.0f;
    float a          = 0.5f;
    float3 s         = p;
    float3 s_stable  = p_stable;
    float fineGainFade = saturate((gain - 0.38f) * 3.0f);

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

        // High FBM gain amplifies the finest Alto octaves into moving micro-detail.
        // Dampen only octaves 3-4 so the large cloud masses remain unchanged.
        if (oct == 3)
            octWeight *= lerp(1.0f, 0.60f, fineGainFade);
        else if (oct == 4)
            octWeight *= lerp(1.0f, 0.25f, fineGainFade);

        // Fine octaves (3-4) use the stable (pre-curl) position to avoid
        // flickering at thin cloud edges. Coarse octaves use the curl-deformed
        // position for organic large-scale shape variation.
        float3 sUse = (oct >= 3) ? s_stable : s;

        // advect is added AFTER scaling — the wind/evo displacement is the
        // same absolute offset in noise-space for every octave.  's' (the
        // spatial-structure coordinate) still scales normally so each octave
        // sees a different level of detail in the cloud morphology.
        float pN   = TexPerlin3D(sUse + advect);
        float valN = pN;
        float bilN = abs(pN * 2.0f - 1.0f); // Perlin billow = abs(snoise) equivalent

        v += a * octWeight * lerp(valN, bilN, billowBlend);
        s        *= lacunarity;
        s_stable *= lacunarity;
        a        *= gain;
    }
    return v;
}

// Lightweight 3-octave Perlin FBM for dissolving morph sources.
// Uses only the first 3 octaves of FBMAlto5 — sufficient for the coarse
// shape of clouds that are fading out. Fine-detail octaves (3-4) contribute
// imperceptible structure during dissolution and are skipped entirely.
// Saves ~40% of per-sample FBM cost compared to FBMAlto5.
//
// Does NOT use the split curled/stable position — all 3 octaves use the same
// position since the fine-octave flickering fix is irrelevant for dissolving clouds.
float FBMAlto3(float3 p, float3 advect, float lacunarity, float gain, float billowBlend, float lod)
{
    float v  = 0.0f;
    float a  = 0.5f;
    float3 s = p;

    [unroll]
    for (int oct = 0; oct < 3; oct++)
    {
        float octWeight;
        if      (oct <= 1) octWeight = 1.0f;
        else               octWeight = max(saturate(1.0f - lod), 0.35f);

        float pN   = TexPerlin3D(s + advect);
        float valN = pN;
        float bilN = abs(pN * 2.0f - 1.0f);

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
        v += a * octWeight * TexValue3D(s);
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
// Dissolve mask (transform-preset driven)
// ===========================================================================

// Per-cluster timing offset mask [0,1].
// Feature size = 1/0.0001 = 10000 world units: large enough that the noise
// is nearly constant inside a single cluster (no internal holes), but small
// enough that 3-6 distinct timing zones exist across the visible cloud field.
float DissolveMask(float3 skyPos)
{
    float3 dissolveCoord = skyPos * 0.0001f;
    float n1 = TexPerlin3D(dissolveCoord + float3(100.0f, 0.0f, 200.0f));
    float n2 = TexPerlin3D(dissolveCoord * 1.7f + float3(47.0f, 0.0f, 83.0f));
    return n1 * 0.6f + n2 * 0.4f;  // [0, 1]
}

// ===========================================================================
// Transform-preset dissolve / formation / drift-out helpers.
// Shared by Alto (morph & non-morph) and standard cloud paths.
// ===========================================================================

// Cluster-staggered edge-first dissolve. Returns modified density.
float ApplyDissolve(float density, float3 skyPos, float phase)
{
    if (phase <= 0.001f || density <= 0.0001f)
        return density;
    const float maxDelay = 0.55f;
    float clusterDelay   = DissolveMask(skyPos) * maxDelay;
    float localPhase     = saturate((phase - clusterDelay) / (1.0f - maxDelay));
    if (localPhase <= 0.0001f)
        return density;
    float cloudStr  = 1.0f - exp(-density * 8.0f);
    float threshold = lerp(-0.1f, 1.1f, localPhase);
    float edgeW     = 0.08f;
    return density * smoothstep(threshold - edgeW, threshold + edgeW, cloudStr);
}

// Cluster-staggered core-first formation (reverse dissolve). Returns modified density.
float ApplyFormation(float density, float3 skyPos, float phase)
{
    if (phase <= 0.001f || phase >= 0.999f || density <= 0.0001f)
        return density;
    const float maxDelay = 0.55f;
    float clusterDelay   = (1.0f - DissolveMask(skyPos)) * maxDelay;
    float localPhase     = saturate((phase - clusterDelay) / (1.0f - maxDelay));
    float cloudStr  = 1.0f - exp(-density * 8.0f);
    float threshold = lerp(1.1f, -0.1f, localPhase);
    float edgeW     = 0.08f;
    return density * smoothstep(threshold - edgeW, threshold + edgeW, cloudStr);
}

// Overloads accepting a pre-computed DissolveMask value.
// During CloudMorph transitions, both ApplyDissolve and ApplyFormation are
// called on the same skyPos within the same sample. Computing DissolveMask
// once and passing the result here saves 4 PerlinNoise3D calls per sample.
float ApplyDissolveWithMask(float density, float mask, float phase)
{
    if (phase <= 0.001f || density <= 0.0001f)
        return density;
    const float maxDelay = 0.55f;
    float clusterDelay   = mask * maxDelay;
    float localPhase     = saturate((phase - clusterDelay) / (1.0f - maxDelay));
    if (localPhase <= 0.0001f)
        return density;
    float cloudStr  = 1.0f - exp(-density * 8.0f);
    float threshold = lerp(-0.1f, 1.1f, localPhase);
    float edgeW     = 0.08f;
    return density * smoothstep(threshold - edgeW, threshold + edgeW, cloudStr);
}

float ApplyFormationWithMask(float density, float mask, float phase)
{
    if (phase <= 0.001f || phase >= 0.999f || density <= 0.0001f)
        return density;
    const float maxDelay = 0.55f;
    float clusterDelay   = (1.0f - mask) * maxDelay;
    float localPhase     = saturate((phase - clusterDelay) / (1.0f - maxDelay));
    float cloudStr  = 1.0f - exp(-density * 8.0f);
    float threshold = lerp(1.1f, -0.1f, localPhase);
    float edgeW     = 0.08f;
    return density * smoothstep(threshold - edgeW, threshold + edgeW, cloudStr);
}

// Wind-directional drift-out dissolution. Returns [0,1] suppression factor
// (multiply with density). Returns 1.0 when DriftOutProgress <= 0.
float DriftOutFactor(float2 skyPosXZ)
{
    if (DriftOutProgress <= 0.001f)
        return 1.0f;
    float2 windDir2D = (dot(WindDirection, WindDirection) > 0.001f)
                     ? normalize(WindDirection) : float2(1.0f, 0.0f);
    float windProjSky    = dot(skyPosXZ, windDir2D);
    float fieldExtent    = max(CloudBottomHeight * 4.0f, 1.0f);
    float normalizedProj = windProjSky / fieldExtent;
    float boundary = lerp(-1.5f, 1.5f, DriftOutProgress);
    float softness = 0.4f;
    return 1.0f - smoothstep(boundary - softness, boundary + softness, normalizedProj);
}

// ===========================================================================
// Parameterized Alto density evaluation for CloudMorph dual-density system.
// Called once for normal rendering, twice during morph (target + source).
// Only the density-shaping params vary; wind/time/fade are shared globals.
// ===========================================================================

struct AltoDensityParams
{
    float CloudSize;     // AltoCloudSize      — feature scale multiplier
    float CloudAmount;   // AltoCloudAmount    — coverage/fill control
    float BillowStr;     // AltoBillowStrength — billow vs smooth FBM
    float CovSoftWidth;  // AltoCovSoftWidth   — coverage soft-threshold width
    float FbmLac;        // AltoFbmLacunarity  — FBM frequency ratio
    float FbmGain;       // AltoFbmGain        — FBM amplitude scaling
    float FbmScale;      // AltoFbmScale       — FBM input pre-scale (2.032=reference)
    float BottomSoft;    // AltoBottomSoftness  — organic bottom shaping
    float ZenithBias;    // AltoZenithBias     — sky-height distribution bias
    float EvolutionSpd;  // EvolutionSpeed     — formation cycling rate
};

// Returns raw Alto density BEFORE dissolve/formation/zenith-cap/drift-out.
float EvalAltoDensityCore(float3 skyPos, float heightFrac, float skyH,
                          float distLOD, float distLOD2, AltoDensityParams ap)
{
    // --- Cheap early-outs before curl/FBM ---
    // AltoHorizonWidth zenith cap: pixels clearly outside the active sky zone
    // always produce zero density. Skip all expensive work for those rays.
    if (AltoHorizonWidth > 0.001f && CloudIsBleedPass < 0.001f &&
        skyH < (AltoHorizonWidth * 0.90f - 0.09f))
        return 0.0f;

    // DriftOutProgress: skip pixels that are fully suppressed by the upwind
    // dissolution boundary (DriftOutFactor returns 0 for fully suppressed).
    if (DriftOutProgress > 0.001f && DriftOutFactor(skyPos.xz) < 0.001f)
        return 0.0f;

    // --- Sky-height redistribution (bias-based) ---
    float horizonEdge = saturate(HorizonFade * 0.10f + DistanceFade * 0.06f);
    static const float zenithEdge = 0.65f;
    float activeRange = max(zenithEdge - horizonEdge, 0.05f);
    float biasFactor;
    {
        float t = saturate((skyH - horizonEdge) / activeRange);
        float s = t * t * (3.0f - 2.0f * t);
        biasFactor = s * 2.0f - 1.0f;
    }

    float densityShift = -ap.ZenithBias * biasFactor * 0.4f;

    float baseScale = 0.001f * ap.CloudSize;

    // --- Global advection ---
    float3 windOfs = float3(WindDirection.x, 0.0f, WindDirection.y) * WindSpeed;

    // --- Evolution-driven formation scrolling ---
    // EvoAccumOffset is pre-integrated in the renderer (like WindSpeed/WindAccumOffset),
    // so it never decreases even when EvolutionSpeed transitions to a lower value.
    // This prevents clouds from drifting against the wind during morph transitions
    // where EvolutionSpeed drops (e.g. RainSnowOvercast→Altocumulus).
    //
    // Direction is PERPENDICULAR to wind (rotated 90°) so that EvolutionSpeed
    // does NOT add to the visible wind-direction speed. Otherwise, on a preset
    // transition that ramps EvSpd from 0 to >0 (e.g. Altocumulus→Thunderstorm),
    // the cloud field appears to suddenly accelerate in wind direction
    // ("time-lapse"), and on the reverse transition it appears to drift
    // against the wind as the curl-warp untwists. With a perpendicular evoDir,
    // wind-direction speed stays = global WindSpeed regardless of EvSpd.
    float3 evoDir = float3(-WindDirection.y, 0.0f, WindDirection.x);
    float3 evoOfs = evoDir * EvoAccumOffset;

    float3 p = skyPos * baseScale + windOfs + evoOfs;

    // Save the pre-curl position. Curl deformation shifts p in world (scaled)
    // space, which gets amplified by lacunarity^3,4 for fine FBM octaves.
    // At the High band's evolution rate (flowTime*1.80) the per-frame position
    // change reaches ~3% of an oct-4 wavelength, making thin cloud edges
    // flicker as they oscillate across the coverage threshold.
    // Fix: fine octaves (3-4) always sample from the un-curled position so
    // they form a stable detail pattern; only coarse octaves (0-2) see the
    // curl deformation for organic large-scale shape variation.
    float3 p_before_curl = p;

    // --- Organic deformation via time-evolving curl noise flow field ---
    // EvolutionSpeed should make clouds breathe and reorganize, but not snake
    // strongly from side to side. Keep the advection/evolution offset above,
    // but damp the time-evolving curl field so the cloud body stays more stable.
    //
    // FlowAccumOffset is pre-integrated in the renderer (like WindAccumOffset),
    // so it is always monotonically non-decreasing. Using it instead of
    // CloudTime*ap.EvolutionSpd*0.16 prevents the curl warp and windBias from
    // reversing when EvolutionSpeed transitions to a lower value (e.g. 1.013→0
    // on RainSnowOvercast→Altocumulus), which previously caused visible
    // backwards cloud-feature drift.
    float flowTime = FlowAccumOffset;
    float heightFlow = lerp(1.0f, 0.65f, saturate(heightFrac));
    float2 windBias = WindDirection * flowTime * 0.03f;
    float curlDamp = lerp(0.85f, 0.40f, saturate(ap.EvolutionSpd * 0.25f));

    // Curl warp: only executed when CurlWarpStrength > 0. At exactly 0.0 the entire
    // domain-warp is bypassed so clouds are shaped purely by FBM/Worley (no organic
    // deformation). CurlWarpStrength linearly scales all three displacement bands.
    //
    // Performance: at distLOD >= 0.45 all three curl bands contribute sub-pixel
    // displacements, so the entire curl warp block is skipped — saving 4-12
    // ValueNoise3D calls per sample at medium-to-far range.
    if (CurlWarpStrength > 0.001f && distLOD < 0.45f)
    {
        float curlAmp = curlDamp * CurlWarpStrength;

        // Low Frequency Band — always evaluated when curl is active.
        {
            float tLow  = flowTime * 0.35f * heightFlow;
            float3 pLow = float3(p.x, 0.0f, p.z) * 0.41f
                        + float3(3.17f + windBias.x + tLow * 0.7f,
                                 0.0f,
                                 7.63f + windBias.y + tLow * 0.5f);
            float2 cLow = TexCurl2D(pLow);
            p.x += cLow.x * 0.11f * curlAmp;
            p.z += cLow.y * 0.11f * curlAmp;
        }

        // Mid Frequency Band (skipped at far range).
        if (distLOD < 0.35f)
        {
            float tMid  = flowTime * 0.75f * heightFlow;
            float3 pMid = float3(p.x, 0.0f, p.z) * 0.80f
                        + float3(0.59f + tMid * 0.9f,
                                 0.0f,
                                 2.44f + tMid * 0.6f);
            float2 cMid = TexCurl2D(pMid);
            p.x += cMid.x * 0.04f * curlAmp;
            p.z += cMid.y * 0.04f * curlAmp;
        }

        // High Frequency Band (skipped at medium range).
        if (distLOD < 0.25f)
        {
            float tHigh  = flowTime * 1.80f * heightFlow;
            float3 pHigh = float3(p.x, 0.0f, p.z) * 1.60f
                         + float3(5.33f + tHigh * 1.2f,
                                  0.0f,
                                  1.88f + tHigh * 0.8f);
            float2 cHigh = TexCurl2D(pHigh);
            p.x += cHigh.x * 0.015f * curlAmp;
            p.z += cHigh.y * 0.015f * curlAmp;
        }
    }

    // FBM evaluation.
    // p_shape: curl-deformed position minus advection — for coarse octaves (0-2).
    // p_shape_stable: pre-curl position minus advection — for fine octaves (3-4).
    // The advection (windOfs + evoOfs) is kept separate and added identically
    // to every octave so all scales translate at the same rate (no flickering
    // from differential octave advection speeds).
    //
    // Distance LOD optimization: at medium+ range use the 3-octave FBMAlto3
    // which saves 2 PerlinNoise3D calls per sample. Fine octaves (3-4) are
    // already LOD-faded to near-zero weight at distLOD >= 0.4, so switching
    // to FBMAlto3 removes negligible-weight work at no visual cost.
    float dens;
    {
        // Wind+evo advection in FBM space, with FbmScale baked into the time
        // integral on the CPU. This prevents the visible time-lapse on AltoFbmScale
        // changes between presets: applying the *current* FbmScale to already-
        // accumulated WindAccumOffset retroactively rescales all past wind motion
        // in FBM space, which on a 60s+ accumulator looks like the cloud field
        // sweeping fast through the noise during the transition.
        // Evo direction is perpendicular to wind so EvolutionSpeed transitions
        // do not add to perceived wind-direction speed.
        float3 windAdvect = float3(WindDirection.x, 0.0f, WindDirection.y) * WindAccumOffsetScaled;
        float3 evoAdvect  = float3(-WindDirection.y, 0.0f, WindDirection.x) * EvoAccumOffsetScaled;
        float3 p_advect   = windAdvect + evoAdvect;

        // p_shape rebuilt from skyPos + curl-only delta so wind+evo terms cancel
        // exactly regardless of FbmScale changes (instead of via p*FbmScale-p_advect
        // which leaves a (windOfs+evoOfs)*FbmScale residual whenever the new
        // FbmScale-scaled accumulator and the old (per-frame FbmScale*accum) diverge).
        float3 curlDelta = p - p_before_curl;
        float3 p_shape   = (skyPos * baseScale + curlDelta) * ap.FbmScale;
        if (distLOD < 0.4f)
        {
            float3 p_shape_stable = skyPos * baseScale * ap.FbmScale;
            dens = FBMAlto5(p_shape, p_shape_stable, p_advect, ap.FbmLac, ap.FbmGain,
                            ap.BillowStr, distLOD);
        }
        else
        {
            dens = FBMAlto3(p_shape, p_advect, ap.FbmLac, ap.FbmGain,
                            ap.BillowStr, distLOD);
        }
    }

    // Dual-scale Worley cellular erosion.
        // Use the continuous 9-cell Worley at all distances.
        //
        // The previous far-distance 1-cell shortcut sampled only the current cell's
        // feature point. That approximation is not continuous across cell borders:
        // as clouds move, the nearest-point distance jumps when floor(p) changes.
        // In motion this shows up as strong moire / watery shimmer, especially when
        // FBM gain and coverage variation are low and the cellular term becomes more
        // visible. The full 3x3 neighborhood keeps the distance field continuous.
    {
        float2 wPos  = float2(p.x, p.z) * 2.032f;
            float worleyA = TexWorley2D(wPos * 0.40f);
        float invWA  = 1.0f - saturate(worleyA * 1.3f);
        dens = saturate(Remap(dens, -(invWA * 0.30f), 1.0f, 0.0f, 1.0f));

        // worleyB: only meaningful at close range (wBStrength fades to 0 at distLOD=1).
        // Skip the sample entirely when contribution is negligible.
        float wBStrength = lerp(0.15f, 0.0f, distLOD);
        if (wBStrength > 0.001f)
        {
                float worleyB = TexWorley2D_B(wPos * 0.75f);
            float invWB = 1.0f - saturate(worleyB * 1.4f);
            dens = saturate(Remap(dens, -(invWB * wBStrength), 1.0f, 0.0f, 1.0f));
        }
    }

    // Coverage threshold
    float clusterNoise = TexPerlin3D(float3(p.x * 0.18f, 0.0f, p.z * 0.18f));

    if (abs(ap.ZenithBias) > 0.001f)
    {
        densityShift += clusterNoise * abs(ap.ZenithBias) * 0.35f;
    }

    float effectiveAmount = saturate(ap.CloudAmount + densityShift);
    float covThresh = saturate(1.0f - effectiveAmount);

    {
        float clusterStrength = covThresh * 0.30f;
        covThresh = saturate(covThresh - clusterNoise * clusterStrength);
    }

    float evoBias = -ap.ZenithBias * biasFactor;
    // Anti-aliasing floor for the coverage soft-threshold.
    //
    // Very small CovSoftWidth values make the coverage gate nearly binary.
    // In the half-res cloud RT each texel covers 2x2 full-res pixels; a
    // step-function density edge means adjacent RT texels are either fully
    // cloud or fully empty — the bilateral upsampler cannot create a smooth
    // transition from that, so moving edges look like shifting pixel-blocks.
    //
    // covSoftMin ensures there is always a visible density gradient at the
    // cloud silhouette wide enough that the 3x3 bilateral upsampler has
    // real alpha signal to blend, rather than hard 0/1 transitions.
    //
    // Absorption scaling: at high absorption Beer-Lambert turns even a
    // density of ~0.002 into near-full opacity, so the visually soft zone
    // spans only a tiny fraction of the coverageGate gradient. Widening
    // covSoftMin proportionally restores a visible feathered edge width.
        // Previous 5x absorption boost over-softened the visible cloud contour and
        // made the entire layer look smeared / underwater. Keep a smaller floor:
        // enough to avoid binary edges in half-res, but not so wide that the cloud
        // silhouette turns into a large blurry halo.
        float absEdgeFactor = lerp(1.0f, 2.25f, saturate((Absorption - 0.5f) * 0.25f));
        float covSoftMin = (0.014f
                         + distLOD2 * 0.012f
                         + saturate(ap.EvolutionSpd * 0.20f) * 0.010f) * absEdgeFactor;
    float covSoft = max(ap.CovSoftWidth, covSoftMin) * clamp(exp2(evoBias * 0.5f), 0.3f, 3.0f);
    {
        float sizeFactor = saturate(ap.CloudSize - 0.5f);
        covSoft *= lerp(1.0f, 2.5f, distLOD2 * sizeFactor);
    }

    // Evolution / pulsing — skip at far distance where the subtle threshold
    // oscillation is invisible (saves 1 ValueNoise3D).
    if (ap.EvolutionSpd > 0.001f && distLOD < 0.6f)
    {
        float spatialMask = sin(TexValue3D(p_before_curl * 0.4f) * 6.2832f);
        // FlowAccumOffset * 0.25 = integral(EvSpd * dt * 0.04) — pre-integrated
        // equivalent of CloudTime * ap.EvolutionSpd * 0.04. Using CloudTime directly
        // causes the swell argument to decrease when EvolutionSpd lerps to a lower
        // value (CloudTime is large, so the product shrinks fast) — sin() then
        // oscillates backwards, visually pulling cloud coverage in reverse.
        float swellWave   = sin(FlowAccumOffset * 0.25f);
        float swellAmp    = 0.08f * saturate(ap.EvolutionSpd * 0.5f + 0.1f);
        covThresh = saturate(covThresh - (spatialMask * swellWave) * swellAmp);
    }

    // Distance-based horizon thinning + cluster grouping
    {
        float sizeFactor = saturate((ap.CloudSize - 0.5f) / 1.0f);

        float horizThin = distLOD * sizeFactor * 0.28f;
        covThresh = saturate(covThresh + horizThin);

        // Cluster grouping: skip at far distance where the tiny offset
        // (±0.05) is invisible — saves 1 ValueNoise3D.
        float clusterAmt = distLOD * sizeFactor * 0.22f;
        if (clusterAmt > 0.001f && distLOD < 0.6f)
        {
            float cn = TexValue3D(float3(p.x * 0.22f, 0.0f, p.z * 0.22f));
            covThresh = saturate(covThresh - cn * clusterAmt);
        }
    }

    // Wind-directional formation / dissolution — skip at far distance where
    // leading/trailing edge offsets are invisible (saves 2 ValueNoise3D).
    if (ap.EvolutionSpd > 0.001f && distLOD < 0.5f)
    {
        float evoStr = saturate(ap.EvolutionSpd * 2.0f);

        float3 windDir3 = float3(WindDirection.x, 0.0f, WindDirection.y);
        float  scaleG   = 0.18f;

        float3 pFwd    = p + windDir3 * 1.8f;
        float  fwdPot  = TexValue3D(float3(pFwd.x * scaleG, 0.0f, pFwd.z * scaleG)) - 0.5f;
        float formStr  = saturate(fwdPot) * evoStr * covThresh * 0.28f;
        covThresh = saturate(covThresh - formStr);

        float3 pBwd    = p - windDir3 * 3.2f;
        float  bwdPot  = TexValue3D(float3(pBwd.x * scaleG, 0.0f, pBwd.z * scaleG)) - 0.5f;
        float frayStr  = saturate(-bwdPot) * evoStr * covThresh * 0.22f;
        covThresh = saturate(covThresh + frayStr);

        float isolationPenalty = saturate(-clusterNoise - 0.1f) * evoStr * 0.35f;
        covThresh = saturate(covThresh + isolationPenalty * covThresh);
    }

    dens *= smoothstep(covThresh, covThresh + covSoft, dens);

    // Organic bottom shaping — at far distance use only coarse noise
    // (saves 1 ValueNoise3D at distLOD >= 0.5).
    if (ap.BottomSoft > 0.001f)
    {
        float3 basePos    = skyPos + (windOfs + evoOfs) / baseScale;
        float3 coarsePos  = basePos * 0.000045f;
        float coarseNoise = TexValue3D(float3(coarsePos.x,  0.0f, coarsePos.z));
        float detailNoise;
        if (distLOD < 0.5f)
        {
            float3 detailPos = basePos * 0.00012f;
            detailNoise = TexValue3D(float3(detailPos.x,  0.0f, detailPos.z));
        }
        else
        {
            detailNoise = coarseNoise; // reuse coarse at distance
        }

        float combinedNoise = saturate(coarseNoise * 0.65f + detailNoise * 0.35f);

        float depthBase  = 0.05f;
        float depthRange = ap.BottomSoft * 0.35f;
        float threshold  = depthBase + combinedNoise * depthRange;

        float fadeRange  = lerp(0.03f, 0.30f, ap.BottomSoft);
        float bottomFade = smoothstep(threshold, threshold + fadeRange, heightFrac);
        dens *= bottomFade;
    }

    // Crown fade
    // Start earlier and finish earlier than the old 0.65->1.0 ramp so the rounded
    // top blends into the side silhouette instead of creating a visible "cap seam".
    // These bounds match the existing AltocumulusMid HeightGradient top envelope.
    dens *= 1.0f - smoothstep(0.58f, 0.90f, heightFrac);

    return dens;
}

// ===========================================================================
// Lightweight Alto density for dissolving CloudMorph sources.
//
// During morph transitions the source preset is dissolving — its fine detail
// is increasingly masked by the dissolve function and contributes nothing
// visible. This simplified version cuts ~55% of per-sample noise cost:
//   - Only Low curl band (skips Mid+High = saves 8 ValueNoise3D)
//   - 3-octave FBM via FBMAlto3 (saves 2 PerlinNoise3D)
//   - Single Worley cell only (skips worleyB = saves 9 distance calcs)
//   - Skips evolution pulsing, wind-directional formation, cluster grouping
//   - Skips bottom shaping detail noise (uses only coarse noise)
//
// The result is visually identical to the full path for clouds that are
// 25%+ dissolved, and very close for the initial 0-25% dissolve ramp
// (where srcLOD == distLOD and the LOD boost hasn't kicked in yet,
// meaning the full version is also already degrading quality via LOD).
// ===========================================================================

float EvalAltoDensityCoreLite(float3 skyPos, float heightFrac, float skyH,
                              float distLOD, float distLOD2, AltoDensityParams ap)
{
    // --- Cheap early-outs (same as full path) ---
    if (AltoHorizonWidth > 0.001f && CloudIsBleedPass < 0.001f &&
        skyH < (AltoHorizonWidth * 0.90f - 0.09f))
        return 0.0f;

    if (DriftOutProgress > 0.001f && DriftOutFactor(skyPos.xz) < 0.001f)
        return 0.0f;

    // --- Sky-height redistribution (identical to full path) ---
    float horizonEdge = saturate(HorizonFade * 0.10f + DistanceFade * 0.06f);
    static const float zenithEdge = 0.65f;
    float activeRange = max(zenithEdge - horizonEdge, 0.05f);
    float biasFactor;
    {
        float t = saturate((skyH - horizonEdge) / activeRange);
        float s = t * t * (3.0f - 2.0f * t);
        biasFactor = s * 2.0f - 1.0f;
    }
    float densityShift = -ap.ZenithBias * biasFactor * 0.4f;

    float baseScale = 0.001f * ap.CloudSize;

    // --- Global advection ---
    float3 windOfs = float3(WindDirection.x, 0.0f, WindDirection.y) * WindSpeed;
    // Evolution drift is perpendicular to wind so it does not add to perceived
    // wind-direction speed during EvolutionSpeed transitions. See EvalAltoDensityCore.
    float3 evoDir = float3(-WindDirection.y, 0.0f, WindDirection.x);
    float3 evoOfs = evoDir * EvoAccumOffset;

    float3 p = skyPos * baseScale + windOfs + evoOfs;

    // Save pre-curl position so the FBM block can reconstruct curl-only delta
    // (needed for the FbmScale-aware p_advect refactor; see EvalAltoDensityCore).
    float3 p_before_curl = p;

    // --- Curl noise: LOW BAND ONLY (skip Mid and High entirely) ---
    // Saves 8 ValueNoise3D calls (4 per skipped band).
    if (CurlWarpStrength > 0.001f)
    {
        float flowTime = FlowAccumOffset;  // Pre-integrated; see EvalAltoDensityCore comment.
        float heightFlow = lerp(1.0f, 0.65f, saturate(heightFrac));
        float2 windBias = WindDirection * flowTime * 0.03f;
        float curlDamp = lerp(0.85f, 0.40f, saturate(ap.EvolutionSpd * 0.25f));
        float curlAmp = curlDamp * CurlWarpStrength;

        float tLow  = flowTime * 0.35f * heightFlow;
        float3 pLow = float3(p.x, 0.0f, p.z) * 0.41f
                    + float3(3.17f + windBias.x + tLow * 0.7f,
                             0.0f,
                             7.63f + windBias.y + tLow * 0.5f);
        float2 cLow = TexCurl2D(pLow);
        p.x += cLow.x * 0.11f * curlAmp;
        p.z += cLow.y * 0.11f * curlAmp;
    }

    // --- FBMAlto3 (3 octaves instead of 5) ---
    float dens;
    {
        // Pre-integrated FbmScale-aware advection (see EvalAltoDensityCore for rationale).
        float3 windAdvect = float3(WindDirection.x, 0.0f, WindDirection.y) * WindAccumOffsetScaled;
        float3 evoAdvect  = float3(-WindDirection.y, 0.0f, WindDirection.x) * EvoAccumOffsetScaled;
        float3 p_advect   = windAdvect + evoAdvect;

        float3 curlDelta = p - p_before_curl;
        float3 p_shape   = (skyPos * baseScale + curlDelta) * ap.FbmScale;
        dens = FBMAlto3(p_shape, p_advect, ap.FbmLac, ap.FbmGain,
                        ap.BillowStr, distLOD);
    }

    // --- Single Worley cell only (skip worleyB entirely) ---
    {
        float2 wPos  = float2(p.x, p.z) * 2.032f;
        float worleyA = TexWorley2D(wPos * 0.40f);
        float invWA  = 1.0f - saturate(worleyA * 1.3f);
        dens = saturate(Remap(dens, -(invWA * 0.30f), 1.0f, 0.0f, 1.0f));
    }

    // --- Coverage threshold (simplified: no evolution, no wind-directional, no cluster grouping) ---
    float clusterNoise = TexPerlin3D(float3(p.x * 0.18f, 0.0f, p.z * 0.18f));

    if (abs(ap.ZenithBias) > 0.001f)
        densityShift += clusterNoise * abs(ap.ZenithBias) * 0.35f;

    float effectiveAmount = saturate(ap.CloudAmount + densityShift);
    float covThresh = saturate(1.0f - effectiveAmount);
    {
        float clusterStrength = covThresh * 0.30f;
        covThresh = saturate(covThresh - clusterNoise * clusterStrength);
    }

    float evoBias = -ap.ZenithBias * biasFactor;
    float absEdgeFactor = lerp(1.0f, 2.25f, saturate((Absorption - 0.5f) * 0.25f));
    float covSoftMin = (0.014f
                     + distLOD2 * 0.012f
                     + saturate(ap.EvolutionSpd * 0.20f) * 0.010f) * absEdgeFactor;
    float covSoft = max(ap.CovSoftWidth, covSoftMin) * clamp(exp2(evoBias * 0.5f), 0.3f, 3.0f);
    {
        float sizeFactor = saturate(ap.CloudSize - 0.5f);
        covSoft *= lerp(1.0f, 2.5f, distLOD2 * sizeFactor);
    }

    // Skip evolution pulsing (dissolving clouds don't need formation cycling).
    // Skip distance-based horizon thinning / cluster grouping (minor visual effect).
    // Skip wind-directional formation / dissolution (minor visual effect).

    dens *= smoothstep(covThresh, covThresh + covSoft, dens);

    // --- Bottom shaping (coarse only, skip detail noise) ---
    if (ap.BottomSoft > 0.001f)
    {
        float3 basePos    = skyPos + (windOfs + evoOfs) / baseScale;
        float3 coarsePos  = basePos * 0.000045f;
        float coarseNoise = TexValue3D(float3(coarsePos.x, 0.0f, coarsePos.z));

        float depthBase  = 0.05f;
        float depthRange = ap.BottomSoft * 0.35f;
        float threshold  = depthBase + coarseNoise * depthRange;

        float fadeRange  = lerp(0.03f, 0.30f, ap.BottomSoft);
        float bottomFade = smoothstep(threshold, threshold + fadeRange, heightFrac);
        dens *= bottomFade;
    }

    // Crown fade
    dens *= 1.0f - smoothstep(0.58f, 0.90f, heightFrac);

    return dens;
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
    // AltocumulusMid density evaluation via parameterized helper.
    // Supports CloudMorph dual-density transitions: when MorphActive > 0.5
    // the shader evaluates density with BOTH target and source params,
    // applies formation to target / dissolve to source, and combines.
    // ===================================================================
    if (CloudType == 1)
    {
        // --- Fill target (main) params from current CB ---
        AltoDensityParams mainParams;
        mainParams.CloudSize    = AltoCloudSize;
        mainParams.CloudAmount  = AltoCloudAmount;
        mainParams.BillowStr    = AltoBillowStrength;
        mainParams.CovSoftWidth = AltoCovSoftWidth;
        mainParams.FbmLac       = AltoFbmLacunarity;
        mainParams.FbmGain      = AltoFbmGain;
        mainParams.FbmScale     = AltoFbmScale;
        mainParams.BottomSoft   = AltoBottomSoftness;
        mainParams.ZenithBias   = AltoZenithBias;
        mainParams.EvolutionSpd = EvolutionSpeed;

        float dens = EvalAltoDensityCore(skyPos, heightFrac, skyH, distLOD, distLOD2, mainParams);

        // --- CloudMorph dual-density path ---
        if (MorphActive > 0.5f)
        {
            float tgtDens = dens;

            // Evaluate source preset density at the same sky position.
            AltoDensityParams srcParams;
            srcParams.CloudSize    = MorphSrcCloudSize;
            srcParams.CloudAmount  = MorphSrcCloudAmount;
            srcParams.BillowStr    = MorphSrcBillowStr;
            srcParams.CovSoftWidth = MorphSrcCovSoftWidth;
            srcParams.FbmLac       = MorphSrcFbmLac;
            srcParams.FbmGain      = MorphSrcFbmGain;
            srcParams.FbmScale     = AltoFbmScale;  // no morph-src slot; use same scale for both
            srcParams.BottomSoft   = MorphSrcBottomSoft;
            srcParams.ZenithBias   = MorphSrcZenithBias;
            srcParams.EvolutionSpd = MorphSrcEvolutionSpd;

            // Source density uses the same full EvalAltoDensityCore as the
            // pre-transition frame. EvalAltoDensityCoreLite produces a different
            // cloud shape (different curl deformation, fewer FBM octaves) which
            // causes a visible jump whenever the switch would occur — regardless
            // of threshold. Organic morph requires identical evaluation throughout.
            float srcDens = EvalAltoDensityCore(skyPos, heightFrac, skyH, distLOD, distLOD2, srcParams);

            // Cache DissolveMask once — used by both dissolve and formation.
            // Saves 4 PerlinNoise3D calls per sample (2 per DissolveMask invocation).
            float dMask = DissolveMask(skyPos);
            srcDens = ApplyDissolveWithMask(srcDens, dMask, DissolvePhase);
            tgtDens = ApplyFormationWithMask(tgtDens, dMask, FormationPhase);

            // Combine: where source had cloud → dissolving, where target has cloud → forming.
            dens = srcDens + tgtDens;

            if (dens <= 0.0001f)
                return 0.0f;
        }
        else
        {
            // --- Non-morph path: dissolve/formation ---
            if (dens <= 0.0001f)
                return 0.0f;

            dens = ApplyDissolve(dens, skyPos, DissolvePhase);
            if (dens <= 0.0001f)
                return 0.0f;

            dens = ApplyFormation(dens, skyPos, FormationPhase);
            if (dens <= 0.0001f)
                return 0.0f;
        }

        // AltoHorizonWidth zenith cap (shared by morph and non-morph).
        if (AltoHorizonWidth > 0.001f && CloudIsBleedPass < 0.001f)
        {
            float altoCapEdge = AltoHorizonWidth * 0.90f;
            dens *= smoothstep(altoCapEdge - 0.08f, altoCapEdge + 0.08f, skyH);
        }

        dens *= DriftOutFactor(skyPos.xz);

        return dens;
    }

    // --- Coverage / Weather noise ---
    // Large-scale weather map controls where clouds exist.
    float2 weatherUV = skyPos.xz * WeatherScale 
                      + WindDirection * WindSpeed * 0.3f;
    float weatherNoise = TexValue3D(float3(weatherUV, 0.0f));

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
    float  bNoise  = TexValue3D(float3(hgUV.x,		hgUV.y,		7.3f));
    float  tNoise  = TexValue3D(float3(hgUV.x + 5.7f, hgUV.y + 3.2f, 13.1f));
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
    float  driftX	= TexValue3D(float3(driftUV.x,		 driftUV.y,		 heightFrac * 2.1f + 0.73f)) - 0.5f;
    float  driftZ	= TexValue3D(float3(driftUV.x + 4.83f, driftUV.y + 2.31f, heightFrac * 2.1f		)) - 0.5f;
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
    float oct0Shape = 0.5f * TexValue3D(shapePos);

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
        float phaseNoise = TexValue3D(float3(skyPos.x, shapeY, skyPos.z) * effectiveShapeScale * 0.25f);
        // Pre-integrated form: EvoAccumOffset == integral(EvSpd * dt * 0.05);
        // multiplier 0.8 = 0.04/0.05 to preserve original swell rate.
        float swellPhase = EvoAccumOffset * 0.8f + phaseNoise * 6.2832f;

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
    float baseEdgeWidth = lerp(0.35f, 0.50f, absEdgeWiden);
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
        // Pre-integrated form: EvoAccumOffset == integral(EvSpd * dt * 0.05);
        // original used CloudTime * EvSpd (factor 1.0), so multiply by 20 = 1.0/0.05
        // to keep the historical detail-drift rate identical at steady state.
        // Direction is perpendicular to wind so EvolutionSpeed transitions do
        // not add to perceived wind-direction speed (see EvalAltoDensityCore).
        float3 detailPos = float3(skyPos.x * DetailScale * noiseScale.x + driftXZ.x * 0.45f,
                                  shapeY   * DetailScale * noiseScale.y,
                                  skyPos.z * DetailScale * noiseScale.z + driftXZ.y * 0.45f)
                          + float3(-WindDirection.y, 0.0f, WindDirection.x) 
                            * EvoAccumOffset * 20.0f;
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

    // --- Transform-preset dissolve / formation / drift-out ---
    finalDensity = ApplyDissolve(finalDensity, skyPos, DissolvePhase);
    if (finalDensity <= 0.0001f)
        return 0.0f;

    finalDensity = ApplyFormation(finalDensity, skyPos, FormationPhase);
    if (finalDensity <= 0.0001f)
        return 0.0f;

    finalDensity *= DriftOutFactor(skyPos.xz);

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
        float lh = HeightFraction(lightPos.y, CamPositionWS.y - CloudBottomHeight, CloudThickness);

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
// Simple blue-noise-like jitter anchored to the cloud field
// ===========================================================================

float ScreenJitter(float2 cloudPos, float jitterScale)
{
    // 2D hash (no directional correlation) in cloud space.
    // Screen-space jitter leaves a fixed grain pattern behind moving cloud
    // edges, which reads as a blurry residue that slowly dissolves. Cloud-space
    // jitter advects with the cloud body instead, so any grain moves with the
    // wind/evolution rather than remaining stuck to the screen.
    float2 hp = frac(cloudPos * float2(443.897f, 441.423f));
    hp += dot(hp, hp.yx + 19.19f);
    float rawJitter = frac(hp.x * hp.y);

    float jitterThickness = (CloudType == 1) ? AltoThickness : CloudThickness;
    float jitterAbsorption = (CloudType == 1) ? AltoAbsorption : Absorption;
    float thickBandGuard = saturate((jitterThickness - 2600.0f) / 2200.0f);
    float jitterMin	  = lerp(0.35f, 0.85f, thickBandGuard);
    float jitterAbsDamp  = lerp(jitterMin, 1.0f, saturate((jitterAbsorption - 0.2f) * 2.5f));

    // Base phase: sample at the center of each step interval by default.
    // This minimizes visible march planes compared to edge-aligned sampling.
    //
    // We deliberately do NOT alternate basePhase per FrameIndex even when
    // temporal is enabled.  Edge pixels (alpha in [TemporalAlphaLow, High])
    // bypass the checkerboard reuse and do a fresh raymarch every frame; if
    // basePhase swung between 0.25 and 0.75 frame-to-frame, those edge pixels
    // would sample at different sub-step positions each frame, producing
    // alpha oscillation that EMA blending only partially hides — visible as
    // shimmer at thin altocumulus borders.  A fixed 0.5 phase combined with
    // the per-pixel spatialJitter below keeps march planes randomized in
    // space while remaining temporally stable.
    float basePhase = 0.5f;

    // Spatial component: symmetric around the base phase instead of only pushing
    // samples forward. That avoids biasing the whole ray toward one side of the
    // step interval and keeps JitterStrength=0 perfectly clean.
    float spatialJitter = (rawJitter - 0.5f) * JitterStrength * jitterAbsDamp * jitterScale;

    return frac(basePhase + spatialJitter + 1.0f);
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

    // Adaptive step count: thick Altocumulus layers and grazing view angles are
    // the worst case for visible stacked march planes. Their cloud shaping is
    // largely anchored to a reference thickness, so allowing step size to grow
    // linearly with AltoThickness under-samples the same visual features and
    // turns them into horizontal bands.
    float thickStepBoost   = saturate((effThickness - 1800.0f) / 2200.0f);
    float grazingStepBoost = saturate((0.35f - abs(rayDir.y)) / 0.30f);
    float altoStepBoost    = (CloudType == 1) ? max(thickStepBoost, grazingStepBoost) : 0.0f;
    float refThickness     = (CloudType == 1) ? min(effThickness, 1800.0f) : effThickness;
    float targetStepWorld  = lerp(refThickness / 32.0f, refThickness / 56.0f, altoStepBoost);
    float minStepsF	  = ceil(maxDist / max(targetStepWorld, 1.0f));

    // AltocumulusMid now uses absolute caps independent of PrimaryStepCount.
    // Low/Medium are already much cheaper via lower render resolution and
    // reduced feature set; tying the cap to PrimaryStepCount reintroduced the
    // same horizontal slab banding at lower qualities even after High was fixed.
    int   baseStepCap      = (CloudType == 1) ? 128 : min(PrimaryStepCount * 4, 64);
    int   boostedStepCap   = (CloudType == 1) ? 192 : min(PrimaryStepCount * 6, 96);
    int   stepCap          = (CloudType == 1)
                           ? (int)lerp((float)baseStepCap, (float)boostedStepCap, altoStepBoost)
                           : baseStepCap;
    int   effectiveSteps = clamp((int)max((float)PrimaryStepCount, minStepsF), 1, stepCap);
    // Low-absorption boost: when absorption is very small, each sample carries
    // little extinction and stochastic sampling noise is more visible.
    // Increase effective steps in that regime to average out point noise.
    float lowAbsStepBoost = lerp(2.0f, 1.0f, saturate((effAbsorption - 0.2f) * 2.5f));
    effectiveSteps		= clamp((int)ceil((float)effectiveSteps * lowAbsStepBoost), 1, stepCap);

    // CloudMorph step reduction: during morph transitions, each step evaluates
    // density TWICE (target + source). A 35% step reduction was used previously
    // to keep per-pixel cost closer to non-morph levels. That is REMOVED here
    // because the larger step size made horizontal march planes plainly visible
    // (read as "stacked cloud slabs") at low/medium quality. Keeping full step
    // count during morph eliminates that banding; the temporary cost increase
    // is acceptable because morphs are short-lived transition events.
    // (Reduction kept only at very high step counts where the gain is negligible
    //  and the visual cost is invisible.)
    if (CloudType == 1 && MorphActive > 0.5f && PrimaryStepCount >= 24)
    {
        int morphSteps = max((int)((float)effectiveSteps * 0.85f), PrimaryStepCount);
        effectiveSteps = min(effectiveSteps, morphSteps);
    }

    float stepSize	   = maxDist / (float)effectiveSteps;
    float thickBandGuard = saturate((effThickness - 2600.0f) / 2200.0f);

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

    // Anchor jitter to the cloud-field entry position instead of the screen.
    // This keeps the pattern moving with the cloud rather than staying fixed
    // in view-space and smearing behind moving silhouettes.
    float2 entrySkyXZ   = rayDir.xz * tRange.x;
    // Pre-integrated form: replaces EvolutionSpeed * CloudTime * 0.05 with
    // EvoAccumOffset (== integral(EvSpd * dt * 0.05)) so the jitter anchor
    // does not jump time-lapse-fast when EvolutionSpeed changes between presets.
    // The EvolutionSpeed * WindSpeed * 0.15 term is constant-per-frame (no time)
    // so it is safe to keep using the current EvolutionSpeed value.
    float2 cloudAdvection = WindDirection * (WindSpeed + EvoAccumOffset + EvolutionSpeed * WindSpeed * 0.15f);
    float2 jitterCloudPos = entrySkyXZ * 0.001f + cloudAdvection;

    // Dampen jitter where it is most visible as shimmer: high-FBM Alto clouds
    // and distant cloud regions. These areas already contain plenty of visual
    // detail, so aggressive stochastic offsets read as glitter/noise instead of
    // useful anti-banding.
    float horizDistEntry = length(entrySkyXZ);
    float lodNearJ = max(CloudBottomHeight * 1.0f, 1.0f);
    float lodFarJ  = max(CloudBottomHeight * 6.0f, lodNearJ + 1.0f);
    float jitterDistLOD = saturate((horizDistEntry - lodNearJ) / (lodFarJ - lodNearJ));
    float jitterDistDamp = lerp(1.0f, 0.22f, jitterDistLOD * jitterDistLOD);
    float jitterFbmDamp = 1.0f;
    if (CloudType == 1)
    {
        // At high AltoFbmGain the silhouette already contains plenty of internal
        // variation. Additional stochastic start jitter mostly reads as shimmer,
        // including near the zenith where distance-based damping is weak.
        jitterFbmDamp = lerp(1.0f, 0.20f, saturate((AltoFbmGain - 0.30f) * 2.0f));
    }
    float jitterScale = jitterDistDamp * jitterFbmDamp;

    // Per-pixel start jitter: uniform [0,1] (see ScreenJitter comments).
    float jitter = ScreenJitter(jitterCloudPos, jitterScale);
    float t = tRange.x + stepSize * jitter;

    // Secondary cloud-space hash for per-step sub-jitter decorrelation.
    // Different transform from ScreenJitter to avoid start/sub correlation.
    float2 hp2 = frac((jitterCloudPos + float2(1.3f, 2.7f)) * float2(317.113f, 271.197f));
    hp2 += dot(hp2, hp2.yx + 27.17f);
    float rawJitter2 = frac(hp2.x * hp2.y);

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
        // For Alto: use AltoJitterAbsCap directly as a virtual absorption for jitter
        // amplitude — completely decoupled from actual AltoAbsorption so the user can
        // suppress shimmer without softening the cloud density/shading.
        // For other cloud types: keep using actual absorption so banding-suppression
        // still scales correctly.
        float jitterAbs    = (CloudType == 1) ? AltoJitterAbsCap : effAbsorption;
        float subJitterAmp = lerp(subJitterMin, 0.7f, saturate((jitterAbs - 0.2f) * 2.5f))
                           * jitterScale;

        // The start-jitter is enough to break up march planes. The additional
        // per-step sub-jitter is what tends to read as glitter/noisy crawling
        // on Alto silhouettes, especially when FBM gain is high or the cloud is
        // far away. Keep only a small fraction of it in those cases.
        if (CloudType == 1)
        {
            float subFbmDamp  = lerp(0.28f, 0.04f, saturate((AltoFbmGain - 0.30f) * 2.2f));
            float subDistDamp = lerp(1.0f, 0.18f, jitterDistLOD * jitterDistLOD);
            subJitterAmp *= subFbmDamp * subDistDamp;
        }
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
                    float thinEdge = 1.0f - smoothstep(0.02f, 0.18f, sampleOptDepth);
                    float heightSoft = smoothstep(0.08f, 0.82f, heightFrac);
                    float heightBlend = saturate(lerp(heightSoft, 1.0f, thinEdge * 0.55f));
                    float3 cloudColor = lerp(AltoCloudColorDark, AltoCloudColor, heightBlend);
                    float  heightIllum = lerp(0.72f, 1.0f, heightSoft);
                    heightIllum = lerp(heightIllum, 1.0f, thinEdge * 0.35f);

                    // Sun-elevation modulation for Altocumulus:
                    float altoSunFade = saturate(CloudSunElevation * 6.0f + 0.5f);
                    altoSunFade = altoSunFade * altoSunFade;
                    // Moonlight: restore direct/silver/forward at night (light dir/color
                    // already blended to moon on CPU). Keep sunFade for warmth tint only.
                    float altoLightFade = max(altoSunFade, CloudMoonLightFactor);
                    float altoTwilightBoost = saturate(1.0f - abs(CloudSunElevation) * 8.0f) * CloudTwilightAmbient;
                    float altoNightBase = lerp(CloudNightAmbient, 0.0f, saturate(CloudSunElevation * 4.0f));
                    float altoDirectFactor = AltoCloudBrightness * heightIllum * altoLightFade * CloudSunLightIntensity;
                    float altoAmbientFactor = (altoTwilightBoost + altoNightBase) * max(CloudAmbientIntensity, 0.001f);

                    // Silverlining: top-edge brightening toward the light source.
                    float altoSilver  = CloudSilverliningStrength * 0.25f * heightSoft * altoLightFade * CloudSunLightIntensity;
                    // Forward scatter: broad light-aligned hazy glow boost.
                    float altoForward = CloudForwardScatterStrength * 0.12f * altoLightFade * CloudSunLightIntensity;
                    // Sun warmth: tint cloud illumination toward golden tone at low sun angles.
                    // Only active during day (altoSunFade), not during moonlight.
                    float3 altoWarmTint = float3(1.0f, 0.88f, 0.65f);
                    float3 altoLitColor = lerp(CloudLightColor, CloudLightColor * altoWarmTint, CloudSunWarmthInfluence * altoSunFade);

                    sampleLight = altoLitColor * cloudColor
                        * (altoDirectFactor + altoAmbientFactor + altoSilver + altoForward);

                    // Moon warm corona: brighten and warm the angular (phase-dependent)
                    // components near the moon direction. cosTheta (dot(ray, lightDir))
                    // is already computed once before the march loop. At night
                    // CloudLightDirection = moon direction (CPU-blended), so this
                    // concentrates the warm yellowish glow in clouds directly facing the moon.
                    // Tint delta: R+22% G+7% B-22% → shifts cool blue-grey → warm yellow.
                    if (CloudMoonLightFactor > 0.001f)
                    {
                        float moonWarmGate      = pow(max(cosTheta, 0.0f), 3.0f) * CloudMoonLightFactor;
                        float3 altoAngularLight = altoLitColor * cloudColor
                            * (altoDirectFactor + altoSilver + altoForward);
                        sampleLight += altoAngularLight * float3(0.22f, 0.07f, -0.22f) * moonWarmGate;
                    }

                    // === Sunset underside illumination (AltocumulusMid) ===
                    // Warm sunset glow on cloud undersides when the sun is near/below the horizon.
                    // CPU pre-computes: SunsetUndersideColor (elevation-dependent gradient),
                    // SunsetUndersideIntensity (activation envelope × user intensity).
                    if (SunsetUndersideIntensity > 0.001f)
                    {
                        // Underside factor: strongest at cloud base, fades toward top.
                        float undersideFactor = pow(saturate(1.0f - heightFrac), SunsetUndersideHeightFade);

                        // Sun-direction factor: glow is concentrated toward the sun's azimuth.
                        // Use XZ (horizontal) components to ignore elevation — the sun is at/below
                        // the horizon, so the lit region is on the horizon side of the cloud.
                        float2 rayDirXZ = normalize(rayDir.xz + float2(0.0001f, 0.0001f));
                        float2 sunDirXZ = normalize(CloudLightDirection.xz + float2(0.0001f, 0.0001f));
                        float  sunDot   = dot(rayDirXZ, sunDirXZ);
                        // Remap [-1,1] → [0,1] with spread control: higher spread = wider glow.
                        float  sunGlow  = saturate((sunDot + 1.0f) * 0.5f);
                        sunGlow = pow(sunGlow, SunsetUndersideSpread);

                        // Thin edges receive less sunset glow (they can't hold much light).
                        float sunsetDensityGate = saturate(extinction * 8.0f);

                        // Combine and add as warm illumination.
                        float3 sunsetContrib = SunsetUndersideColor * SunsetUndersideIntensity
                            * undersideFactor * sunGlow * sunsetDensityGate
                            * AltoCloudBrightness;
                        sampleLight += sunsetContrib * cloudColor;
                    }

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
                            float  flashCoverage = saturate(Remap(TexValue3D(float3(flashWeatherUV, 0.0f)),
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
                                float  boltCoverage = saturate(Remap(TexValue3D(float3(boltWeatherUV, 0.0f)),
                                    1.0f - Coverage, 1.0f, 0.0f, 1.0f));
                                float boltDist = length(samplePos - boltPos);
                                float boltGlow = pow(saturate(boltRadius / max(boltDist, 0.001f)), 2.2f);
                                float boltFrac = frac(boltTime + boltOffset);
                                float boltPulse = exp(-boltFrac * 6.0f);
                                lightningContrib += LightningBoltColor * boltGlow * boltPulse
                                    * LightningGlowIntensity * boltCoverage;
                            }
                        }

                        sampleLight += lightningContrib;
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
                    // Moonlight: restore direct/silver/forward at night (light dir/color
                    // already blended to moon on CPU). Keep sunFade for warmth tint only.
                    float lightFade = max(sunFade, CloudMoonLightFactor);
                    float twilightBoost = saturate(1.0f - abs(CloudSunElevation) * 8.0f) * CloudTwilightAmbient;
                    float nightAmbientBase = lerp(CloudNightAmbient, 0.0f, saturate(CloudSunElevation * 4.0f));
                    float effectiveAmbient = ambient * (1.0f + edgeColorHold) + twilightBoost + nightAmbientBase;

                    // Beer-Lambert absorption exponent on shadow transmittance.
                    float absLightT = pow(max(lightT, 0.001f), CloudLightAbsorption);
                    // Direct light: HG phase boosted by forward-scatter strength.
                    float directSun = absLightT * phase * CloudForwardScatterStrength * lightFade * CloudSunLightIntensity;
                    // Silverlining: additive phase-squared forward glow (edge brightening toward light source).
                    float silverGlow = phase * phase * lightFade * CloudSilverliningStrength * 0.3f * CloudSunLightIntensity;
                    // Ambient scaled by CloudAmbientIntensity.
                    float ambLight = effectiveAmbient * max(CloudAmbientIntensity, 0.001f);
                    // Sun warmth: tint light toward warm golden tone.
                    // Only active during day (sunFade), not during moonlight.
                    float3 stdWarmTint = float3(1.0f, 0.88f, 0.65f);
                    float3 stdLitColor = lerp(CloudLightColor, CloudLightColor * stdWarmTint, CloudSunWarmthInfluence * sunFade);
                    sampleLight = stdLitColor * (directSun + silverGlow + ambLight);

                    // Moon warm corona: same as the alto path above.
                    if (CloudMoonLightFactor > 0.001f)
                    {
                        float moonWarmGate      = pow(max(cosTheta, 0.0f), 3.0f) * CloudMoonLightFactor;
                        float3 stdAngularLight  = stdLitColor * (directSun + silverGlow);
                        sampleLight += stdAngularLight * float3(0.22f, 0.07f, -0.22f) * moonWarmGate;
                    }

                    // === Sunset underside illumination (standard clouds) ===
                    if (SunsetUndersideIntensity > 0.001f)
                    {
                        float undersideFactor = pow(saturate(1.0f - heightFrac), SunsetUndersideHeightFade);
                        float2 rayDirXZ = normalize(rayDir.xz + float2(0.0001f, 0.0001f));
                        float2 sunDirXZ = normalize(CloudLightDirection.xz + float2(0.0001f, 0.0001f));
                        float  sunDot   = dot(rayDirXZ, sunDirXZ);
                        float  sunGlow  = saturate((sunDot + 1.0f) * 0.5f);
                        sunGlow = pow(sunGlow, SunsetUndersideSpread);
                        float sunsetDensityGate = saturate(extinction * 8.0f);
                        float3 sunsetContrib = SunsetUndersideColor * SunsetUndersideIntensity
                            * undersideFactor * sunGlow * sunsetDensityGate;
                        sampleLight += sunsetContrib;
                    }
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
        float wn = TexValue3D(float3(wUV, 0.0f));
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

    // Soft fade band from 0 deg to ~17 deg above the horizon.
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
    // --- Full temporal reprojection ---
    // Instead of same-UV checkerboard reuse, reproject the previous frame's
    // history to the current camera orientation.  Clouds are at infinite
    // distance (sky dome) so only camera rotation causes UV motion; the
    // reprojection projects a sky-distance point through PrevViewProjection.
    //
    // Checkerboard skip: 50% of pixels per frame.  Skipped pixels return
    // reprojected history directly.  Active pixels do a full raymarch and
    // then EMA-blend with reprojected history for noise reduction.
    //
    // TemporalEnabled == 2: warmup complete, reprojection active.
    // Values 0 and 1 always do a full raymarch (0=disabled, 1=warmup).
    float4 reprojectedHistory = float4(0.0f, 0.0f, 0.0f, 0.0f);
    bool   hasValidHistory    = false;

    if (TemporalEnabled == 2)
    {
        // Reproject current pixel's sky direction through previous frame's VP.
        float3 worldDir = GetViewRayDir(input.UV);
        float3 skyPoint = CamPositionWS.xyz + worldDir * 1000000.0f;
        float4 prevClip = mul(float4(skyPoint, 1.0f), PrevViewProjection);

        if (prevClip.w > 0.001f)
        {
            float2 prevNDC = prevClip.xy / prevClip.w;
            float2 prevUV  = prevNDC * float2(0.5f, -0.5f) + 0.5f;

            if (all(prevUV > 0.002f) && all(prevUV < 0.998f))
            {
                reprojectedHistory = CloudTexture.Sample(LinearSamp, prevUV);
                hasValidHistory = true;

                // Checkerboard skip: every other pixel reuses reprojected history.
                int2 px   = (int2)input.Position.xy;
                bool skip = (((px.x + px.y) + (int)FrameIndex) & 1) != 0;

                // Lightning override: during an active flash or bolt, disable the
                // checkerboard skip to prevent a 1x1 dither pattern on lightning.
                if (skip && LightningEnabled != 0 && CloudType == 1)
                {
                    float flashCycleCheck = floor(CloudTime * LightningInternalSpeed);
                    float flashRandCheck  = frac(sin(flashCycleCheck * 127.1f + 311.7f) * 43758.5453f);
                    if (flashRandCheck < LightningInternalFreq)
                    {
                        skip = false;
                    }
                    else
                    {
                        float boltTimeCheck = CloudTime * LightningSpeed;
                        [unroll]
                        for (int bci = 0; bci < 3; bci++)
                        {
                            float boltOffC = (float)bci * 0.33333f;
                            float boltCycC = floor(boltTimeCheck + boltOffC);
                            float boltRndC = frac(sin(boltCycC * (311.7f + (float)bci * 83.5f) + 127.1f) * 43758.5453f);
                            if (boltRndC < LightningStrikeFreq)
                            {
                                float boltFrcC = frac(boltTimeCheck + boltOffC);
                                if (exp(-boltFrcC * 6.0f) > 0.05f)
                                    skip = false;
                            }
                        }
                    }
                }

                if (skip)
                {
                    // Stability check: reuse only clear sky or solid cloud interior.
                    // Mid-alpha silhouette band is forced to fresh raymarch because
                    // cloud edges shift between frames from wind/evolution advection
                    // (which reprojection does not correct — it only handles camera motion).
                    bool stable = (reprojectedHistory.a <= TemporalAlphaLow ||
                                   reprojectedHistory.a >= TemporalAlphaHigh);
                    if (stable)
                        return reprojectedHistory;
                }
            }
        }
    }

    float3 rayOrigin = CamPositionWS.xyz;
    float3 rayDir    = GetViewRayDir(input.UV);

    // In TEN Y-down: upward rays have rayDir.y < 0.
    // Discard downward-looking rays (into the ground).
    if (rayDir.y > 0.02f)
        return float4(0.0f, 0.0f, 0.0f, 0.0f);

    float4 cloudResult = RaymarchClouds(rayOrigin, rayDir, input.Position.xy);

    // --- EMA temporal accumulation ---
    // Blend fresh raymarch with reprojected history for noise reduction.
    // TemporalBlendFactor (0.05) controls convergence speed: each frame,
    // 5% of the pixel comes from the fresh raymarch, 95% from accumulated
    // history. Over ~20 frames the result converges to the true value.
    // Lightning is an impulsive effect and already baked into cloudResult;
    // the EMA naturally fades it over the accumulation window which matches
    // the visual duration of a lightning flash.
    if (hasValidHistory)
        cloudResult = lerp(reprojectedHistory, cloudResult, TemporalBlendFactor);

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
    // At far range (low elevation), the threshold rises slightly to handle
    // the larger boundary noise from distant grazing-angle rays.
    // NOTE: do NOT raise this threshold aggressively (e.g. to 0.18) —
    // cutting the sub-0.18 gradient destroys the soft feathered zone that
    // the bilateral upsampler needs to anti-alias the half-res cloud edge.
    {
        float elevation  = saturate(-rayDir.y);
        float distFactor = saturate(1.0f - elevation * 5.0f);
        float thinThresh = lerp(0.03f, 0.10f, distFactor);
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

    // 5x5 bilateral upsampling filter.
    // Quarter-res rendering (4x downscale per axis) needs a larger kernel to
    // smooth out the coarser texel grid.  The 5x5 (25-tap) cross-bilateral
    // filter still runs at full-res screen pixels but covers 5 cloud texels
    // in each direction, producing smooth silhouettes even at 0.25x scale.
    // At higher resolution scales (0.5, 0.75) the outer taps contribute less
    // (spatialSigma2 naturally attenuates them) so quality degrades gracefully.
    const float spatialSigma2   = UpsampleSpatialSigma2; // tunable via debug menu (scaled for 5x5)
    const float alphaSigma2RGB  = 0.1568f; // 2 * 0.28^2 (RGB bilateral sigma = 0.28)
    const float alphaSigma2Edge = 0.50f; // 2 * 0.50^2 (alpha bilateral sigma = 0.50)

    float3 accumRGBPremul = float3(0.0f, 0.0f, 0.0f);
    float  accumAlpha	 = 0.0f;
    float  accumWeightRGB = 0.0f;
    float  accumWeightA   = 0.0f;

    [unroll]
    for (int oy = -2; oy <= 2; oy++)
    {
        [unroll]
        for (int ox = -2; ox <= 2; ox++)
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
    // The sun/moon disc is NO LONGER in this background — it is rendered in a
    // separate additive pass after cloud compositing. This means screen blend
    // works correctly (bg is never near 1.0 from the sun) and no special
    // sun-disc masking is needed.
    // .a contains any previous cloud layer's coverage (for dual-layer accumulation).
    float4 bgFull = SceneBackgroundTex.Sample(LinearSamp, uv);
    float3 bg = bgFull.rgb;

    // Cloud presence (cloud exists here).
    float cloudPresence = saturate(cloudAlpha * 10.0f);

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

    // Two-stage threshold modulation: day → sunset → (back to sliders at night).
    //
    // DAY   (elev > 0.15):  sunsetFactor=0 → effectiveHigh = BlendThresholdHigh (slider).
    //                        Only bright/thin cloud wisps screen blend; dense bodies alpha-blend.
    //
    // SUNSET (elev ≈ 0.0):  sunsetFactor=1 → shifts toward kSunset* constants.
    //                        sfSuppression=0 kills screen blend; sun occlusion via alphaForBg.
    //
    // NIGHT  (elev < -0.15): sunsetFactor=0 again → effectiveHigh = BlendThresholdHigh (slider).
    //                        Sliders are fully active. The alphaOcclude boost is suppressed via
    //                        nightThreshFade so no hard cutout silhouette forms on a black sky.
    const float kSunsetStart = 0.15f;
    const float kNightStart  = 0.15f;
    const float kSunsetHigh  = 0.12f;
    const float kSunsetWidth = 0.400f;
    const float kSunsetLow   = 0.150f;
    float sunsetRise   = saturate(1.0f - CloudSunElevation / kSunsetStart);  // 0→1 as elev 0.15→0
    float nightReturn  = saturate(-CloudSunElevation / kNightStart);          // 0→1 as elev 0→-kNight
    float sunsetFactor = sunsetRise * (1.0f - nightReturn);
    // Smooth night factor (used to suppress alphaOcclude boost, not to override thresholds).
    float nightThreshFade = nightReturn * nightReturn * (3.0f - 2.0f * nightReturn);
    // Thresholds blend day→sunset, then back to slider values at night (sunsetFactor returns to 0).
    float effectiveHigh  = lerp(BlendThresholdHigh,      kSunsetHigh,  sunsetFactor);
    float effectiveWidth = lerp(BlendThresholdHighWidth,  kSunsetWidth, sunsetFactor);
    float effectiveLow   = lerp(BlendThresholdLow,        kSunsetLow,   sunsetFactor);

    // Bright side: transition width comes from the per-preset BlendThresholdHighWidth
    // (runtime-blended toward the sunset target above).
    // Dark side: use a small fixed transition (0.025).
    const float darkTransW = 0.025f;
    float brightScreen = smoothstep(effectiveHigh - effectiveWidth,
                                    effectiveHigh + effectiveWidth, cloudLuma);
    // Dark-screen zone is only for transparent edges (near-zero alpha with residual dark color
    // from premultiplied filtering). Alto second color produces solid dark faces (high alpha,
    // low luma) — those must alpha-blend to properly occlude the background sun disc.
    // Gate darkScreen by the inverse of cloud presence: 1 at fully transparent, 0 at solid.
    float darkEdgeWeight = 1.0f - saturate(cloudAlpha * 10.0f);
    float darkScreen     = (1.0f - smoothstep(effectiveLow - darkTransW,
                                              effectiveLow + darkTransW, cloudLuma))
                           * darkEdgeWeight;
    float screenFactor = max(brightScreen, darkScreen);

    // Alpha blend result.
    //
    // At day (sunsetFactor=0): cloudAlpha is used directly for alphaResult.
    //   Thin screen-blend clouds (low cloudAlpha) stay semi-transparent → sun visible ✓
    //   Dense alpha-blend clouds (high cloudAlpha, screenFactor≈0) are boosted via the
    //   gentle curve 1-(1-α)² → sun hidden ✓
    //
    // At sunset (sunsetFactor>0): cloudAlpha is blended toward cloudPresence.
    //   cloudPresence = sat(cloudAlpha*10) — already 1.0 for any pixel with α≥0.1,
    //   the same binary "cloud exists here" signal used by the master presence gate below.
    //   This makes ANY cloud (even thin screen-blend ones) fully opaque in alpha-blend
    //   so the sun disc stored in bg is properly hidden.
    //
    // effectiveSunset = sqrt(sunsetFactor) — power curve that makes the ramp approach 1.0
    //   faster than linear.  At elevation=0.07 (sunsetFactor≈0.72): sqrt(0.72)≈0.849,
    //   giving alphaForBg≈0.96 for a cloud with cloudAlpha=0.15.
    //
    // effectiveScreenFactor uses (1-effectiveSunset)² so the screen-blend contribution
    //   naturally fades toward sunset.
    // cloudPresence already declared above.
    float effectiveSunset   = sqrt(sunsetFactor);
    // brightBgBoost removed: the sun disc is no longer in bg (handled by the separate
    // PSSunMoonDisc additive pass), so there is no bright bg to compensate for here.
    float alphaForBg        = lerp(cloudAlpha, cloudPresence, effectiveSunset);
    // Opacity boost (1-(1-α)²) strengthens occlusion for dense clouds during the day.
    // At night (nightThreshFade=1) this boost is suppressed completely — there is no sun
    // disc to occlude and the boost would create a hard dark silhouette on the black sky.
    float boostWeight       = (1.0f - screenFactor) * (1.0f - nightThreshFade);
    float alphaOcclude      = lerp(alphaForBg,
                                   1.0f - (1.0f - alphaForBg) * (1.0f - alphaForBg),
                                   boostWeight);
    float3 alphaResult      = lerp(bg, cloudStraight, alphaOcclude);

    float sfSuppression     = (1.0f - effectiveSunset) * (1.0f - effectiveSunset);
    float effectiveScreenFactor = screenFactor * sfSuppression;

    // Final hybrid composite.
    float3 finalColor = lerp(alphaResult, screenResult, effectiveScreenFactor);

    // Where there is no cloud at all, preserve the background exactly.
    // cloudAlpha acts as the master presence signal.
    // The sun disc is no longer in bg, so no special bright-bg gate needed.
    finalColor = lerp(bg, finalColor, saturate(cloudAlpha * 10.0f));

    // Alto dark-edge screen-blend override: applied AFTER all composite blend logic
    // (luma classifier, sunset factor, master gate) so it is fully independent
    // of every blend setting.
    //
    // Targets ONLY the dark/grey boundary halos:
    //   altoEdgeAlpha — tight boundary zone: full at alpha=0, zero at alpha=0.12.
    //                   Keeps the effect away from the visible cloud body.
    //   altoEdgeDark  — luma gate: 1.0 for dark pixels (luma=0), 0.0 at luma>=0.5.
    //                   Bright cloud edges are already handled correctly by the
    //                   normal luma classifier; only dark/grey outlines need forcing.
    //
    // Combined: only dark boundary pixels get screen-blended. Dense or bright
    // cloud pixels (altoEdgeAlpha=0 or altoEdgeDark=0) are completely unaffected.
    if (CloudType == 1)
    {
        float altoEdgeAlpha = 1.0f - smoothstep(0.0f, 0.92f, cloudAlpha);
        float altoEdgeDark  = 1.0f - saturate(cloudLuma / 0.5f);
        float altoAbsEdge   = altoEdgeAlpha * altoEdgeDark;
        finalColor = lerp(finalColor, screenResult, altoAbsEdge);
    }

    // Output cloud coverage in alpha channel so the sun/moon disc pass can
    // mask out the disc where clouds are present.
    // Use cloudAlpha directly (not cloudPresence) so thin cloud edges
    // (cloudAlpha ≈ 0.1) don't aggressively suppress the sun disc.
    // cloudPresence = sat(cloudAlpha*10) would give 1.0 at cloudAlpha=0.1,
    // fully blocking the sun at semi-transparent edges → dark outline.
    // Accumulate with previous layer's coverage via max().
    float combinedCoverage = max(bgFull.a, cloudAlpha);
    return float4(finalColor, combinedCoverage);
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

    [loop]
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
    // 20 pixels at typical half-res (960x540) ≈ 40 full-res pixels from sun center.
    // Larger radius ensures that clouds surrounding a small gap at the sun position
    // are still captured, preventing sprite/flare bleed through cloud patches.
    // ---------------------------------------------------------------------------
    static const float CORONA_RADIUS = 20.0f;

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
        // occAggressiveness scales the cloud alpha before subtracting from 1.
        // Day (sunsetFactor=0):    multiplier=1  → matches raw transmittance.
        // Sunset (sunsetFactor=1): multiplier=8  → cloudAlpha≥0.125 yields full occlusion.
        // This makes the sun sprite / lens flare halo / starflare disappear behind
        // screen-blend clouds (moderate alpha in cloud RT) at sunset, exactly mirroring
        // the sunDiscVis logic in GodRay.hlsl (same 0.25-elevation ramp, same multiplier).
        float occSunsetFactor   = saturate(1.0f - CloudSunElevation / 0.25f);
        float occAggressiveness = lerp(1.0f, 8.0f, occSunsetFactor);
        float screenVisibility  = saturate(1.0f - cloudAlpha * occAggressiveness);

        // Use the most-occluding estimate from either method.
        visibility = min(rayVisibility, screenVisibility);
    }

    return float4(visibility, 0.0f, 0.0f, 1.0f);
}
