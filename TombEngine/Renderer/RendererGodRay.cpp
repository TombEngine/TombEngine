// ============================================================================
// RendererGodRay.cpp — Lightweight screen-space god ray rendering integration.
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
#include "Renderer/GodRay/GodRaySettings.h"
#include "Renderer/Moon/MoonSettings.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Scripting/Include/ScriptInterfaceLevel.h"
#include "Scripting/Internal/TEN/Flow/Level/FlowLevel.h"
#include "Specific/level.h"

using namespace TEN::Sky;
using namespace TEN::Renderer::ConstantBuffers;
using namespace TEN::Renderer::GodRay;

namespace TEN::Renderer
{
	// God ray half-res scale.  0.5 = quarter pixel count.
	static constexpr float GOD_RAY_RESOLUTION_SCALE = 0.5f;

	// Helper: clamp to [0,1].
	static float Saturate(float x) { return std::clamp(x, 0.0f, 1.0f); }

	// ========================================================================
	// Initialization
	// ========================================================================

	void Renderer::InitializeGodRays()
	{
		_cbGodRay = ConstantBuffer<CGodRayBuffer>(_device.Get());

		int w = std::max(1, (int)(_screenWidth  * GOD_RAY_RESOLUTION_SCALE));
		int h = std::max(1, (int)(_screenHeight * GOD_RAY_RESOLUTION_SCALE));

		_godRayRenderTarget = RenderTarget2D(
			_device.Get(), w, h,
			DXGI_FORMAT_R11G11B10_FLOAT,   // Lightweight HDR, no alpha needed.
			false,
			DXGI_FORMAT_UNKNOWN);          // No depth buffer.
	}

	// ========================================================================
	// Auto-strength computation
	// ========================================================================

	static float ComputeGodRayAutoStrength(float sunElevation, float cloudCoverage)
	{
		// Elevation factor: slightly stronger when sun is low/near horizon.
		// No hard cut-off — sunset is the MOST dramatic time for god rays.
		float elevFactor = std::max(1.0f - sunElevation * 0.6f, 0.3f);

		// Cloud coverage factor: ramps from 0 (no clouds) to 1.0 at ~33% coverage.
		float coverageFactor = Saturate(cloudCoverage * 3.0f);

		return elevFactor * coverageFactor;
	}

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
			constexpr float SHORT_TO_RAD = (DirectX::XM_2PI / 65536.0f);
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
			float moonPitchRad = moon.Pitch * (DirectX::XM_PI / 180.0f);
			float moonYawRad   = moon.Yaw   * (DirectX::XM_PI / 180.0f);
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
				// Cap at ±10 NDC to avoid numeric issues with near-clipped positions.
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
		float rayFacingFade = Saturate((rayFacingDot - (-0.25f)) / (0.15f - (-0.25f)));
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
		else
		{
			const auto* activeSettings = GetActiveVolumetricCloudSettings();
			if (activeSettings && activeSettings->Enabled)
				cloudCoverage = activeSettings->Coverage;
		}

		// --- Auto-strength ---
		float rayElevation = useMoonRays ? std::sin(moon.Pitch * (DirectX::XM_PI / 180.0f)) : sunElevation;
		float autoStrength = ComputeGodRayAutoStrength(rayElevation, cloudCoverage);

		// Fade: for sun rays, fade out when sun is below horizon.
		// For moon rays, fade out when moon is below horizon.
		float belowHorizonFade;
		if (useMoonRays)
		{
			float moonElev = std::sin(moon.Pitch * (DirectX::XM_PI / 180.0f));
			belowHorizonFade = std::clamp(1.0f + moonElev * 8.0f, 0.0f, 1.0f);
		}
		else
		{
			belowHorizonFade = std::clamp(1.0f + sunElevation * 8.0f, 0.0f, 1.0f);
		}
		belowHorizonFade = belowHorizonFade * belowHorizonFade * (3.0f - 2.0f * belowHorizonFade);

		float finalAutoStrength = (1.0f + (autoStrength - 1.0f) * rayAutoMix) * rayFacingFade * belowHorizonFade;

		// --- Fill constant buffer ---
		_stGodRay.SunScreenPos  = rayScreenUV;
		_stGodRay.RayLength     = rayLength;
		_stGodRay.Intensity     = rayIntensity;

		_stGodRay.Decay         = rayDecay;
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

		UpdateConstantBuffer(_stGodRay, _cbGodRay);
	}

	// ========================================================================
	// Draw god rays
	// ========================================================================

	void Renderer::DrawGodRays(RenderView& renderView)
	{
		// Check if any ray source is enabled.
		float dayNightBlend = 0.0f;
		{
			auto* levelPtr = g_GameFlow->GetLevel(CurrentLevel);
			float sunElev = 1.0f;
			if (levelPtr->GetLensFlareEnabled())
			{
				constexpr float SHORT_TO_RAD = (DirectX::XM_2PI / 65536.0f);
				float pitch = (float)levelPtr->GetLensFlarePitch() * SHORT_TO_RAD;
				sunElev = std::sin(pitch);
			}
			dayNightBlend = ComputeDayNightBlend(sunElev);
		}
		bool useMoonRays = _moonSettings.Enabled && _moonSettings.GodRays.Enabled && dayNightBlend > 0.5f;

		if (useMoonRays)
		{
			if (!_moonSettings.GodRays.Enabled)
				return;
		}
		else
		{
			if (!_godRaySettings.Enabled)
				return;
		}

		// Require volumetric clouds to provide the occlusion mask.
		// Aurora-category layers are pure light effects, not cloud geometry — exclude them
		// so they contribute neither an occlusion mask nor auto-strength for god rays.
		bool hasClouds = false;
		if (g_SkyCloudSystem.IsCloudAActive() || g_SkyCloudSystem.IsCloudBActive())
		{
			// Only count as "real clouds" if at least one layer is not the Aurora category.
			const int auroraType = (int)CloudCategory::Aurora;
			bool aOnly = !g_SkyCloudSystem.IsCloudAActive() || g_SkyCloudSystem.GetCloudARenderSettings().CloudType == auroraType;
			bool bOnly = !g_SkyCloudSystem.IsCloudBActive() || g_SkyCloudSystem.GetCloudBRenderSettings().CloudType == auroraType;
			if (!(aOnly && bOnly))
				hasClouds = true;
		}
		else
		{
			const auto* activeSettings = GetActiveVolumetricCloudSettings();
			if (activeSettings && activeSettings->Enabled)
				hasClouds = true;
		}

		if (!hasClouds)
			return;

		// Update the constant buffer.
		UpdateGodRayBuffer(renderView);

		// Skip only if the sun is behind the camera (sentinel -10).
		// Off-screen but in-front suns still produce rays from the screen edge.
		if (_stGodRay.SunScreenPos.x < -5.0f)
			return;

		// --- Pass 1: Render god rays to half-res target ---
		float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		_context->ClearRenderTargetView(_godRayRenderTarget.RenderTargetView.Get(), clearColor);
		_context->OMSetRenderTargets(1, _godRayRenderTarget.RenderTargetView.GetAddressOf(), nullptr);

		int grW = std::max(1, (int)(_screenWidth  * GOD_RAY_RESOLUTION_SCALE));
		int grH = std::max(1, (int)(_screenHeight * GOD_RAY_RESOLUTION_SCALE));

		D3D11_VIEWPORT godRayViewport = {};
		godRayViewport.Width    = (float)grW;
		godRayViewport.Height   = (float)grH;
		godRayViewport.MinDepth = 0.0f;
		godRayViewport.MaxDepth = 1.0f;
		_context->RSSetViewports(1, &godRayViewport);

		// Bind god ray CB to b10 (reuses ATmosphericSky/Hud slot; safe at this pipeline stage).
		auto* buf = _cbGodRay.get();
		_context->PSSetConstantBuffers(10, 1, buf);
		_context->VSSetConstantBuffers(10, 1, buf);

		// Bind cloud render targets as t0 (layer A) and t1 (layer B).
		// Both layers are sampled; the shader takes max(alphaA, alphaB) for the occlusion mask.
		BindRenderTargetAsTexture(TextureRegister::ColorMap, &_cloudRenderTarget,
			SamplerStateRegister::LinearClamp);
		BindRenderTargetAsTexture(TextureRegister::NormalMap, &_cloudRenderTargetB,
			SamplerStateRegister::LinearClamp);

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

		_shaders.Bind(Shader::GodRay);
		DrawTriangles(3, 0);

		// --- Pass 2: Additively composite half-res god rays over the main scene ---
		_context->RSSetViewports(1, &renderView.Viewport);
		_context->OMSetRenderTargets(1, _renderTarget.RenderTargetView.GetAddressOf(),
			_renderTarget.DepthStencilView.Get());

		SetBlendMode(BlendMode::Additive);

		BindRenderTargetAsTexture(TextureRegister::ColorMap, &_godRayRenderTarget,
			SamplerStateRegister::LinearClamp);

		_shaders.Bind(Shader::GodRayComposite);
		DrawTriangles(3, 0);

		// --- Cleanup ---
		_context->IASetInputLayout(_inputLayout.Get());
		_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		ID3D11ShaderResourceView* nullSRVs[2] = { nullptr, nullptr };
		_context->PSSetShaderResources((UINT)TextureRegister::ColorMap, 2, nullSRVs);

		SetBlendMode(BlendMode::Opaque);
		SetDepthState(DepthState::Write);
		SetCullMode(CullMode::CounterClockwise);

		_context->RSSetViewports(1, &renderView.Viewport);
		_context->OMSetRenderTargets(1, _renderTarget.RenderTargetView.GetAddressOf(),
			_renderTarget.DepthStencilView.Get());
	}
}
