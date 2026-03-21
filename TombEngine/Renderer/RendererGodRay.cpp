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

		// Project sun to screen UV: prefer lens flare world position, fall back to virtual sun.
		// Sentinel -10 means "no sun direction available at all" — rays fully suppressed.
		// Off-screen or behind-camera suns get properly projected UVs; intensity fades via
		// sunFacingFade (see below) so the rays gracefully disappear as you turn away.
		if (!renderView.LensFlaresToDraw.empty() && renderView.LensFlaresToDraw[0].IsGlobal)
		{
			const auto& sunPos = renderView.LensFlaresToDraw[0].Position;
			auto clip = Vector4::Transform(
				Vector4(sunPos.x, sunPos.y, sunPos.z, 1.0f),
				renderView.Camera.ViewProjection);
			float absW = std::abs(clip.w);
			if (absW > 0.0001f)
			{
				// Divide by |w| WITHOUT a sign flip so the UV stays on the same side
				// of the screen even when the sun crosses behind the camera plane.
				// (Standard clip.x/clip.w would mirror the UV to the opposite edge.)
				float ndcX = std::clamp(clip.x / absW, -8.0f, 8.0f);
				float ndcY = std::clamp(clip.y / absW, -8.0f, 8.0f);
				sunScreenUV = Vector2(ndcX * 0.5f + 0.5f, ndcY * -0.5f + 0.5f);
			}
		}

		// Fallback: project a virtual sun far along sunDir from the camera.
		if (sunScreenUV.x < -5.0f && sunDir.LengthSquared() > 0.001f)
		{
			constexpr float VIRTUAL_SUN_DIST = 500000.0f;
			Vector3 virtualSunPos = renderView.Camera.WorldPosition + sunDir * VIRTUAL_SUN_DIST;
			auto clip = Vector4::Transform(
				Vector4(virtualSunPos.x, virtualSunPos.y, virtualSunPos.z, 1.0f),
				renderView.Camera.ViewProjection);
			float absW = std::abs(clip.w);
			if (absW > 0.0001f)
			{
				float ndcX = std::clamp(clip.x / absW, -8.0f, 8.0f);
				float ndcY = std::clamp(clip.y / absW, -8.0f, 8.0f);
				sunScreenUV = Vector2(ndcX * 0.5f + 0.5f, ndcY * -0.5f + 0.5f);
			}
		}

		// Sun-facing fade: smoothly mute rays as the camera turns away from the sun.
		// Uses the dot product between sun direction and camera look direction.
		//   dot =  1 : sun directly ahead       → fully visible
		//   dot =  0 : sun exactly 90° to side  → ~50% visible (rays come from edge)
		//   dot = -0.25 : sun 104° off-axis     → fully gone
		float sunFacingDot  = sunDir.Dot(renderView.Camera.WorldDirection);
		float sunFacingFade = Saturate((sunFacingDot - (-0.25f)) / (0.15f - (-0.25f)));
		// Smooth the fade curve (smoothstep equivalent).
		sunFacingFade = sunFacingFade * sunFacingFade * (3.0f - 2.0f * sunFacingFade);

		// --- Cloud coverage for auto-strength ---
		// Use scene-wide Coverage from cloud settings — NOT sun-point transmittance,
		// which would be ~0 whenever the sun shines through a gap (exactly when you'd want rays).
		float cloudCoverage = 0.0f;
		if (g_SkyCloudSystem.IsCloudAActive() || g_SkyCloudSystem.IsCloudBActive())
		{
			float coverageA = 0.0f, coverageB = 0.0f;
			if (g_SkyCloudSystem.IsCloudAActive())
				coverageA = g_SkyCloudSystem.GetCloudARenderSettings().Coverage;
			if (g_SkyCloudSystem.IsCloudBActive())
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
		float autoStrength = ComputeGodRayAutoStrength(sunElevation, cloudCoverage);
		float finalAutoStrength = (1.0f + (autoStrength - 1.0f) * settings.AutoStrengthMix) * sunFacingFade;

		// --- Fill constant buffer ---
		_stGodRay.SunScreenPos  = sunScreenUV;
		_stGodRay.RayLength     = settings.Length;
		_stGodRay.Intensity     = settings.Intensity;

		_stGodRay.Decay         = settings.Decay;
		_stGodRay.SampleCount   = settings.SampleCount;
		_stGodRay.SunElevation  = sunElevation;
		_stGodRay.AutoStrength  = finalAutoStrength;

		// Apply atmospheric sky gradient so god ray tint matches the sky dome.
		{
			const auto& atmo = _atmosphericSkySettings;
			float sunInfl = std::max(0.0f, 1.0f - sunElevation * atmo.SunElevationRampSpeed);
			float blend   = sunInfl * atmo.SunWarmInfluence;
			sunColor.x = 1.0f + (sunColor.x - 1.0f) * blend;
			sunColor.y = 1.0f + (sunColor.y - 1.0f) * blend;
			sunColor.z = 1.0f + (sunColor.z - 1.0f) * blend;
		}
		_stGodRay.SunColor      = sunColor;
		_stGodRay.Softness      = settings.Softness;

		_stGodRay.ViewSize      = Vector2((float)_screenWidth, (float)_screenHeight);
		_stGodRay.InvViewSize   = Vector2(1.0f / (float)_screenWidth, 1.0f / (float)_screenHeight);

		UpdateConstantBuffer(_stGodRay, _cbGodRay);
	}

	// ========================================================================
	// Draw god rays
	// ========================================================================

	void Renderer::DrawGodRays(RenderView& renderView)
	{
		if (!_godRaySettings.Enabled)
			return;

		// Require volumetric clouds to provide the occlusion mask.
		bool hasClouds = false;
		if (g_SkyCloudSystem.IsCloudAActive() || g_SkyCloudSystem.IsCloudBActive())
			hasClouds = true;
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
