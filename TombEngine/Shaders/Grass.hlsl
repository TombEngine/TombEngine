// Grass.hlsl - Instanced grass blade rendering with influence-based bending.
// Each grass blade is a vertical quad (2 triangles, 6 vertices) positioned and
// oriented per-instance. The vertex shader generates quad vertices from InstanceID
// and VertexID, applies wind animation, and bends blades based on nearby influence
// spheres (e.g. the player walking through).

#include "./Math.hlsli"
#include "./CBCamera.hlsli"
#include "./CBGrass.hlsli"
#include "./ShaderLight.hlsli"
#include "./Blending.hlsli"

struct PixelShaderInput
{
	float4 Position      : SV_POSITION;
	float3 WorldPosition : POSITION0;
	float3 Normal        : NORMAL;
	float2 UV            : TEXCOORD0;
	float4 Color         : COLOR;
	float4 FogBulbs      : TEXCOORD2;
	float  DistanceFog   : FOG;
	float  FadeFactor    : TEXCOORD3;
	nointerpolation float3 AmbientLight  : TEXCOORD1;
	nointerpolation float3 SunDirection  : TEXCOORD4;
	nointerpolation float  SunIntensity  : TEXCOORD5;
	nointerpolation float3 SunColor      : TEXCOORD6;
};

struct PixelShaderOutput
{
	float4 Color : SV_TARGET0;
};

Texture2D GrassTexture : register(t14);
SamplerState GrassSampler : register(s14);

// ---------- Vertex generation ----------
// Generates a vertical quad from VertexID (0..5 for two triangles).
// Returns local-space position: X = [-0.5, 0.5], Y = [0, 1], Z = 0.

static const float2 QuadPos[6] =
{
	float2(-0.5f, 0.0f), // BL
	float2( 0.5f, 0.0f), // BR
	float2( 0.5f, 1.0f), // TR
	float2(-0.5f, 0.0f), // BL
	float2( 0.5f, 1.0f), // TR
	float2(-0.5f, 1.0f)  // TL
};

static const float2 QuadUV[6] =
{
	float2(0.0f, 1.0f),
	float2(1.0f, 1.0f),
	float2(1.0f, 0.0f),
	float2(0.0f, 1.0f),
	float2(1.0f, 0.0f),
	float2(0.0f, 0.0f)
};

// ---------- Wind animation ----------
// Produces organic traveling-wave displacement along and across the wind direction.
// Wave crests propagate in the wind direction; multiple harmonics add variation.
// windEnabled: 1.0 = outdoor room (waving on); 0.0 = indoor (no wind).
float3 ComputeWind(float3 worldPos, float seed, float heightFrac, float windEnabled)
{
	// Only wave in outdoor (sky-visible) rooms.
	if (windEnabled < 0.5f)
		return float3(0.0f, 0.0f, 0.0f);

	// Wind only affects upper portion of blade — squared mask for stiffer natural base.
	float windMask = saturate(heightFrac * heightFrac);

	// 2D wind direction (XZ-plane); guaranteed normalized on the CPU side.
	float2 windDir2D  = float2(WindDirection.x, WindDirection.z);
	float2 windPerp2D = float2(-windDir2D.y, windDir2D.x); // Perpendicular in XZ.

	// Traveling-wave spatial phases: crests propagate along / across wind direction.
	// 0.008 ≈ 785 world-unit wavelength; 0.005 ≈ 1257 wu cross-wavelength.
	float forwardPhase = dot(worldPos.xz, windDir2D ) * 0.008f;
	float crossPhase   = dot(worldPos.xz, windPerp2D) * 0.005f;

	// Per-blade seed shifts phase so blades don't all peak simultaneously.
	float seedOff = seed * PI2;

	float t = Time * WindFrequency;

	// Primary wave: strong, slow rolling sway in the wind direction.
	float wave1 = sin(t        - forwardPhase          + seedOff);
	// Second harmonic: faster, smaller — adds ripple on top of the primary sway.
	float wave2 = sin(t * 1.7f - forwardPhase * 1.3f   + seedOff * 0.6f)  * 0.4f;
	// Cross ripple: subtle sideways wobble perpendicular to the wind.
	float side  = cos(t * 0.8f - crossPhase            + seedOff * 1.4f)  * 0.25f;

	// Forward displacement (in wind direction) + sideways ripple.
	float3 windOffset =
		float3(windDir2D.x,  0.0f, windDir2D.y)  * (wave1 + wave2) * WindStrength        * windMask +
		float3(windPerp2D.x, 0.0f, windPerp2D.y) * side            * WindStrength * 0.3f * windMask;

	return windOffset;
}

// ---------- Influence bending ----------
float3 ComputeInfluenceBend(float3 bladeBase, float seed, float heightFrac)
{
	float3 totalBend = float3(0.0f, 0.0f, 0.0f);

	// Bend mask: anchored at base, full bend from ~40% height upward.
	// smoothstep gives a more natural bowing look than heightFrac^2.
	float bendMask = smoothstep(0.0f, 0.4f, heightFrac);

	for (int i = 0; i < NumInfluences; i++)
	{
		float3 toGrass = bladeBase - Influences[i].Position;
		float dist = length(toGrass);
		float radius = Influences[i].Radius;

		if (dist > radius || dist < 0.001f)
			continue;

		// Distance falloff: strongest at center, zero at edge.
		float falloff = 1.0f - saturate(dist / radius);
		falloff = falloff * falloff; // Quadratic falloff.

		// Rise: ramps from 0 to 1 based on time since first contact.
		// BendRiseSpeed controls how fast blades initially bend (lower = slower).
		float timeSinceBorn   = Time - Influences[i].BirthTimestamp;
		float rise            = saturate(timeSinceBorn * BendRiseSpeed);

		// Decay: full strength while actively refreshed; exponential falloff after.
		// A small active window accounts for per-frame timestamp refresh lag.
		float timeSinceActive = Time - Influences[i].Timestamp;
		float activeWindow    = 0.2f;
		float decayTime       = max(timeSinceActive - activeWindow, 0.0f);
		float timeFactor      = rise * exp(-decayTime * BendDecaySpeed);

		// Per-instance variation using seed.
		float variation = 0.8f + 0.4f * frac(seed * 127.1f);

		// Bend direction: push grass away from influence center.
		float3 bendDir = toGrass / dist;

		// When directly underfoot (close to center), collapse more vertically.
		// When farther from center, push more horizontally outward.
		float closeness = 1.0f - saturate(dist / (radius * 0.4f));
		bendDir.y = lerp(0.3f, 1.5f, closeness); // Positive Y = downward in TEN.
		bendDir = SafeNormalize(bendDir);

		float bendStrength = Influences[i].Intensity * falloff * timeFactor * variation * BendMaxAngle;
		totalBend += bendDir * bendStrength * bendMask;
	}

	return totalBend;
}

// ---------- Vertex Shader ----------
PixelShaderInput VS(uint VertexID : SV_VertexID, uint InstanceID : SV_InstanceID)
{
	PixelShaderInput output;

	GrassInstanceData inst = GrassInstances[InstanceID];

	// Generate quad vertex from VertexID.
	float2 localPos = QuadPos[VertexID % 6];
	float2 uv = QuadUV[VertexID % 6];
	float heightFrac = localPos.y; // 0 at base, 1 at tip.

	// Scale the quad.
	float3 bladeLocal;
	bladeLocal.x = localPos.x * BladeWidth * inst.Scale;
	bladeLocal.y = localPos.y * BladeHeight * inst.Scale;
	bladeLocal.z = 0.0f;

	// Create a rotation matrix so the blade faces the camera (Y-axis billboard).
	// The blade rotates around its up vector to face the camera, but stays upright.
	float3 up = inst.Normal;
	float3 toCamera = CamPositionWS.xyz - inst.Position;

	toCamera.y = 0.0f; // Keep upright.

	float camDist = length(toCamera);
	float3 forward;

	if (camDist > 0.01f)
		forward = toCamera / camDist;
	else
		forward = float3(0.0f, 0.0f, 1.0f);

	// Per-instance random rotation to break uniformity.
	// Use seed to add a fixed rotation offset so blades aren't all aligned.
	float rotAngle = inst.Seed * 6.283f;
	float cosR = cos(rotAngle);
	float sinR = sin(rotAngle);

	float3 rotatedForward = float3(
		forward.x * cosR - forward.z * sinR,
		0.0f,
		forward.x * sinR + forward.z * cosR
	);

	rotatedForward = SafeNormalize(rotatedForward);

	float3 right = cross(up, rotatedForward);
	right = SafeNormalize(right);
	float3 adjustedForward = cross(right, up);

	// Local to world: right = X, up = Y, forward = Z.
	float3 worldPos = inst.Position
		+ right * bladeLocal.x
		+ up * bladeLocal.y
		+ adjustedForward * bladeLocal.z;

	// Apply wind animation (traveling waves; only in outdoor rooms).
	float3 wind = ComputeWind(worldPos, inst.Seed, heightFrac, inst.WindEnabled);
	worldPos += wind;

	// Apply influence bending.
	float3 bend = ComputeInfluenceBend(inst.Position, inst.Seed, heightFrac);
	worldPos += bend;

	// LOD: fade based on distance from camera.
	float distToCamera = length(CamPositionWS.xyz - inst.Position);
	float fadeFactor = 1.0f - saturate((distToCamera - FadeStartDistance) / max(MaxDrawDistance - FadeStartDistance, 1.0f));

	output.Position = mul(float4(worldPos, 1.0f), ViewProjection);
	output.WorldPosition = worldPos;
	output.Normal = up;
	output.UV = uv * inst.UVScale + inst.UVOffset;
	output.Color = inst.Color;
	output.AmbientLight = inst.AmbientLight;
	output.FadeFactor = fadeFactor;

	// Pass per-instance sun data to pixel shader.
	output.SunDirection = inst.SunDirection;
	output.SunIntensity = inst.SunIntensity;
	output.SunColor = inst.SunColor;

	output.FogBulbs = DoFogBulbsForVertex(worldPos);
	output.DistanceFog = DoDistanceFogForVertex(worldPos);

	return output;
}

// ---------- Pixel Shader ----------
PixelShaderOutput PS(PixelShaderInput input)
{
	PixelShaderOutput output;

	float4 tex = GrassTexture.Sample(GrassSampler, input.UV);

	// Hard alpha cutout for solid, opaque grass appearance.
	clip(tex.a - 0.5f);

	// Room ambient lighting (from per-instance data).
	// Modulate with a hemispherical gradient for natural variation.
	float NdotUp = dot(input.Normal, float3(0.0f, -1.0f, 0.0f)) * 0.5f + 0.5f;
	float hemiBlend = lerp(0.7f, 1.0f, NdotUp);
	float3 ambient = input.AmbientLight * hemiBlend;

	// Gradient: darker at base, lighter at tip.
	// UV.y=0 is top, UV.y=1 is bottom - darken at bottom.
	float baseGradient = lerp(0.5f, 1.0f, 1.0f - input.UV.y);

	// Sun bulb directional lighting (per-room, from instance data).
	float3 sunLighting = float3(0.0f, 0.0f, 0.0f);

	if (input.SunIntensity > 0.0f)
	{
		// TEN uses inverted Y (up = -Y), so the sun direction points "down" in world space.
		// NdotL uses the blade's up normal against the negated sun direction.
		float NdotL = saturate(dot(input.Normal, -input.SunDirection));

		// Wrap lighting: soften the NdotL falloff so grass doesn't go fully black
		// on faces pointing away from the sun. wrap=0.5 gives a half-lambert look.
		float wrap = 0.5f;
		float wrappedNdotL = saturate((NdotL + wrap) / (1.0f + wrap));

		sunLighting = input.SunColor * input.SunIntensity * wrappedNdotL;
	}

	float3 color = tex.rgb * input.Color.rgb * (ambient + sunLighting) * baseGradient;

	// Apply distance fade.
	float alpha = tex.a * input.FadeFactor;
	clip(alpha - 0.1f);

	output.Color = float4(color, alpha);

	// Apply fog.
	output.Color = DoFogBulbsForPixel(output.Color, float4(input.FogBulbs.xyz, 1.0f));
	output.Color = DoDistanceFogForPixel(output.Color, FogColor, input.DistanceFog);

	return output;
}
