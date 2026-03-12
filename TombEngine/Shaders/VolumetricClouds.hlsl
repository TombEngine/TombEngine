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
float FBMLowFreq(float3 p)
{
	float v  = 0.0f;
	float a  = 0.5f;
	float3 s = p;

	[unroll]
	for (int oct = 0; oct < 3; oct++)
	{
		v += a * ValueNoise3D(s);
		s *= 2.37f;  // Lacunarity (non-power-of-2 avoids tiling artifacts)
		a *= 0.5f;
	}
	return v;
}

// High-frequency FBM (2 octaves) — detail erosion.
float FBMDetail(float3 p)
{
	float v  = 0.0f;
	float a  = 0.5f;
	float3 s = p;

	[unroll]
	for (int oct = 0; oct < 2; oct++)
	{
		v += a * ValueNoise3D(s);
		s *= 2.73f;
		a *= 0.5f;
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
float HeightFraction(float worldY, float bottomY, float thickness)
{
	return saturate((worldY - bottomY) / max(thickness, 1.0f));
}

// Height-based density gradient: rounded-bottom cumulus-like profile.
// Produces soft ramp-in at bottom, plateau in middle, smooth fade at top.
float HeightGradient(float heightFrac)
{
	// Bottom ramp: soft entry from 0.0 to 0.15
	float bottom = smoothstep(0.0f, 0.15f, heightFrac);
	// Top ramp: fade starting at 0.7
	float top = 1.0f - smoothstep(0.7f, 1.0f, heightFrac);
	return bottom * top;
}

// ===========================================================================
// Cloud density sampling
// ===========================================================================

float CloudDensityAtWorldPos(float3 worldPos, float heightFrac, bool useDetail)
{
	// --- Coverage / Weather noise ---
	// Large-scale weather map controls where clouds exist.
	float2 weatherUV = worldPos.xz * WeatherScale 
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

	// --- Base shape noise ---
	float3 shapePos = worldPos * ShapeScale 
	                 + float3(WindDirection.x, 0.0f, WindDirection.y) 
	                   * CloudTime * WindSpeed;
	float baseShape = FBMLowFreq(shapePos);

	// Remap shape noise to create hard-edged cloud masses.
	float shapeDensity = Remap(baseShape, 0.3f, 1.0f, 0.0f, 1.0f);
	shapeDensity *= coverageMask * hGrad;

	if (shapeDensity <= 0.001f)
		return 0.0f;

	// --- Detail erosion ---
	if (useDetail && DetailNoiseEnabled != 0)
	{
		// Detail noise uses different wind speed for local evolution.
		float3 detailPos = worldPos * DetailScale 
		                  + float3(WindDirection.x, 0.3f, WindDirection.y) 
		                    * CloudTime * EvolutionSpeed;
		float detail = FBMDetail(detailPos);

		// Erode the shape by subtracting weighted detail.
		// More erosion near cloud edges (low density).
		float erosionWeight = lerp(DetailStrength, DetailStrength * 0.25f, shapeDensity);
		shapeDensity = Remap(shapeDensity, erosionWeight * detail, 1.0f, 0.0f, 1.0f);
	}

	return max(shapeDensity * CloudDensity, 0.0f);
}

// ===========================================================================
// Ray-sphere intersection for cloud shell
// ===========================================================================

// Returns (tNear, tFar) for intersection with a sphere centered at (0, planetCenterY, 0).
// Returns (-1, -1) if no intersection.
float2 IntersectSphere(float3 rayOrigin, float3 rayDir, float sphereRadius)
{
	float3 center = float3(0.0f, PlanetCenterY, 0.0f);
	float3 oc = rayOrigin - center;

	float b = dot(oc, rayDir);
	float c = dot(oc, oc) - sphereRadius * sphereRadius;
	float disc = b * b - c;

	if (disc < 0.0f)
		return float2(-1.0f, -1.0f);

	float sqrtDisc = sqrt(disc);
	return float2(-b - sqrtDisc, -b + sqrtDisc);
}

// Intersect ray with the cloud shell (two concentric spheres).
// Returns (tEntry, tExit) of the overlapping region.
float2 IntersectCloudVolume(float3 rayOrigin, float3 rayDir)
{
	float innerRadius = EarthRadius + CloudBottomHeight;
	float outerRadius = EarthRadius + CloudTopHeight;

	float2 innerHit = IntersectSphere(rayOrigin, rayDir, innerRadius);
	float2 outerHit = IntersectSphere(rayOrigin, rayDir, outerRadius);

	// Camera is below cloud layer (most common case).
	if (innerHit.x < 0.0f && innerHit.y < 0.0f)
	{
		// Camera inside inner sphere — cloud volume starts at outer sphere near hit.
		// This shouldn't typically happen for sky views.
		return float2(-1.0f, -1.0f);
	}

	// Camera below inner sphere: march from inner far to outer far.
	if (innerHit.x > 0.0f)
		return float2(innerHit.x, outerHit.y);

	// Camera inside cloud layer: march from 0 to outer far.
	if (innerHit.y > 0.0f && outerHit.x < 0.0f)
		return float2(0.0f, outerHit.y);

	// Camera above cloud layer: march from outer near to inner near.
	if (outerHit.x > 0.0f)
		return float2(outerHit.x, innerHit.x);

	return float2(-1.0f, -1.0f);
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
	float forward  = HenyeyGreenstein(cosTheta, PhaseForward);
	float backward = HenyeyGreenstein(cosTheta, -PhaseBackward);
	return lerp(backward, forward, 0.7f);
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
		           CamPositionWS.y + CloudBottomHeight, CloudThickness);

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
	float powder = 1.0f - exp(-accumDensity * Absorption * 2.0f);
	powder = lerp(1.0f, powder, SilverliningStr);

	return transmittance * powder;
}

// ===========================================================================
// Simple blue-noise-like jitter from screen position + frame
// ===========================================================================

float ScreenJitter(float2 screenPos)
{
	// Interleaved gradient noise (Jimenez 2014) — good temporal properties.
	float3 magic = float3(0.06711056f, 0.00583715f, 52.9829189f);
	float jitter = frac(magic.z * frac(dot(screenPos + FrameIndex * float2(1.7f, 3.5f), magic.xy)));
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
		                    rayOrigin.y + CloudBottomHeight, CloudThickness);

		// Skip samples outside the cloud layer boundary.
		if (heightFrac >= 0.0f && heightFrac <= 1.0f)
		{
			// Sample density (use detail noise if available).
			bool useDetail = (DetailNoiseEnabled != 0);
			float density = CloudDensityAtWorldPos(samplePos, heightFrac, useDetail);

			if (density > 0.001f)
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
		float2 wUV = samplePos.xz * WeatherScale + WindDirection * CloudTime * WindSpeed * 0.3f;
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

	// Only render clouds for upward-looking rays (above horizon).
	// Allow a small angle below horizon for seamless blending.
	if (rayDir.y < -0.02f)
		return float4(0.0f, 0.0f, 0.0f, 0.0f);

	float4 cloudResult = RaymarchClouds(rayOrigin, rayDir, input.Position.xy);

	// Debug views.
	if (CloudDebugView != 0)
		cloudResult = DebugVisualization(rayOrigin, rayDir, input.Position.xy, cloudResult, PrimaryStepCount);

	return cloudResult;
}

// ===========================================================================
// PSCloudComposite — Upscale half-res clouds and composite over scene
// ===========================================================================

float4 PSCloudComposite(VSOutput input) : SV_TARGET
{
	// Sample background scene color.
	float4 sceneColor = SceneColorTexture.Sample(PointSamp, input.UV);

	// Sample cloud result (bilinear upscale from half-res).
	float4 cloudColor = CloudTexture.Sample(LinearSamp, input.UV);

	// Alpha blend: clouds over scene.
	float3 result = lerp(sceneColor.rgb, cloudColor.rgb, cloudColor.a);
	return float4(result, sceneColor.a);
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
		                   rayOrigin.y + CloudBottomHeight, CloudThickness);

		if (heightFrac >= 0.0f && heightFrac <= 1.0f)
		{
			float density = CloudDensityAtWorldPos(samplePos, heightFrac, false);
			transmittance *= exp(-density * Absorption * stepSize);
		}

		t += stepSize;
	}

	return float4(transmittance, 0.0f, 0.0f, 1.0f);
}
