// ============================================================================
// RendererAtmosphericSky.cpp — Atmospheric sky dome rendering integration.
//
// Implements the renderer-side logic for the atmospheric scattering sky dome:
//   - Initialization of constant buffer
//   - Per-frame CB update using lens flare sun direction/color
//   - Day/night blend computation
//   - Starfield visibility computation
//   - Fullscreen draw call
//
// Called from DrawHorizonAndSky() in RendererDraw.cpp.
// ============================================================================

#include "framework.h"
#include "Renderer/Renderer.h"

#include "Renderer/ConstantBuffers/AtmosphericSkyBuffer.h"
#include "Renderer/AtmosphericSky/AtmosphericSkySettings.h"
#include "Renderer/Moon/MoonSettings.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Scripting/Include/ScriptInterfaceLevel.h"
#include "Scripting/Internal/TEN/Flow/Level/FlowLevel.h"
#include "Specific/level.h"

using namespace TEN::Renderer::ConstantBuffers;

namespace TEN::Renderer
{
	// Helper: clamp to [0,1].
	static float Saturate(float x) { return std::clamp(x, 0.0f, 1.0f); }
	// ========================================================================
	// Initialization
	// ========================================================================

	void Renderer::InitializeAtmosphericSky()
	{
		_cbAtmosphericSky = ConstantBuffer<CAtmosphericSkyBuffer>(_device.Get());
	}

	// ========================================================================
	// Day/night blend computation
	// ========================================================================

	float Renderer::ComputeDayNightBlend(float sunElevation) const
	{
		// sunElevation = sin(pitch): +1 = zenith, 0 = horizon, -1 = nadir.
		// Twilight starts at the TwilightOffset above horizon.
		// Full night when sun is well below horizon.
		float twilightStart = _atmosphericSkySettings.TwilightOffset;
		float nightSpeed    = _atmosphericSkySettings.NightBlendSpeed;

		// Blend factor: 0 at twilightStart, 1 when sun is sufficiently below horizon.
		float nightFactor = Saturate((-sunElevation + twilightStart) * nightSpeed);
		// Smooth the transition.
		nightFactor = nightFactor * nightFactor * (3.0f - 2.0f * nightFactor);

		return nightFactor;
	}

	float Renderer::ComputeStarfieldVisibility(float sunElevation) const
	{
		// Stars fade in at the same time as the moon — using identical thresholds
		// so they appear together during twilight.
		float twilightStart = _atmosphericSkySettings.TwilightOffset * 0.25f;
		float starFade = Saturate((-sunElevation + twilightStart) * 3.0f);
		starFade = starFade * starFade * (3.0f - 2.0f * starFade); // smoothstep
		return starFade;
	}

	// ========================================================================
	// Moon phase computation
	// ========================================================================

	float Renderer::ComputeMoonPhase(const Vector3& sunDir, const Vector3& moonDir) const
	{
		// Phase is based on the angular relationship between sun and moon.
		// When the moon is opposite the sun (dot = -1), the sun fully illuminates
		// the moon face toward the viewer → full moon.
		// When the moon is in the same direction as the sun (dot = +1),
		// the sun illuminates the far side → new moon.
		//
		// phase = 0.5 * (1 - dot(sunDir, moonDir))
		//   dot = -1 → phase = 1.0 (full moon)
		//   dot =  0 → phase = 0.5 (half moon / quarter)
		//   dot = +1 → phase = 0.0 (new moon)
		float d = sunDir.Dot(moonDir);
		return 0.5f * (1.0f - d);
	}

	float Renderer::ComputeMoonVisibility(float sunElevation) const
	{
		// Moon becomes visible as the sky darkens.
		// Starts fading in during twilight, fully visible at night.
		// Uses a slightly earlier threshold than starfield so the moon
		// appears before the stars.
		float twilightStart = _atmosphericSkySettings.TwilightOffset * 0.25f;
		float moonFade = Saturate((-sunElevation + twilightStart) * 3.0f);
		moonFade = moonFade * moonFade * (3.0f - 2.0f * moonFade); // smoothstep
		return moonFade;
	}

	// ========================================================================
	// Per-frame constant buffer update
	// ========================================================================

	void Renderer::UpdateAtmosphericSkyBuffer(RenderView& renderView)
	{
		const auto& settings = _atmosphericSkySettings;

		// --- Compute sun direction and color from lens flare system ---
		// TEN uses Y-down: negative Y = up. Default sun direction points upward.
		Vector3 sunDir(0.0f, -1.0f, 0.0f);  // Default: high sun (pointing up in Y-down space).
		Vector3 sunColor(1.0f, 0.95f, 0.85f);
		float   sunElevation = 1.0f;

		auto* levelPtr = g_GameFlow->GetLevel(CurrentLevel);
		if (levelPtr->GetLensFlareEnabled())
		{
			// GetLensFlarePitch/Yaw return TEN short angles (65536 = 360 degrees).
			constexpr float SHORT_TO_RAD = (DirectX::XM_2PI / 65536.0f);
			float pitch = (float)levelPtr->GetLensFlarePitch() * SHORT_TO_RAD;
			float yaw   = (float)levelPtr->GetLensFlareYaw()   * SHORT_TO_RAD;

			// Build direction in TEN Y-down space: negate Y so that a sun
			// above the horizon (positive pitch) has negative Y (= up in TEN).
			sunDir = Vector3(
				std::cos(pitch) * std::sin(yaw),
				-std::sin(pitch),
				std::cos(pitch) * std::cos(yaw));
			sunDir.Normalize();

			// Sun elevation: positive = above horizon (used for day/night blend).
			sunElevation = std::sin(pitch);

			auto flareColor = levelPtr->GetLensFlareEvaluatedColor();
			sunColor = Vector3(flareColor.x, flareColor.y, flareColor.z);
		}

		float dayNightBlend     = ComputeDayNightBlend(sunElevation);
		float starfieldVis      = ComputeStarfieldVisibility(sunElevation);

		// --- Fill constant buffer ---
		_stAtmosphericSky.SunDirection    = sunDir;
		_stAtmosphericSky.SunElevation    = sunElevation;

		_stAtmosphericSky.SunColor        = sunColor;
		_stAtmosphericSky.DayNightBlend   = dayNightBlend;

		_stAtmosphericSky.SkyColor        = Vector3(settings.SkyColorR, settings.SkyColorG, settings.SkyColorB);
		_stAtmosphericSky.Density         = settings.Density;

		_stAtmosphericSky.ZenithOffset          = settings.ZenithOffset;
		_stAtmosphericSky.MultiScatterPhase     = settings.MultiScatterPhase;
		_stAtmosphericSky.AnisotropicIntensity  = settings.AnisotropicIntensity;
		_stAtmosphericSky.MieIntensity          = settings.MieIntensity;

		_stAtmosphericSky.RayleighIntensity     = settings.RayleighIntensity;
		_stAtmosphericSky.SunGlowIntensity      = settings.SunGlowIntensity;
		_stAtmosphericSky.HorizonDarkeningStr   = settings.HorizonDarkeningStr;
		_stAtmosphericSky.ExposureMultiplier    = settings.ExposureMultiplier;

		_stAtmosphericSky.NightSkyBrightness    = settings.NightSkyBrightness;
		_stAtmosphericSky.StarfieldVisibility   = starfieldVis;
		_stAtmosphericSky.TwilightOffset        = settings.TwilightOffset;
		_stAtmosphericSky.NightBlendSpeed       = settings.NightBlendSpeed;

		_stAtmosphericSky.ViewSize     = Vector2((float)_screenWidth, (float)_screenHeight);
		_stAtmosphericSky.InvViewSize  = Vector2(1.0f / (float)_screenWidth, 1.0f / (float)_screenHeight);

		_stAtmosphericSky.SunElevationRampSpeed = settings.SunElevationRampSpeed;
		_stAtmosphericSky.SunWarmInfluence      = settings.SunWarmInfluence;

		// Pre-compute cos(half_angle) on CPU to avoid a cos() call per pixel in the shader.
		_stAtmosphericSky.SunDiskCosRadius  = std::cos(settings.SunDiskSize * (DirectX::XM_PI / 180.0f));
		_stAtmosphericSky.SunDiskIntensity  = settings.SunDiskIntensity;

		// --- Moon data ---
		const auto& moon = _moonSettings;

		// Build moon direction from pitch/yaw (same convention as sun).
		float moonPitchRad = moon.Pitch * (DirectX::XM_PI / 180.0f);
		float moonYawRad   = moon.Yaw   * (DirectX::XM_PI / 180.0f);
		Vector3 moonDir(
			std::cos(moonPitchRad) * std::sin(moonYawRad),
			-std::sin(moonPitchRad),
			std::cos(moonPitchRad) * std::cos(moonYawRad));
		moonDir.Normalize();

		float moonElevation = std::sin(moonPitchRad);

		// Moon phase from sun-moon angular relationship.
		float moonPhase = ComputeMoonPhase(sunDir, moonDir);

		// Phase brightness: full moon (phase ~1.0) = bright, new moon (phase ~0.0) = dark.
		// Use a smoothed curve so quarter moons are dimmer than expected.
		float phaseBrightness = moonPhase * moonPhase * (3.0f - 2.0f * moonPhase);

		// Moon visibility: fades in as the sky darkens.
		float moonVisibility = moon.Enabled ? ComputeMoonVisibility(sunElevation) : 0.0f;

		// Moon color: base color tinted by sun illumination.
		// The lit side takes on a slight warm tint from the sun color.
		float sunTint = 0.15f * phaseBrightness; // subtle sun coloring
		Vector3 moonColor(
			moon.BaseColorR + sunColor.x * sunTint,
			moon.BaseColorG + sunColor.y * sunTint,
			moon.BaseColorB + sunColor.z * sunTint);

		_stAtmosphericSky.MoonDirection      = moonDir;
		_stAtmosphericSky.MoonElevation      = moonElevation;
		_stAtmosphericSky.MoonColor          = moonColor;
		_stAtmosphericSky.MoonPhase          = moonPhase;
		_stAtmosphericSky.MoonDiskCosRadius  = std::cos(moon.DiskSize * (DirectX::XM_PI / 180.0f));
		_stAtmosphericSky.MoonDiskIntensity  = moon.DiskIntensity;
		_stAtmosphericSky.MoonGlowIntensity  = moon.GlowIntensity;
		_stAtmosphericSky.MoonGlowFalloff    = moon.GlowFalloff;
		_stAtmosphericSky.MoonEnabled        = moon.Enabled ? 1.0f : 0.0f;
		_stAtmosphericSky.MoonPhaseBrightness = phaseBrightness;
		_stAtmosphericSky.MoonVisibility     = moonVisibility;
		_stAtmosphericSky.MoonPad0           = 0.0f;

		UpdateConstantBuffer(_stAtmosphericSky, _cbAtmosphericSky);
	}

	// ========================================================================
	// Draw atmospheric sky dome
	// ========================================================================

	void Renderer::DrawAtmosphericSkyDome(RenderView& renderView)
	{
		if (!_atmosphericSkySettings.Enabled)
			return;

		// Update constant buffer with current sun/sky state.
		UpdateAtmosphericSkyBuffer(renderView);

		// Bind atmospheric sky CB to register b10 (shared with HUD — different render pass).
		auto* buf = _cbAtmosphericSky.get();
		_context->PSSetConstantBuffers(10, 1, buf);
		_context->VSSetConstantBuffers(10, 1, buf);

		// Set up fullscreen triangle rendering.
		SetBlendMode(BlendMode::Opaque);
		SetCullMode(CullMode::CounterClockwise);
		SetDepthState(DepthState::None);

		_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		_context->IASetInputLayout(_fullscreenTriangleInputLayout.Get());

		unsigned int stride = sizeof(PostProcessVertex);
		unsigned int offset = 0;
		_context->IASetVertexBuffers(0, 1,
			_fullscreenTriangleVertexBuffer.Buffer.GetAddressOf(), &stride, &offset);

		// Bind and draw.
		_shaders.Bind(Shader::AtmosphericSkyDome);
		DrawTriangles(3, 0);

		// Restore regular input layout.
		_context->IASetInputLayout(_inputLayout.Get());
		_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}
}
