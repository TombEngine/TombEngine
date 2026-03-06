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
	float4 Position     : SV_POSITION;
	float3 WorldPosition: POSITION0;
	float3 Normal       : NORMAL;
	float2 UV           : TEXCOORD0;
	float4 Color        : COLOR;
	float4 PositionCopy : TEXCOORD1;
	float4 FogBulbs     : TEXCOORD2;
	float  DistanceFog  : FOG;
	float  FadeFactor   : TEXCOORD3;
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
float3 ComputeWind(float3 worldPos, float seed, float heightFrac)
{
	// Wind only affects upper portion of blade.
	float windMask = saturate(heightFrac * heightFrac);

	// Use fractal noise for organic variation.
	float windPhase = Time * WindFrequency + seed * 6.283f;
	float noiseVal = sin(windPhase + worldPos.x * 0.01f + worldPos.z * 0.013f) *
	                 cos(windPhase * 0.7f + worldPos.z * 0.009f);

	float3 windOffset = WindDirection * noiseVal * WindStrength * windMask;
	return windOffset;
}

// ---------- Influence bending ----------
float3 ComputeInfluenceBend(float3 worldPos, float seed, float heightFrac)
{
	float3 totalBend = float3(0.0f, 0.0f, 0.0f);

	// Only bend upper portion of blade.
	float bendMask = saturate(heightFrac * heightFrac);

	for (int i = 0; i < NumInfluences; i++)
	{
		float3 toGrass = worldPos - Influences[i].Position;
		float dist = length(toGrass);
		float radius = Influences[i].Radius;

		if (dist > radius || dist < 0.001f)
			continue;

		// Distance falloff: strongest at center, zero at edge.
		float falloff = 1.0f - saturate(dist / radius);
		falloff = falloff * falloff; // Quadratic falloff.

		// Time-based rise/decay: how long since the influence was active.
		float timeSinceActive = Time - Influences[i].Timestamp;
		float decay = exp(-timeSinceActive * BendDecaySpeed);
		float rise = saturate(timeSinceActive * BendRiseSpeed);
		float timeFactor = rise * decay;

		// Per-instance variation using seed.
		float variation = 0.8f + 0.4f * frac(seed * 127.1f);

		// Bend direction: push grass away from influence center, biased downward.
		float3 bendDir = SafeNormalize(toGrass);
		bendDir.y = 0.5f; // Push blades downward (positive Y = down in TEN).
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

	// Apply wind animation.
	float3 wind = ComputeWind(worldPos, inst.Seed, heightFrac);
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
	output.PositionCopy = output.Position;
	output.FadeFactor = fadeFactor;

	output.FogBulbs = DoFogBulbsForVertex(worldPos);
	output.DistanceFog = DoDistanceFogForVertex(worldPos);

	return output;
}

// ---------- Pixel Shader ----------
PixelShaderOutput PS(PixelShaderInput input)
{
	PixelShaderOutput output;

	float4 tex = GrassTexture.Sample(GrassSampler, input.UV);

	// Alpha cutout.
	clip(tex.a - 0.5f);

	// Simple hemispherical ambient lighting (TEN uses inverted Y: up = -Y).
	float NdotUp = dot(input.Normal, float3(0.0f, -1.0f, 0.0f)) * 0.5f + 0.5f;
	float3 ambient = lerp(float3(0.3f, 0.25f, 0.2f), float3(0.7f, 0.75f, 0.65f), NdotUp);

	// Gradient: darker at base, lighter at tip.
	// UV.y=0 is top, UV.y=1 is bottom - darken at bottom.
	float baseGradient = lerp(0.5f, 1.0f, 1.0f - input.UV.y);

	float3 color = tex.rgb * input.Color.rgb * ambient * baseGradient;

	// Apply distance fade.
	float alpha = tex.a * input.FadeFactor;
	clip(alpha - 0.1f);

	output.Color = float4(color, alpha);

	// Apply fog.
	output.Color = DoFogBulbsForPixel(output.Color, float4(input.FogBulbs.xyz, 1.0f));
	output.Color = DoDistanceFogForPixel(output.Color, FogColor, input.DistanceFog);

	return output;
}
