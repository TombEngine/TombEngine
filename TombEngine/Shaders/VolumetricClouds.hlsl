// VolumetricClouds.hlsl — Bounded-volume procedural volumetric cloud renderer.
//
// Architecture:
//   - Rendered as a fullscreen pass AFTER the sky bitmap, BEFORE world geometry.
//   - Raymarches through a spherical-shell cloud volume around the planet.
//   - Uses purely procedural noise (no 3D textures) for maximum portability.
//   - Outputs RGBA: RGB = lit cloud color, A = cloud opacity.
//   - A separate entry point provides lens flare occlusion transmittance.
//
// Shader entry points:
//   VS              — shared fullscreen-triangle vertex shader
//   PSClouds        — main cloud rendering pixel shader
//   PSCloudOcclusion— lens flare occlusion evaluation (single-pixel)
//   PSCloudComposite— upsamples half-res cloud result and composites over scene

#include "./CBCamera.hlsli"
#include "./CBVolumetricCloud.hlsli"
#include "./Math.hlsli"

// ---------------------------------------------------------------------------
// Samplers (reuse existing engine samplers)
// ---------------------------------------------------------------------------

Texture2D SceneColorTexture : register(t0);  // Background sky color (for composite)
Texture2D CloudTexture      : register(t1);  // Half-res cloud RGBA (for upscale)
Texture2D DepthTexture      : register(t2);  // Scene depth (for composite masking)
SamplerState PointSamp      : register(s1);  // Point sampler
SamplerState LinearSamp     : register(s2);  // Linear sampler

// ---------------------------------------------------------------------------
// Vertex shader — fullscreen triangle (reads from vertex buffer like PostProcess)
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

VSOutput VS(VSInput input)
{
	VSOutput output;

	output.Position     = float4(input.Position, 1.0f);
	output.UV           = input.UV;
	output.PositionCopy = output.Position;

	return output;
}

// ===========================================================================
// Noise functions — purely procedural, no texture lookups
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

// Low-frequency FBM (3 octaves) — cloud shape.
// Persistence reduced to 0.38 so higher octaves barely affect the silhouette;
// only the first (dominant) octave drives large stable cloud masses.
//
// lod [0,1]: distance-based LOD for Moiré prevention.
//   When a cloud sample is far from the camera, the step size of the primary
//   ray march becomes comparable to the wavelength of the finest octave.
//   That sub-step-size noise aliases into shimmering interference bands.
//   At lod=0 (near): all three octaves at full weight.
//   At lod=0.5:      oct=2 (finest, 5.62x freq) fully silent.
//   At lod=1.0:      oct=1 (2.37x freq) also silent; only the first
//                    (coarsest) octave drives cloud shapes.
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
		if      (oct == 0) octWeight = 1.0f;
		else if (oct == 1) octWeight = saturate(1.0f - lod);
		else               octWeight = saturate(1.0f - lod * 2.0f);
		v += a * octWeight * ValueNoise3D(s);
		s *= 2.37f;  // Lacunarity (non-power-of-2 avoids tiling artifacts)
		a *= 0.38f;  // Low persistence: each octave contributes much less than the previous
	}
	return v;
}

// High-frequency FBM (2 octaves) — detail erosion.
// Persistence reduced to 0.35 (was 0.5).
//
// lod [0,1]: same distance LOD as FBMLowFreq but more aggressive.
//   Detail noise runs at a much finer scale than shape noise, so it aliases
//   even at shorter distances. The second (finest) octave is suppressed by
//   lod=0.5; the first detail octave fades fully by lod=1.
//   At medium viewing distances (lod≈0.4) both octaves are already partially
//   muted, removing the high-contrast detail bands that produce Moiré.
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
// CloudType 0 (None):                  generic cumulus fallback
// CloudType 1 (CirrusHigh):            thin concentrated band in upper 30%
// CloudType 2 (AltocumulusMid):        rounded cumulus, slight bottom bias
// CloudType 3 (StratocumulusLow):      broad flat slab, gradual top fade
// CloudType 4 (CumulonimbusVertical):  tall anvil shape, dense through most of height
float HeightGradient(float heightFrac)
{
	if (CloudType == 1) // CirrusHigh
	{
		// Thin wispy band concentrated in the upper portion of the slab.
		// Peaks at 75-85% height, fades quickly below and above.
		float bottom = smoothstep(0.5f, 0.75f, heightFrac);
		float top    = 1.0f - smoothstep(0.85f, 1.0f, heightFrac);
		return bottom * top;
	}
	else if (CloudType == 2) // AltocumulusMid
	{
		// Classic cumulus profile — rounded bottom entry, broad middle, gentle top fade.
		float bottom = smoothstep(0.0f, 0.2f, heightFrac);
		float top    = 1.0f - smoothstep(0.65f, 1.0f, heightFrac);
		return bottom * top;
	}
	else if (CloudType == 3) // StratocumulusLow
	{
		// Broad flat slab — density is nearly constant through the middle,
		// with thin ramps at bottom and top.
		float bottom = smoothstep(0.0f, 0.08f, heightFrac);
		float top    = 1.0f - smoothstep(0.8f, 1.0f, heightFrac);
		return bottom * top;
	}
	else if (CloudType == 4) // CumulonimbusVertical
	{
		// Towering anvil: dense from bottom through 80% of height,
		// then a slower fade near top to simulate the spreading anvil cap.
		float bottom = smoothstep(0.0f, 0.1f, heightFrac);
		float top    = 1.0f - smoothstep(0.8f, 1.0f, heightFrac) * 0.6f; // never fully zero at top
		return bottom * top;
	}
	else // None / default — original generic profile
	{
		float bottom = smoothstep(0.0f, 0.15f, heightFrac);
		float top    = 1.0f - smoothstep(0.7f, 1.0f, heightFrac);
		return bottom * top;
	}
}

// ===========================================================================
// Cloud density sampling
// ===========================================================================

float CloudDensityAtWorldPos(float3 worldPos, float heightFrac, bool useDetail)
{
	// --- Sky-space coordinates for stable cloud anchoring ---
	// The cloud slab follows the camera, so worldPos (= CamPos + rayDir*t)
	// shifts with camera movement.  Subtracting the camera position gives
	// a position that depends only on the ray direction and intersection
	// distance, making the noise pattern behave like an infinitely distant
	// sky dome — stable during camera translation.
	float3 skyPos = worldPos - CamPositionWS.xyz;

	// --- Coverage / Weather noise ---
	// Large-scale weather map controls where clouds exist.
	float2 weatherUV = skyPos.xz * WeatherScale 
	                  + WindDirection * CloudTime * WindSpeed * 0.3f;
	float weatherNoise = ValueNoise3D(float3(weatherUV, 0.0f));

	// Remap weather noise with coverage parameter.
	// Higher coverage -> more clouds, but always with variation.
	float coverageMask = Remap(weatherNoise, 1.0f - Coverage, 1.0f, 0.0f, 1.0f);
	coverageMask = pow(coverageMask, lerp(1.5f, 0.7f, Coverage));

	if (coverageMask <= 0.001f)
		return 0.0f;

	// --- Height gradient ---
	float hGrad = HeightGradient(heightFrac);

	// --- Distance LOD for Moiré / aliasing prevention ---
	// The step size of the primary ray march is fixed at CloudThickness*6 / PrimaryStepCount.
	// When a sample is far from the camera, the finest noise octaves have wavelengths
	// smaller than that step size → they alias into shimmering interference bands (Moiré).
	//
	// Metric: horizontal (XZ-plane) distance from camera to sample.
	//   At CloudBottomHeight * 1.5 (≈56° elevation) → lod = 0, full quality.
	//   At CloudBottomHeight * 6.0 (≈ 9° elevation)  → lod = 1, fine octaves silent.
	// CloudBottomHeight is the camera-to-cloud-base vertical distance, naturally
	// scaling with scene size without needing separate tuning.
	float horizDist = length(skyPos.xz);
	float lodNear   = max(CloudBottomHeight * 1.5f, 1.0f);
	float lodFar    = max(CloudBottomHeight * 6.0f, lodNear + 1.0f);
	float distLOD   = saturate((horizDist - lodNear) / (lodFar - lodNear));

	// --- Base shape noise ---
	// Sampling position is purely driven by horizontal wind drift.
	// No Y-displacement here — any 3D shift of shapePos produces twisting artifacts
	// because the noise has structure in all three axes.
	//
	// Per-CloudType noise distortion:
	//   CirrusHigh:   horizontal stretch (3x XZ, 0.4x Y) for wispy fibrous streaks.
	//   AltocumulusMid: slight XZ stretch (1.3x) for patchy roundish puffs.
	//   StratocumulusLow: strong XZ stretch (2x, 0.6x Y) for flat sheet-like slabs.
	//   CumulonimbusVertical: vertical stretch (0.7x XZ, 1.5x Y) for tall towers.
	//   Default: isotropic (no distortion).
	float3 noiseScale;
	if (CloudType == 1) // CirrusHigh — fibrous horizontal streaks
		noiseScale = float3(3.0f, 0.4f, 3.0f);
	else if (CloudType == 2) // AltocumulusMid — slightly elongated patches
		noiseScale = float3(1.3f, 0.9f, 1.3f);
	else if (CloudType == 3) // StratocumulusLow — flat sheet
		noiseScale = float3(2.0f, 0.6f, 2.0f);
	else if (CloudType == 4) // CumulonimbusVertical — tall vertical towers
		noiseScale = float3(0.7f, 1.5f, 0.7f);
	else
		noiseScale = float3(1.0f, 1.0f, 1.0f);

	float3 shapePos = skyPos * ShapeScale * noiseScale
	                 + float3(WindDirection.x, 0.0f, WindDirection.y) 
	                   * CloudTime * WindSpeed;

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
	float remapLow = 0.22f;
	if (EvolutionSpeed > 0.001f)
	{
		// Phase noise sampled at much coarser scale than shape — one puff region
		// covers many cloud masses, giving a coherent swell without fragmentation.
		float phaseNoise = ValueNoise3D(skyPos * ShapeScale * 0.25f);
		float swellPhase = CloudTime * EvolutionSpeed * 0.04f + phaseNoise * 6.2832f;

		// Max threshold shift: ±0.26 at EvolutionSpeed=1, ±0.42 at EvolutionSpeed=5.
		// Clamped so clouds never fully vanish or fully merge.
		// Scaled down at distance: far cloud regions have a much smaller sweep range.
		// This is critical — the billowing sweep moves shapeDensity through the
		// S-curve's steep zone (0.3–0.7) repeatedly, creating bright/dark banding
		// at medium/far distance. Reducing swellAmp there avoids that oscillation.
		float swellAmp = 0.26f * saturate(EvolutionSpeed * 0.2f + 0.1f);
		swellAmp *= (1.0f - distLOD * 0.85f);  // Near: full swell. Far: ~15% of normal.
		remapLow = clamp(0.22f - sin(swellPhase) * swellAmp, 0.001f, 0.46f);
	}

	// --- Stable silhouette blend ---
	// The visible cloud BOUNDARY is defined exclusively by octave 0 — the
	// smoothest, lowest-frequency noise component. Higher FBM octaves add fine
	// interior structure but must never extend or retract the visible edge,
	// as that produces per-pixel crawling and noisy silhouettes during motion.
	//
	// oct0Density: where oct0 says "cloud exists". Smooth, stable boundary.
	// fullDensity: full multi-octave FBM. Rich interior detail.
	// edgeMask:    smoothstep ramp [0, 0.15] of oct0Density — controls how
	//             much higher-octave detail is permitted at each point.
	//
	// Near the boundary (edgeMask ≈ 0): density = oct0Density (smooth).
	// Deep inside (edgeMask = 1): density = fullDensity (full detail).
	// The wide transition band (15% of the remapped oct0 range) prevents any
	// visible seam where higher octaves suddenly appear.
	// During wind and billowing, the edge moves with oct0's smooth gradient,
	// not with noisy higher-octave features.
	float oct0Density  = Remap(oct0Shape, remapLow, 1.0f, 0.0f, 1.0f);
	float fullDensity  = Remap(baseShape, remapLow, 1.0f, 0.0f, 1.0f);

	// --- Absorption-widened silhouette zone ---
	// At the cloud boundary, oct0Density is very small (0.001–0.05 range).
	// At low absorption these tiny values produce negligible opacity per step.
	// At high absorption, exp(-d * A * step) turns them into visible per-pixel
	// speckle because even d=0.02 at Abs=5 gives ~10% opacity per step.
	//
	// Fix: widen the oct0-only (smooth) boundary zone proportionally to
	// absorption.  At high Abs the smooth-edge region extends deeper into
	// the cloud body before higher octaves are introduced.
	//   edgeWidth = 0.15 (default) → 0.35 (at Abs≥3.0)
	// This ensures the density gradient at the visible boundary is always
	// smooth and low-frequency, regardless of absorption setting.
	float absEdgeWiden = saturate((Absorption - 0.5f) * 0.4f);
	float edgeWidth    = lerp(0.15f, 0.35f, absEdgeWiden);
	float edgeMask     = smoothstep(0.0f, edgeWidth, oct0Density);
	float shapeDensity = lerp(oct0Density, fullDensity, edgeMask);

	// --- Distance- and absorption-softened S-curve ---
	// smoothstep(0,1,x) amplifies contrast (peak slope 1.5 at x=0.5).
	// At distance or high absorption, blend toward a linear ramp to prevent
	// the steep mid-range from creating banding during motion.
	{
		float absorpFade  = saturate((Absorption - 0.5f) * 0.4f);
		float sCurveFade  = max(distLOD, absorpFade);
		float ssValue     = smoothstep(0.0f, 1.0f, shapeDensity);
		shapeDensity      = lerp(ssValue, shapeDensity, sCurveFade);
	}

	shapeDensity *= coverageMask * hGrad;

	if (shapeDensity <= 0.0001f)
		return 0.0f;

	// --- Detail erosion (interior only) ---
	if (useDetail && DetailNoiseEnabled != 0)
	{
		// Apply same per-type noise distortion to detail sampling for consistency.
		// No Y-drift: vertical wobble was the primary source of edge warping.
		float3 detailPos = skyPos * DetailScale * noiseScale
		                  + float3(WindDirection.x, 0.0f, WindDirection.y) 
		                    * CloudTime * EvolutionSpeed;
		// Pass distLOD: at medium/far distance FBMDetail progressively mutes its
		// octaves, so detail returns 0 at lod=1 without needing a separate branch.
		float detail = FBMDetail(detailPos, distLOD);

		// Erosion is strongest INSIDE cloud bodies and near-zero at the silhouette.
		// This preserves stable edges while allowing internal billowing texture.
		//
		// Two guards control how far inward the erosion zone begins:
		//   (a) distLOD via erosionWeight scalar — already applied
		//   (b) Absorption — when Abs is high, even a small density value becomes
		//       opaque, so boundary noise that erosion carves into looks like hard
		//       holes.  We move the interior mask threshold upward with absorption
		//       so only genuinely dense core regions are ever eroded.
		//
		//   absorpEdgeGuard: 0 at Abs≤0.5, 1 at Abs≥3.0
		//   maskLow  moves from 0.35 (default) → 0.60 (high absorption)
		//   maskHigh moves from 0.65            → 0.85
		//
		// Additionally, total erosion depth scales down with absorption via
		// absorpErosionScale, preventing detail noise from carving visible holes
		// in cloud silhouettes that the Beer-Lambert curve makes 
		// disproportionately dark.
		float absorpEdgeGuard  = saturate((Absorption - 0.5f) * 0.4f);
		float maskLow          = lerp(0.35f, 0.60f, absorpEdgeGuard);
		float maskHigh         = lerp(0.65f, 0.85f, absorpEdgeGuard);
		float interiorMask     = smoothstep(maskLow, maskHigh, shapeDensity);
		float absorpErosionScale = saturate(1.0f - (Absorption - 0.5f) * 0.35f);

		// Per-CloudType erosion weight multiplier:
		//   CirrusHigh: minimal erosion — cirrus clouds are smooth/wispy.
		//   StratocumulusLow: reduced erosion — flat smooth sheets.
		//   CumulonimbusVertical: enhanced erosion — dramatic towering structures.
		//   Others: standard erosion.
		float typeErosionMul;
		if      (CloudType == 1) typeErosionMul = 0.15f; // CirrusHigh: almost no erosion
		else if (CloudType == 3) typeErosionMul = 0.6f;  // StratocumulusLow: mild
		else if (CloudType == 4) typeErosionMul = 1.4f;  // CumulonimbusVertical: enhanced
		else                     typeErosionMul = 1.0f;  // default

		float erosionWeight    = interiorMask * DetailStrength * 0.40f
		                         * (1.0f - distLOD) * absorpErosionScale * typeErosionMul;
		shapeDensity = Remap(shapeDensity, erosionWeight * detail, 1.0f, 0.0f, 1.0f);
	}

	// --- Absorption-proportional soft density floor ---
	// At high absorption, Beer-Lambert turns even tiny density values into
	// visible per-pixel opacity spots (dithering / speckle at cloud edges).
	// Example: d=0.02, Abs=5.0, stepSize=500 → extinction=50 → fully opaque
	// from what should be an imperceptible boundary wisp.
	//
	// Fix: apply a soft threshold that smoothly fades very low density
	// values to zero. The threshold scales with Absorption so the
	// "clean" zone around zero widens as absorption increases.
	//   At Abs=0.5:  minVisible ≈ 0.005 (barely any density is suppressed).
	//   At Abs=3.0+: minVisible ≈ 0.06  (wider band → no speckle at edges).
	// Above the threshold, density is untouched.
	float finalDensity = max(shapeDensity * CloudDensity, 0.0f);
	float minVisible   = lerp(0.005f, 0.06f, saturate((Absorption - 0.5f) * 0.4f));
	finalDensity      *= smoothstep(0.0f, minVisible, finalDensity);

	return finalDensity;
}

// ===========================================================================
// Cloud slab intersection — TEN uses Y-down coordinates
// ===========================================================================

// Intersect a ray with the flat horizontal cloud slab.
// In TEN: Y increases downward, so clouds ABOVE the camera have lower Y values.
//   Slab bottom face (nearest ground)  = CamPositionWS.y - CloudBottomHeight
//   Slab top face   (furthest from ground) = bottom - CloudThickness
// Returns (tEntry, tExit), or (-1, -1) on miss.
float2 IntersectCloudVolume(float3 rayOrigin, float3 rayDir)
{
	float slabBottom = CamPositionWS.y - CloudBottomHeight;
	float slabTop    = slabBottom - CloudThickness;

	// Horizontal rays never cross the horizontal slab.
	if (abs(rayDir.y) < 0.0001f)
		return float2(-1.0f, -1.0f);

	float t0 = (slabBottom - rayOrigin.y) / rayDir.y;
	float t1 = (slabTop    - rayOrigin.y) / rayDir.y;

	float tNear = min(t0, t1);
	float tFar  = max(t0, t1);

	// Miss: slab entirely behind camera, or degenerate interval.
	if (tFar < 0.0f || tNear >= tFar)
		return float2(-1.0f, -1.0f);

	return float2(max(tNear, 0.0f), tFar);
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
	//   CirrusHigh: ice crystals — very strong forward scattering (g=0.85),
	//               minimal backscatter. Creates bright halo/glare near sun.
	//   AltocumulusMid: standard water droplets — balanced dual-lobe.
	//   StratocumulusLow: water droplets — slightly more diffuse (broader lobe).
	//   CumulonimbusVertical: large mixed-phase drops — broad forward, some back.
	//   Default: use CB PhaseForward/PhaseBackward directly.

	float fwd, bk, fwdWeight;
	if (CloudType == 1) // CirrusHigh — ice crystal forward scattering
	{
		fwd       = 0.85f;
		bk        = 0.15f;
		fwdWeight = 0.9f;  // heavily forward-dominant
	}
	else if (CloudType == 3) // StratocumulusLow — diffuse water droplets
	{
		fwd       = 0.45f;
		bk        = 0.35f;
		fwdWeight = 0.6f;
	}
	else if (CloudType == 4) // CumulonimbusVertical — large mixed-phase
	{
		fwd       = 0.7f;
		bk        = 0.4f;
		fwdWeight = 0.65f;
	}
	else // AltocumulusMid, None, default — use CB parameters
	{
		fwd       = PhaseForward;
		bk        = PhaseBackward;
		fwdWeight = 0.7f;
	}

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
		float d = CloudDensityAtWorldPos(lightPos, lh, false);
		accumDensity += d * stepSize;
	}

	// Beer-Lambert.
	float transmittance = exp(-accumDensity * Absorption);

	// Powder / silver lining approximation:
	// Bright edge when looking toward light through thin cloud.
	//
	// The powder term darkens thin regions: 1 - exp(-d*A*2) is near-zero when
	// accumDensity is tiny.  multiplying transmittance (≈1) by a near-zero
	// powder makes the boundary near-black.  As noise makes density wobble
	// frame-to-frame, that black flickers bright↔dark.
	//
	// Fix: raise the powder floor proportionally with Absorption.
	//   At Abs=0.5 (low): floor = 0.25  (original value)
	//   At Abs=1.5:       floor = 0.42
	//   At Abs=3.0+:      floor = 0.65
	// High absorption must not make thin edges darker; it should make thick
	// interiors richer while leaving boundary lighting soft and stable.
	float powderFloor = lerp(0.25f, 0.65f, saturate((Absorption - 0.5f) * 0.4f));
	float powder = 1.0f - exp(-accumDensity * Absorption * 2.0f);
	powder = lerp(1.0f, max(powder, powderFloor), SilverliningStr);

	return transmittance * powder;
}

// ===========================================================================
// Simple blue-noise-like jitter from screen position + frame
// ===========================================================================

float ScreenJitter(float2 screenPos)
{
	// Interleaved gradient noise (Jimenez 2014).
	//
	// Per-frame shift REMOVED. Previously FrameIndex shifted the IGN pattern
	// each frame to break up banding, but without TAA to accumulate frames the
	// effect is the opposite: every frame independently samples a different
	// slice of the detail noise field. In thin/medium-density layers this makes
	// the per-sample density jump between near-zero and non-zero every frame,
	// which manifests as the stripe-like shimmer.
	//
	// Static-per-pixel IGN still gives full spatial decorrelation between
	// neighboring pixels (no fixed banding) but the same pixel always starts
	// at the same sub-step offset → temporally stable density → no flicker.
	float3 magic = float3(0.06711056f, 0.00583715f, 52.9829189f);
	float jitter = frac(magic.z * frac(dot(screenPos, magic.xy)));
	return jitter * JitterStrength;
}

// ===========================================================================
// Main cloud raymarch
// ===========================================================================

float4 RaymarchClouds(float3 rayOrigin, float3 rayDir, float2 screenPos)
{
	// Intersect cloud volume.
	float2 tRange = IntersectCloudVolume(rayOrigin, rayDir);

	if (tRange.x < 0.0f)
		return float4(0.0f, 0.0f, 0.0f, 0.0f); // No intersection — fully transparent.

	// Clamp max march distance to avoid wasting steps on very long grazing rays.
	float maxDist = min(tRange.y - tRange.x, CloudThickness * 6.0f);
	float stepSize = maxDist / (float)PrimaryStepCount;

	// Jittered start offset to hide banding.
	float jitter = ScreenJitter(screenPos);
	float t = tRange.x + stepSize * jitter;

	// Phase function for light scattering.
	float cosTheta = dot(rayDir, normalize(CloudLightDirection));
	float phase = DualLobePhase(cosTheta);

	// Accumulation.
	float  transmittance = 1.0f;
	float3 scatteredLight = float3(0.0f, 0.0f, 0.0f);
	int    steps = 0;

	[loop]
	for (int step = 0; step < PrimaryStepCount; step++)
	{
		if (transmittance < 0.01f)
			break; // Early out — cloud is opaque.

		float3 samplePos = rayOrigin + rayDir * t;
		float  heightFrac = HeightFraction(samplePos.y,
		                    rayOrigin.y - CloudBottomHeight, CloudThickness);

		// Skip samples outside the cloud layer boundary.
		if (heightFrac >= 0.0f && heightFrac <= 1.0f)
		{
			// Sample density (use detail noise if available).
			bool useDetail = (DetailNoiseEnabled != 0);
			float density = CloudDensityAtWorldPos(samplePos, heightFrac, useDetail);

			if (density > 0.0001f)
			{
				// Lighting at this point.
				float lightT = LightTransmittance(samplePos, heightFrac);
				
				// Ambient term: increases with height to simulate sky scattering.
				float ambient = AmbientContrib * lerp(0.6f, 1.0f, heightFrac);

				// Total in-scattered light at this sample.
				float3 sampleLight = CloudLightColor * (lightT * phase + ambient);

				// Extinction for this step.
				float extinction = density * Absorption * stepSize;
				float sampleTransmittance = exp(-extinction);

				// Energy-conserving integration (Frostbite technique).
				float3 integScatter = sampleLight * (1.0f - sampleTransmittance) / max(density * Absorption, 0.0001f);
				scatteredLight += transmittance * integScatter * density * Absorption;
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
		float2 wUV = dbgSkyPos.xz * WeatherScale + WindDirection * CloudTime * WindSpeed * 0.3f;
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
	if (CloudDebugView == 6) // NoDetailNoise — re-march without detail
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
// In TEN's Y-down space: sky is -Y, horizon is Y ≈ 0.
//   elevation = -rayDir.y   (0 at horizon, 1 straight up)
//
// Distance and elevation are geometrically equivalent for a flat cloud slab:
//   tEntry ≈ CloudBottomHeight / elevation
// So a single elevation-based curve captures both the distance-fade and the
// horizon-haze fade simultaneously.
//
// Curve design:
//   elevation 0.00  (0°)   → 0%   (totally transparent — merges with horizon)
//   elevation 0.06  (~3°)  → 20%  (strongly faded — very distant fringe)
//   elevation 0.12  (~7°)  → 50%  (half strength — hazy transition region)
//   elevation 0.22  (~13°) → 85%  (nearly full — close-ish cloud masses)
//   elevation 0.30  (~17°) → 100% (fully opaque — nearby/overhead clouds)
//
// The sqrt push on the smoothstep result gives an exponential-feeling rolloff:
// opacity recovers quickly once a cloud is even a few degrees above the horizon,
// while the near-horizon region stays very faint and "airy".
float HorizonAtmosphericFade(float3 rayDir)
{
	// Elevation in [0,1]: 0 = horizontal, 1 = overhead.
	float elevation = saturate(-rayDir.y);

	// Soft fade band from 0° to ~17° above the horizon.
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
// PS — Main cloud rendering pixel shader
// ===========================================================================

float4 PS(VSOutput input) : SV_TARGET
{
	float3 rayOrigin = CamPositionWS.xyz;
	float3 rayDir    = GetViewRayDir(input.UV);

	// In TEN Y-down: upward rays have rayDir.y < 0.
	// Discard downward-looking rays (into the ground).
	if (rayDir.y > 0.02f)
		return float4(0.0f, 0.0f, 0.0f, 0.0f);

	float4 cloudResult = RaymarchClouds(rayOrigin, rayDir, input.Position.xy);

	// Debug views.
	if (CloudDebugView != 0)
		cloudResult = DebugVisualization(rayOrigin, rayDir, input.Position.xy, cloudResult, PrimaryStepCount);

	// Atmospheric horizon fade: attenuate cloud opacity for low-elevation rays.
	// Distant clouds smoothly dissolve into whatever sky/horizon is behind them.
	// Only applied when not in a debug view (debug views show unmodified density).
	if (CloudDebugView == 0)
	{
		cloudResult.a *= HorizonAtmosphericFade(rayDir);

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
	}

	return cloudResult;
}

// ===========================================================================
// PSCloudComposite — Upscale half-res clouds and alpha-blend over scene.
//
// The cloud render target (RGBA: lit cloud color + opacity) is bound to t0.
// Hardware alpha blending (AlphaBlend) composites this over the existing
// framebuffer content, which may contain:
//   - Legacy sky bitmap layer(s)
//   - Horizon mesh (possibly closing overhead)
//   - Starfield
//   - Black void where nothing was drawn
// The shader simply passes through the cloud RGBA unchanged.
// Where cloud alpha is 0 the existing content shows through; where alpha
// is 1 the cloud fully occludes whatever is behind it.
// ===========================================================================

float4 PSCloudComposite(VSOutput input) : SV_TARGET
{
	// Sample cloud result from half-res RT (bilinear upscale).
	return SceneColorTexture.Sample(LinearSamp, input.UV);
}

// ===========================================================================
// PSCloudOcclusion — Evaluate cloud transmittance along a single direction
// for lens flare occlusion. Outputs transmittance in the red channel.
// ===========================================================================

float4 PSCloudOcclusion(VSOutput input) : SV_TARGET
{
	float3 rayOrigin = CamPositionWS.xyz;
	float3 rayDir    = normalize(CloudLightDirection);

	// Intersect cloud volume.
	float2 tRange = IntersectCloudVolume(rayOrigin, rayDir);

	if (tRange.x < 0.0f)
		return float4(1.0f, 0.0f, 0.0f, 1.0f); // No clouds in path — fully visible.

	// Use a small number of samples for occlusion.
	int occSteps = max(PrimaryStepCount / 2, 4);
	float maxDist = min(tRange.y - tRange.x, CloudThickness * 4.0f);
	float stepSize = maxDist / (float)occSteps;
	float t = tRange.x;

	float transmittance = 1.0f;

	[loop]
	for (int occStep = 0; occStep < occSteps; occStep++)
	{
		if (transmittance < 0.01f)
			break;

		float3 samplePos = rayOrigin + rayDir * t;
		float heightFrac = HeightFraction(samplePos.y,
		                   rayOrigin.y - CloudBottomHeight, CloudThickness);

		if (heightFrac >= 0.0f && heightFrac <= 1.0f)
		{
			float density = CloudDensityAtWorldPos(samplePos, heightFrac, false);
			transmittance *= exp(-density * Absorption * stepSize);
		}

		t += stepSize;
	}

	return float4(transmittance, 0.0f, 0.0f, 1.0f);
}
