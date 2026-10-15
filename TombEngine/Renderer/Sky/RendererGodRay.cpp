// ============================================================================
// RendererGodRay.cpp - Lightweight screen-space god ray rendering integration.
//
// Implements the renderer-side logic for god rays:
//   - Initialization: render target, constant buffer
//   - Per-frame CB update: sun position, auto-strength, cloud state
//   - Draw calls: half-res radial shaft pass + full-res additive composite
//
// Called from the main render pipeline in RendererDraw.cpp, after volumetric
// clouds are composited and before the GBuffer pass.
// ============================================================================

#include "framework.h"
#include "Renderer/Renderer.h"

#include "Game/Sky/SkyCloudSystem.h"
#include "Renderer/ConstantBuffers/GodRayBuffer.h"
#include "Renderer/Sky/GodRaySettings.h"
#include "Renderer/Sky/MoonSettings.h"
#include "Renderer/Sky/SkyQuality.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Scripting/Include/ScriptInterfaceLevel.h"
#include "Scripting/Internal/TEN/Flow/Level/FlowLevel.h"
#include "Specific/level.h"
#include "Specific/trutils.h"

using namespace TEN::Sky;
using namespace TEN::Renderer::ConstantBuffers;
using namespace TEN::Renderer::GodRay;

namespace TEN::Renderer
{
	// God ray half-res scale.  0.5 = quarter pixel count.
	static constexpr float GOD_RAY_RESOLUTION_SCALE = 0.5f;

	// ========================================================================
	// Initialization
	// ========================================================================

	// ========================================================================
	// Auto-strength computation
	// ========================================================================

	static float ComputeGodRayAutoStrength(float sunElevation, float cloudCoverage)
	{
		// Elevation factor: slightly stronger when sun is low/near horizon.
		// No hard cut-off - sunset is the MOST dramatic time for god rays.
		float elevFactor = std::max(1.0f - sunElevation * 0.6f, 0.3f);

		// Cloud coverage factor: ramps from 0 (no clouds) to 1.0 at ~33% coverage.
		float coverageFactor = std::clamp(cloudCoverage * 3.0f, 0.0f, 1.0f);

		return elevFactor * coverageFactor;
	}

	// ========================================================================
	// Horizon mask pass
	// ========================================================================

	// ========================================================================
	// Per-frame constant buffer update
	// ========================================================================

	void Renderer::UpdateGodRayBuffer(RenderView& renderView)
	{
		const auto& settings = _godRaySettings;
		const auto& moon     = _moonSettings;

		// --- Sun direction and screen position ---
		Vector3 sunDir(0.0f, -1.0f, 0.0f);
		Vector3 sunColor(1.0f, 0.95f, 0.85f);
		float   sunElevation = 1.0f;
		Vector2 sunScreenUV(-10.0f, -10.0f);  // sentinel: sun behind camera

		auto* levelPtr = g_GameFlow->GetLevel(CurrentLevel);
		if (levelPtr->GetLensFlareEnabled())
		{
			constexpr float SHORT_TO_RAD = (PI_MUL_2 / 65536.0f);
			float pitch = (float)levelPtr->GetLensFlarePitch() * SHORT_TO_RAD;
			float yaw   = (float)levelPtr->GetLensFlareYaw()   * SHORT_TO_RAD;

			sunDir = Vector3(
				std::cos(pitch) * std::sin(yaw),
				-std::sin(pitch),
				std::cos(pitch) * std::cos(yaw));
			sunDir.Normalize();

			sunElevation = std::sin(pitch);

			auto flareColor = levelPtr->GetLensFlareEvaluatedColor();
			sunColor = Vector3(flareColor.x, flareColor.y, flareColor.z);
		}
		else
		{
			// Fallback: use cloud system light direction (same source as volumetric clouds).
			sunDir = _stVolumetricCloud.LightDirection;
			if (sunDir.LengthSquared() > 0.001f)
			{
				sunDir.Normalize();
				// sunDir.y = -sin(pitch) in TEN's Y-down space, so elevation = -sunDir.y.
				sunElevation = -sunDir.y;
			}
		}

		// --- Determine if we should use moon god rays at night ---
		float dayNightBlend = ComputeDayNightBlend(sunElevation);
		bool useMoonRays = moon.Enabled && moon.GodRays.Enabled && dayNightBlend > 0.5f;

		// The effective ray source direction, screen UV, color, and settings
		// switch from sun to moon based on day/night state.
		Vector3 rayDir   = sunDir;
		Vector3 rayColor = sunColor;
		Vector2 rayScreenUV(-10.0f, -10.0f);
		float   rayLength, rayIntensity, rayDecay, raySoftness, rayAutoMix;
		int     raySampleCount;

		if (useMoonRays)
		{
			// Build moon direction.
			float moonPitchRad = moon.Pitch * (PI / 180.0f);
			float moonYawRad   = moon.Yaw   * (PI / 180.0f);
			rayDir = Vector3(
				std::cos(moonPitchRad) * std::sin(moonYawRad),
				-std::sin(moonPitchRad),
				std::cos(moonPitchRad) * std::cos(moonYawRad));
			rayDir.Normalize();

			// Moon phase brightness for ray intensity modulation.
			float moonPhase = ComputeMoonPhase(sunDir, rayDir);
			float phaseBrightness = moonPhase * moonPhase * (3.0f - 2.0f * moonPhase);

			// Moon color: cool bluish tint.
			rayColor = Vector3(moon.BaseColorR, moon.BaseColorG, moon.BaseColorB) * phaseBrightness;

			rayLength      = moon.GodRays.Length;
			rayIntensity   = moon.GodRays.Intensity * phaseBrightness;
			rayDecay       = moon.GodRays.Decay;
			raySampleCount = moon.GodRays.SampleCount;
			raySoftness    = moon.GodRays.Softness;
			rayAutoMix     = moon.GodRays.AutoStrength;
		}
		else
		{
			// Daytime: use sun settings.
			rayLength      = settings.Length;
			rayIntensity   = settings.Intensity;
			rayDecay       = settings.Decay;
			raySampleCount = settings.SampleCount;
			raySoftness    = settings.Softness;
			rayAutoMix     = settings.AutoStrengthMix;
		}

		// Project ray source (sun or moon) to screen UV.
		if (!useMoonRays && !renderView.LensFlaresToDraw.empty() && renderView.LensFlaresToDraw[0].IsGlobal)
		{
			const auto& sunPos = renderView.LensFlaresToDraw[0].Position;
			auto clip = Vector4::Transform(
				Vector4(sunPos.x, sunPos.y, sunPos.z, 1.0f),
				renderView.Camera.ViewProjection);
			// Only project if the source is in front of the camera (clip.w > 0).
			// Using abs(clip.w) was wrong: when the sun is behind the camera clip.w is
			// negative, and abs() inverts the sign, placing the projected UV near the
			// screen centre instead of the off-screen sentinel.
			if (clip.w > 0.0001f)
			{
				float ndcX = clip.x / clip.w;
				float ndcY = clip.y / clip.w;
				// Cap at +/-10 NDC to avoid numeric issues with near-clipped positions.
				// Off-screen sources are handled gracefully by the shader: sunDiscVis
				// fades to zero and off-screen march samples are excluded, giving a
				// smooth natural fade-out instead of an abrupt cutoff.
				if (std::abs(ndcX) <= 10.0f && std::abs(ndcY) <= 10.0f)
					rayScreenUV = Vector2(ndcX * 0.5f + 0.5f, ndcY * -0.5f + 0.5f);
			}
		}

		// Fallback: project a virtual source far along rayDir from the camera.
		if (rayScreenUV.x < -5.0f && rayDir.LengthSquared() > 0.001f)
		{
			constexpr float VIRTUAL_SUN_DIST = 500000.0f;
			Vector3 virtualPos = renderView.Camera.WorldPosition + rayDir * VIRTUAL_SUN_DIST;
			auto clip = Vector4::Transform(
				Vector4(virtualPos.x, virtualPos.y, virtualPos.z, 1.0f),
				renderView.Camera.ViewProjection);
			if (clip.w > 0.0001f)
			{
				float ndcX = clip.x / clip.w;
				float ndcY = clip.y / clip.w;
				if (std::abs(ndcX) <= 10.0f && std::abs(ndcY) <= 10.0f)
					rayScreenUV = Vector2(ndcX * 0.5f + 0.5f, ndcY * -0.5f + 0.5f);
			}
		}

		// Ray-source-facing fade: smoothly mute rays as the camera turns away.
		float rayFacingDot  = rayDir.Dot(renderView.Camera.WorldDirection);
		float rayFacingFade = std::clamp((rayFacingDot - (-0.25f)) / (0.15f - (-0.25f)), 0.0f, 1.0f);
		rayFacingFade = rayFacingFade * rayFacingFade * (3.0f - 2.0f * rayFacingFade);

		// --- Cloud coverage for auto-strength ---
		// Aurora-category layers are not real clouds and must not boost god ray strength.
		float cloudCoverage = 0.0f;
		const int auroraType = (int)CloudCategory::Aurora;
		if (g_SkyCloudSystem.IsCloudAActive() || g_SkyCloudSystem.IsCloudBActive())
		{
			float coverageA = 0.0f, coverageB = 0.0f;
			if (g_SkyCloudSystem.IsCloudAActive() &&
			    g_SkyCloudSystem.GetCloudARenderSettings().CloudType != auroraType)
				coverageA = g_SkyCloudSystem.GetCloudARenderSettings().Coverage;
			if (g_SkyCloudSystem.IsCloudBActive() &&
			    g_SkyCloudSystem.GetCloudBRenderSettings().CloudType != auroraType)
				coverageB = g_SkyCloudSystem.GetCloudBRenderSettings().Coverage;
			cloudCoverage = std::max(coverageA, coverageB);
		}

		// Underwater sky acts as an occlusion layer for the sun. Use the preset
		// visibility as synthetic coverage so god rays emerge from the bright
		// Snell's window at the sun's projected position on the water surface,
		// without requiring volumetric clouds to be present.
		float underwaterVis = _stAtmosphericSky.UnderwaterSkyVisibility;
		if (underwaterVis > 0.001f)
		{
			// 0.5 hits the breakup parabola peak (4*x*(1-x)) for max strength.
			float syntheticCoverage = 0.5f * underwaterVis;
			cloudCoverage = std::max(cloudCoverage, syntheticCoverage);
		}

		// --- Auto-strength ---
		float rayElevation = useMoonRays ? std::sin(moon.Pitch * (PI / 180.0f)) : sunElevation;
		float autoStrength = ComputeGodRayAutoStrength(rayElevation, cloudCoverage);

		// Fade: for sun rays, fade out when sun is below horizon.
		// For moon rays, fade out when moon is below horizon.
		float belowHorizonFade;
		if (useMoonRays)
		{
			float moonElev = std::sin(moon.Pitch * (PI / 180.0f));
			belowHorizonFade = std::clamp(1.0f + moonElev * 8.0f, 0.0f, 1.0f);
		}
		else
		{
			belowHorizonFade = std::clamp(1.0f + sunElevation * 8.0f, 0.0f, 1.0f);
		}
		belowHorizonFade = belowHorizonFade * belowHorizonFade * (3.0f - 2.0f * belowHorizonFade);

		float finalAutoStrength = (1.0f + (autoStrength - 1.0f) * rayAutoMix) * rayFacingFade * belowHorizonFade;

		// Underwater sky overrides the below-horizon fade: even when the sun is
		// physically low, rays should still shine through the water surface.
		// Hold strength proportional to the underwater visibility and the user
		// shaft slider so the artist can dial overall intensity.
		if (underwaterVis > 0.001f && !useMoonRays)
		{
			float shaftMul = _underwaterSkySettings.ShaftStrength;
			float underwaterStrength = underwaterVis * shaftMul * rayFacingFade;
			finalAutoStrength = std::max(finalAutoStrength, underwaterStrength);
		}

		// --- Fill constant buffer ---
		_stGodRay.SunScreenPos  = rayScreenUV;
		_stGodRay.RayLength     = rayLength;
		_stGodRay.Intensity     = rayIntensity;

		_stGodRay.Decay         = rayDecay;
		{
			// Cap sample count by the player's atmospheric sky quality preset.
			auto caps = GetSkyQualityCaps(GetCurrentSkyQuality());
			if (raySampleCount > caps.GodRaySampleCountMax)
				raySampleCount = caps.GodRaySampleCountMax;
		}
		_stGodRay.SampleCount   = raySampleCount;
		_stGodRay.SunElevation  = rayElevation;
		_stGodRay.AutoStrength  = finalAutoStrength;

		// Apply atmospheric sky gradient to daytime sun rays.
		if (!useMoonRays)
		{
			const auto& atmo = _atmosphericSkySettings;
			float sunInfl = std::max(0.0f, 1.0f - sunElevation * atmo.SunElevationRampSpeed);
			float blend   = sunInfl * atmo.SunWarmInfluence;
			rayColor.x = 1.0f + (rayColor.x - 1.0f) * blend;
			rayColor.y = 1.0f + (rayColor.y - 1.0f) * blend;
			rayColor.z = 1.0f + (rayColor.z - 1.0f) * blend;
		}
		_stGodRay.SunColor      = rayColor;
		_stGodRay.Softness      = raySoftness;

		_stGodRay.ViewSize      = Vector2((float)_screenWidth, (float)_screenHeight);
		_stGodRay.InvViewSize   = Vector2(1.0f / (float)_screenWidth, 1.0f / (float)_screenHeight);

		// Underwater shaft mode: when the underwater sky is visible, drive the god
		// ray shader with a procedural wave mask instead of the (empty) cloud RT.
		// Wind direction is sourced from the atmospheric sky CB so drift matches exactly.
		if (underwaterVis > 0.001f && !useMoonRays)
		{
			_stGodRay.UnderwaterShaftActive     = 1.0f;
			_stGodRay.UnderwaterShaftBrightness = underwaterVis * _underwaterSkySettings.ShaftStrength;
			_stGodRay.UnderwaterShaftTime       = _stAtmosphericSky.UnderwaterTime;
			_stGodRay.UnderwaterShaftSharpness  = std::max(0.25f, _underwaterSkySettings.ShaftSharpness);
			_stGodRay.UnderwaterWindX           = _stAtmosphericSky.UnderwaterWindDirX;
			_stGodRay.UnderwaterWindY           = _stAtmosphericSky.UnderwaterWindDirY;
			_stGodRay.UnderwaterRayLength       = _underwaterSkySettings.RayLength;
			_stGodRay.UnderwaterRayDecay        = _underwaterSkySettings.RayDecay;
			_stGodRay.UnderwaterRayIntensity    = _underwaterSkySettings.RayIntensity;
			_stGodRay.UnderwaterSampleCount     = std::max(8, _underwaterSkySettings.RaySampleCount);
			{
				// Cap underwater sample count by sky-quality preset as well.
				auto caps = GetSkyQualityCaps(GetCurrentSkyQuality());
				if (_stGodRay.UnderwaterSampleCount > caps.GodRaySampleCountMax)
					_stGodRay.UnderwaterSampleCount = caps.GodRaySampleCountMax;
			}
		}
		else
		{
			_stGodRay.UnderwaterShaftActive     = 0.0f;
			_stGodRay.UnderwaterShaftBrightness = 0.0f;
			_stGodRay.UnderwaterShaftTime       = 0.0f;
			_stGodRay.UnderwaterShaftSharpness  = 1.0f;
			_stGodRay.UnderwaterWindX           = 0.0f;
			_stGodRay.UnderwaterWindY           = 0.0f;
			_stGodRay.UnderwaterRayLength       = 0.0f;
			_stGodRay.UnderwaterRayDecay        = 0.0f;
			_stGodRay.UnderwaterRayIntensity    = 0.0f;
			_stGodRay.UnderwaterSampleCount     = 8;
		}

	}

	// ========================================================================
	// Draw god rays
	// ========================================================================

}
