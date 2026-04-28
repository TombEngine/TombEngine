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

#include "Game/Sky/SkyCloudSystem.h"
#include "Renderer/ConstantBuffers/AtmosphericSkyBuffer.h"
#include "Renderer/AtmosphericSky/AtmosphericSkySettings.h"
#include "Renderer/Aurora/AuroraSettings.h"
#include "Renderer/Moon/MoonSettings.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Scripting/Include/ScriptInterfaceLevel.h"
#include "Scripting/Internal/TEN/Flow/Level/FlowLevel.h"
#include "Specific/level.h"

using namespace TEN::Renderer::ConstantBuffers;
using namespace TEN::Sky;

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
		float twilightStart = _atmosphericSkySettings.TwilightOffset * 1.5f;
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

		float dayNightBlend = ComputeDayNightBlend(sunElevation);
		float starfieldVis  = ComputeStarfieldVisibility(sunElevation);

		// Inform the sky cloud system of the current day/night state so that
		// probabilistic next-preset chains can blend between day and night weights.
		g_SkyCloudSystem.SetNightBlend(starfieldVis);

		// --- Pre-compute moon data ---
		// Done before the CB fill so phaseBrightness is available to modulate NightSkyBrightness.
		const auto& moon  = _moonSettings;
		float moonPitchRad    = moon.Pitch * (DirectX::XM_PI / 180.0f);
		float moonYawRad      = moon.Yaw   * (DirectX::XM_PI / 180.0f);
		Vector3 moonDir(
			std::cos(moonPitchRad) * std::sin(moonYawRad),
			-std::sin(moonPitchRad),
			std::cos(moonPitchRad) * std::cos(moonYawRad));
		moonDir.Normalize();
		float moonElevation   = std::sin(moonPitchRad);
		float moonPhase       = ComputeMoonPhase(sunDir, moonDir);
		// Phase brightness: full moon (phase~1) = 1, new moon (phase~0) = 0 (smoothstep).
		float phaseBrightness = moonPhase * moonPhase * (3.0f - 2.0f * moonPhase);
		float moonVisibility  = moon.Enabled ? ComputeMoonVisibility(sunElevation) : 0.0f;
		// Moon color: base color tinted slightly by sun warmth on the lit side.
		float sunTint = 0.15f * phaseBrightness;
		Vector3 moonColor(
			moon.BaseColorR + sunColor.x * sunTint,
			moon.BaseColorG + sunColor.y * sunTint,
			moon.BaseColorB + sunColor.z * sunTint);

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

		// NightSkyBrightness scales with moon phase:
		//   full moon  (phaseBrightness = 1) → settings.NightSkyBrightness     (max)
		//   new moon   (phaseBrightness = 0) → 5% of settings value           (minimal ambient)
		float nightBrightness = moon.Enabled
			? settings.NightSkyBrightness * (0.05f + 0.95f * phaseBrightness)
			: settings.NightSkyBrightness;
		_stAtmosphericSky.NightSkyBrightness    = nightBrightness;
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

		// --- Moon CB fill ---
		// (direction, phase, visibility, and color are all pre-computed above)
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

		// --- Aurora CB fill ---
		auto& aurora = _auroraSettings;

		// Drive aurora.Enabled from the sky cloud system's live snapshot.
		// This ensures aurora rendering responds to preset selection outside the debug menu.
		bool wantsAurora = g_SkyCloudSystem.IsAuroraPresetActive();

		// Advance preset fade toward target (linear, then smoothstepped for output).
		{
			float fadeTarget = wantsAurora ? 1.0f : 0.0f;
			float fadeStep   = (_auroraPresetFadeDuration > 0.0f)
				? (1.0f / 30.0f) / _auroraPresetFadeDuration
				: 1.0f;
			if (_auroraPresetFade < fadeTarget)
				_auroraPresetFade = Saturate(_auroraPresetFade + fadeStep);
			else if (_auroraPresetFade > fadeTarget)
				_auroraPresetFade = Saturate(_auroraPresetFade - fadeStep);
		}

		// Keep aurora enabled while the fade is still visible (allows fade-out to complete).
		aurora.Enabled = wantsAurora || (_auroraPresetFade > 0.001f);

		// Aurora visibility: computed from sun elevation, similar to starfield.
		// Fully visible at night, fades out during twilight, invisible during day.
		float auroraVisibility = 0.0f;
		if (aurora.Enabled)
		{
			float auroraFade = Saturate((-sunElevation + aurora.NightFadeThreshold) * aurora.SunSuppressionStr);
			auroraFade = auroraFade * auroraFade * (3.0f - 2.0f * auroraFade); // smoothstep
			auroraVisibility = auroraFade;

			// Apply preset fade (smoothstepped) as an additional multiplier.
			float presetFadeSmooth = _auroraPresetFade * _auroraPresetFade * (3.0f - 2.0f * _auroraPresetFade);
			auroraVisibility *= presetFadeSmooth;

			// Accumulate animation time.
			_auroraTime += 1.0f / 30.0f; // Approximate frame time; consistent drift.
		}

		_stAtmosphericSky.AuroraEnabled          = aurora.Enabled ? 1.0f : 0.0f;
		_stAtmosphericSky.AuroraIntensity         = aurora.Intensity;
		_stAtmosphericSky.AuroraBrightness        = aurora.Brightness;
		_stAtmosphericSky.AuroraHeight            = aurora.Height;
		_stAtmosphericSky.AuroraSpread            = aurora.Spread;
		_stAtmosphericSky.AuroraSpeed             = aurora.Speed;
		_stAtmosphericSky.AuroraBandSharpness     = aurora.BandSharpness;
		_stAtmosphericSky.AuroraNoiseScale        = aurora.NoiseScale;
		_stAtmosphericSky.AuroraVerticalStretch   = aurora.VerticalStretch;
		_stAtmosphericSky.AuroraDistortionStr     = aurora.DistortionStrength;
		_stAtmosphericSky.AuroraLayerCount        = (float)aurora.LayerCount;
		_stAtmosphericSky.AuroraSoftness          = aurora.Softness;
		_stAtmosphericSky.AuroraColorPreset       = (float)aurora.ColorPreset;
		_stAtmosphericSky.AuroraColorIntensity    = aurora.ColorIntensity;
		_stAtmosphericSky.AuroraSaturation        = aurora.Saturation;
		_stAtmosphericSky.AuroraVisibility        = auroraVisibility;
		_stAtmosphericSky.AuroraNightFadeThreshold = aurora.NightFadeThreshold;
		_stAtmosphericSky.AuroraHorizonFade       = aurora.HorizonFade;
		_stAtmosphericSky.AuroraSunSuppressionStr = aurora.SunSuppressionStr;
		_stAtmosphericSky.AuroraTime              = _auroraTime;

		// Cloud disc occlusion: 1 - transmittance, clamped. Suppresses sun disc in shader
		// when clouds cover the sun position (prev-frame readback, 1-frame latency is fine).
		// Use combined dual-layer transmittance (Beer-Lambert product of both layers).
		float cloudTransmittance = g_SkyCloudSystem.GetCombinedCloudTransmittance();
		_stAtmosphericSky.CloudDiscOcclusion = 1.0f - cloudTransmittance;

		// --- Horizon ground color ---
		_stAtmosphericSky.HorizonColorR = settings.HorizonColorR;
		_stAtmosphericSky.HorizonColorG = settings.HorizonColorG;
		_stAtmosphericSky.HorizonColorB = settings.HorizonColorB;

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

	// ========================================================================
	// Draw aurora — separate additive pass, independent of sky dome
	// ========================================================================

	void Renderer::DrawAurora(RenderView& renderView)
	{
		const auto& aurora = _auroraSettings;
		if (!aurora.Enabled)
			return;

		// If the sky dome is disabled it won't have called UpdateAtmosphericSkyBuffer,
		// so update the CB here to ensure aurora parameters are current.
		if (!_atmosphericSkySettings.Enabled)
			UpdateAtmosphericSkyBuffer(renderView);

		if (_stAtmosphericSky.AuroraVisibility < 0.001f)
			return;

		// Bind CB to b10 (same slot used by sky dome).
		auto* buf = _cbAtmosphericSky.get();
		_context->PSSetConstantBuffers(10, 1, buf);
		_context->VSSetConstantBuffers(10, 1, buf);

		// Bind cloud coverage for aurora occlusion.
		// When the atmospheric sky pass has run, DrawSunMoonDisc has already copied
		// the composited scene (with cloud coverage in its alpha channel) into
		// _scenePreCloudBackup. When the atmospheric sky is disabled, that copy has
		// not been made yet, so we do it here before binding.
		if (!_atmosphericSkySettings.Enabled)
			_context->CopyResource(_scenePreCloudBackup.Texture.Get(), _renderTarget.Texture.Get());

		BindRenderTargetAsTexture(TextureRegister::ColorMap, &_scenePreCloudBackup,
			SamplerStateRegister::LinearClamp);

		// Render additively on top of whatever sky is below.
		// DrawAurora is called before the horizon mesh draw loop, so opaque horizon
		// geometry naturally overwrites aurora pixels — no depth test needed.
		SetBlendMode(BlendMode::Additive);
		SetCullMode(CullMode::CounterClockwise);
		SetDepthState(DepthState::None);

		_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		_context->IASetInputLayout(_fullscreenTriangleInputLayout.Get());

		unsigned int stride = sizeof(PostProcessVertex);
		unsigned int offset = 0;
		_context->IASetVertexBuffers(0, 1,
			_fullscreenTriangleVertexBuffer.Buffer.GetAddressOf(), &stride, &offset);

		_shaders.Bind(Shader::Aurora);
		DrawTriangles(3, 0);

		// Unbind the SRV so _renderTarget can be bound as RT again without conflicts.
		ID3D11ShaderResourceView* nullSRV = nullptr;
		_context->PSSetShaderResources((UINT)TextureRegister::ColorMap, 1, &nullSRV);

		// Restore regular input layout.
		_context->IASetInputLayout(_inputLayout.Get());
		_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}

	// ========================================================================
	// Draw sun/moon disc — separate additive pass AFTER cloud compositing
	// ========================================================================
	// The cloud compositor writes cloud coverage into the alpha channel of
	// _renderTarget. This pass reads that alpha to mask sun and moon discs
	// so that clouds naturally occlude them without blend-mode hacks.

	void Renderer::DrawSunMoonDisc(RenderView& renderView)
	{
		if (!_atmosphericSkySettings.Enabled)
			return;

		// Bind atmospheric sky CB to register b10.
		auto* buf = _cbAtmosphericSky.get();
		_context->PSSetConstantBuffers(10, 1, buf);
		_context->VSSetConstantBuffers(10, 1, buf);

		// Copy current _renderTarget to _scenePreCloudBackup so we can read
		// the scene (including cloud coverage alpha) as a texture input.
		_context->CopyResource(_scenePreCloudBackup.Texture.Get(),
			_renderTarget.Texture.Get());

		// Bind the copy as t0 (ColorMap) for the shader to read cloud coverage.
		BindRenderTargetAsTexture(TextureRegister::ColorMap, &_scenePreCloudBackup,
			SamplerStateRegister::LinearClamp);

		// Render target: _renderTarget (additive on top of the composited scene).
		_context->OMSetRenderTargets(1, _renderTarget.RenderTargetView.GetAddressOf(),
			_renderTarget.DepthStencilView.Get());

		// Additive blend: sun/moon disc light is added on top.
		SetBlendMode(BlendMode::Additive);
		SetCullMode(CullMode::CounterClockwise);
		SetDepthState(DepthState::None);

		// Fullscreen triangle rendering.
		_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		_context->IASetInputLayout(_fullscreenTriangleInputLayout.Get());

		unsigned int stride = sizeof(PostProcessVertex);
		unsigned int offset = 0;
		_context->IASetVertexBuffers(0, 1,
			_fullscreenTriangleVertexBuffer.Buffer.GetAddressOf(), &stride, &offset);

		_shaders.Bind(Shader::SunMoonDisc);
		DrawTriangles(3, 0);

		// Unbind the SRV so _renderTarget can be used as RT again.
		ID3D11ShaderResourceView* nullSRV = nullptr;
		_context->PSSetShaderResources((UINT)TextureRegister::ColorMap, 1, &nullSRV);

		// Restore regular input layout.
		_context->IASetInputLayout(_inputLayout.Get());
		_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}
}
