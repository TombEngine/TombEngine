// ============================================================================
// RendererDualClouds.cpp — Dual volumetric cloud layer rendering integration
//
// This file provides the renderer-side entry points for the new layered
// sky/cloud system.  It is called from the main render pipeline after the
// legacy DrawHorizonAndSky() pass.
//
// Rendering order:
//   1. Legacy SkyLayer1  (bitmap, additive)           — DrawHorizonAndSky()
//   2. Legacy SkyLayer2  (bitmap, additive)           — DrawHorizonAndSky()
//   3. Volumetric Cloud Layer A (high/thin, alpha)    — DrawDualVolumetricClouds()
//   4. Volumetric Cloud Layer B (low/dense, alpha)    — DrawDualVolumetricClouds()
//
// The two volumetric passes reuse the same shader and constant buffer slot
// (register b9).  Each is rendered into its own half-res target, then
// composited back-to-front over the scene:
//   - Cloud A composited first (farther / higher)
//   - Cloud B composited second (closer / lower / denser)
//
// Lens flare occlusion is computed independently for each layer, and the
// combined transmittance is the product of both.
// ============================================================================

#include "framework.h"
#include "Renderer/Renderer.h"

#include <DirectXPackedVector.h>
#include "Game/control/control.h"
#include "Game/camera.h"
#include "Game/Sky/SkyCloudSystem.h"
#include "Renderer/VolumetricCloud/VolumetricCloud.h"
#include "Renderer/ConstantBuffers/VolumetricCloudBuffer.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Scripting/Include/ScriptInterfaceLevel.h"
#include "Scripting/Internal/TEN/Flow/Level/FlowLevel.h"
#include "Sound/sound.h"
#include "Sound/sound_effects.h"
#include "Specific/level.h"

using namespace TEN::Renderer::VolumetricCloud;
using namespace TEN::Renderer::ConstantBuffers;
using namespace TEN::Sky;

namespace TEN::Renderer
{
	// ========================================================================
	// Dual-layer cloud initialization
	// ========================================================================

	void Renderer::InitializeDualVolumetricClouds()
	{
		// CB is shared — already created by InitializeVolumetricClouds().
		// Create render targets for layer B.
		ResizeDualCloudTargets();
	}

	void Renderer::ResizeDualCloudTargets()
	{
		// Layer A targets (reuse existing _cloudRenderTarget / _cloudOcclusionTarget).

		// Layer B target — same format, same resolution as layer A.
		float scale = _cloudStateB.ActiveQuality.RenderResolutionScale;
		int w = std::max(1, (int)(_screenWidth * scale));
		int h = std::max(1, (int)(_screenHeight * scale));

		_cloudRenderTargetB = RenderTarget2D(
			_device.Get(), w, h,
			DXGI_FORMAT_R16G16B16A16_FLOAT,
			false,
			DXGI_FORMAT_UNKNOWN);

		// Previous-frame RT for temporal checkerboard on layer B.
		_cloudPrevFrameRTB = RenderTarget2D(
			_device.Get(), w, h,
			DXGI_FORMAT_R16G16B16A16_FLOAT,
			false,
			DXGI_FORMAT_UNKNOWN);

		_cloudOcclusionTargetB = RenderTarget2D(
			_device.Get(), 1, 1,
			DXGI_FORMAT_R16_FLOAT,
			false,
			DXGI_FORMAT_UNKNOWN);
	}

	// ========================================================================
	// Draw both volumetric cloud layers
	// ========================================================================

	void Renderer::DrawDualVolumetricClouds(RenderView& renderView)
	{
		// For active layers DrawSingleVolumetricCloudLayer handles its own clear AFTER
		// it copies the current RT to the prevFrameRT — order matters for temporal reprojection.
		// Pre-clearing before DrawSingle would make prevFrameRT = black → temporal pixels read black.
		// For INACTIVE layers we still clear here so the GodRay shader never reads stale cloud data.
		float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

		// Draw Cloud Layer A (higher / thinner — composited first = behind).
		if (g_SkyCloudSystem.IsCloudAActive())
		{
			auto settingsA = g_SkyCloudSystem.GetCloudARenderSettings();
			DrawSingleVolumetricCloudLayer(
				settingsA, _cloudState, _cloudRenderTarget, renderView, &_cloudPrevFrameRT);
		}
		else
		{
			_context->ClearRenderTargetView(_cloudRenderTarget.RenderTargetView.Get(), clearColor);
			// Reset temporal state so when the layer becomes active again it starts with
			// fresh history (TemporalEnabled = warmup) rather than stale old-preset data.
			_cloudState.FrameCounter = 0;
			_cloudState.PrevCloudType = -1;
		}

		// Draw Cloud Layer B (lower / denser — composited second = in front).
		if (g_SkyCloudSystem.IsCloudBActive())
		{
			auto settingsB = g_SkyCloudSystem.GetCloudBRenderSettings();
			DrawSingleVolumetricCloudLayer(
				settingsB, _cloudStateB, _cloudRenderTargetB, renderView, &_cloudPrevFrameRTB);
		}
		else
		{
			_context->ClearRenderTargetView(_cloudRenderTargetB.RenderTargetView.Get(), clearColor);
			// Reset temporal state so when the layer becomes active again it starts with
			// fresh history (TemporalEnabled = warmup) rather than stale old-preset data.
			_cloudStateB.FrameCounter = 0;
			_cloudStateB.PrevCloudType = -1;
		}

		// Update lens flare occlusion for both layers.
		UpdateDualCloudLensFlareOcclusion(renderView);
	}

	// ========================================================================
	// Draw a single volumetric cloud layer into its render target
	// ========================================================================

	void Renderer::DrawSingleVolumetricCloudLayer(
		const CloudRenderSettings& settings,
		CloudRuntimeState& state,
		RenderTarget2D& renderTarget,
		RenderView& renderView,
		RenderTarget2D* prevFrameRT,
		bool advanceState)
	{
		// CloudType 1 = AltocumulusMid (volumetric), CloudType 2 = Aurora (rendered by separate pass — no cloud geometry).
		bool layerIsAlto = (settings.CloudType == 1);
		if (!settings.Enabled || settings.CloudType == 2)
			return;
		// For non-Alto types, Coverage drives the raymarch density directly — skip at 0.
		// For AltocumulusMid, Coverage is a post-fade opacity multiplier in the shader;
		// still skip the full render pass when Coverage is exactly 0 (fully transparent).
		if (settings.Coverage < 0.001f)
			return;

		// Resolve quality params.
		auto newQuality = GetQualityParams(settings.Quality);
		if (newQuality.RenderResolutionScale != state.ActiveQuality.RenderResolutionScale)
		{
			state.ActiveQuality = newQuality;
			// Resize both the main render target and the prev-frame history target.
			// Without resizing prevFrameRT, CopyResource fails silently (dimension mismatch)
			// and prevFrameRT stays all-zeros → skip-pixels always return black → clouds invisible.
			float scale = newQuality.RenderResolutionScale;
			int w = std::max(1, (int)(_screenWidth * scale));
			int h = std::max(1, (int)(_screenHeight * scale));
			renderTarget = RenderTarget2D(
				_device.Get(), w, h,
				DXGI_FORMAT_R16G16B16A16_FLOAT,
				false,
				DXGI_FORMAT_UNKNOWN);
			if (prevFrameRT)
			{
				*prevFrameRT = RenderTarget2D(
					_device.Get(), w, h,
					DXGI_FORMAT_R16G16B16A16_FLOAT,
					false,
					DXGI_FORMAT_UNKNOWN);
			}
			// Reset FrameCounter so temporal starts in warmup (all pixels raymarched fresh).
			state.FrameCounter = 0;
		}
		else
		{
			state.ActiveQuality = newQuality;
		}

		// Invalidate temporal history when the cloud type changes (e.g. preset switch).
		// When CloudType changes, prevFrameRT holds clouds from a different cloud type.
		// Without invalidation, the stale history bleeds through via EMA — most visible
		// as ghost clouds at incorrect screen positions when the camera rotates (reprojection
		// maps old UV positions to new directions via PrevViewProjection).
		//
		// EXCEPTION: during a CloudMorph transition (MorphActive > 0.5) the shader
		// evaluates BOTH source and target densities simultaneously, so the visible
		// content evolves continuously even though settings.CloudType has already
		// switched to the target. Resetting temporal history here would disable EMA
		// smoothing for the entire morph, exposing the raw raymarcher's per-frame
		// noise (visible as flickering and horizontal march-plane banding).
		// Keep the history alive — it stays valid because the on-screen result is a
		// smooth blend of the same source clouds plus a gradually-forming target.
		if (state.PrevCloudType >= 0
			&& state.PrevCloudType != settings.CloudType
			&& settings.MorphActive <= 0.5f)
		{
			state.FrameCounter = 0;
			if (prevFrameRT)
			{
				float clearTemporal[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
				_context->ClearRenderTargetView(prevFrameRT->RenderTargetView.Get(), clearTemporal);
			}
		}
		state.PrevCloudType = settings.CloudType;

		// Update times — evolution time and wind offset accumulated separately.
		// Wind offset is monotonically non-decreasing to prevent backwards motion
		// when WindSpeed transitions to a lower value during preset blending.
		// Optional skip is used by the post-horizon bleed draw so clouds don't
		// advance twice per frame when the extra overlay pass is enabled.
		if (advanceState)
		{
			float dt = 1.0f / std::max(_refreshRate, 30);
			if (!state.FreezeEvolution)
				state.AccumulatedTime += dt;
			if (!state.FreezeWind)
				state.WindAccumOffset += settings.WindSpeed * dt;
			state.FrameCounter++;

			if (settings.LightningEnabled && settings.CloudType == 1)
				UpdateLightningThunder(settings, state, dt);
		}

		// Fill constant buffer.
		UpdateVolumetricCloudBuffer(settings, state, renderView);

		// Store current values for next frame's temporal reprojection.
		// (Must be AFTER UpdateVolumetricCloudBuffer which reads the Prev* values.)
		if (advanceState)
		{
			state.PrevCameraForward    = renderView.Camera.WorldDirection;
			state.PrevAccumulatedTime  = state.AccumulatedTime;
			state.PrevWindAccumOffset  = state.WindAccumOffset;
			state.PrevEvoAccumOffset   = state.EvoAccumOffset;
			state.PrevDissolvePhase    = settings.DissolvePhase;
			state.PrevFormationPhase   = settings.FormationPhase;
			state.PrevViewProjection   = renderView.Camera.ViewProjection;
		}

		// --- Pass 1: Render to half-res target ---
		// Temporal checkerboard: copy current RT to prevFrameRT before clearing,
		// so the shader can read last frame's result for skipped checkerboard pixels.
		if (prevFrameRT && state.ActiveQuality.TemporalReprojection)
			_context->CopyResource(prevFrameRT->Texture.Get(), renderTarget.Texture.Get());

		float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		_context->ClearRenderTargetView(renderTarget.RenderTargetView.Get(), clearColor);
		_context->OMSetRenderTargets(1, renderTarget.RenderTargetView.GetAddressOf(), nullptr);

		D3D11_VIEWPORT cloudViewport = {};
		cloudViewport.Width    = _stVolumetricCloud.CloudRenderSize.x;
		cloudViewport.Height   = _stVolumetricCloud.CloudRenderSize.y;
		cloudViewport.MinDepth = 0.0f;
		cloudViewport.MaxDepth = 1.0f;
		_context->RSSetViewports(1, &cloudViewport);

		BindConstantBufferPS(ConstantBufferRegister::VolumetricCloud, _cbVolumetricCloud.get());
		BindConstantBufferVS(ConstantBufferRegister::VolumetricCloud, _cbVolumetricCloud.get());

		if (_atmosphericSkySettings.Enabled)
		{
			auto* atmoSkyBuf = _cbAtmosphericSky.get();
			_context->PSSetConstantBuffers(10, 1, atmoSkyBuf);
		}

		// Bind previous-frame RT as t1 for temporal pixel reuse.
		if (prevFrameRT && state.ActiveQuality.TemporalReprojection)
		{
			BindRenderTargetAsTexture(TextureRegister::NormalMap, prevFrameRT,
				SamplerStateRegister::LinearClamp);
		}

		SetBlendMode(BlendMode::Opaque);
		SetCullMode(CullMode::CounterClockwise);
		SetDepthState(DepthState::None);

		_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		_context->IASetInputLayout(_fullscreenTriangleInputLayout.Get());

		unsigned int stride = sizeof(PostProcessVertex);
		unsigned int offset = 0;
		_context->IASetVertexBuffers(0, 1,
			_fullscreenTriangleVertexBuffer.Buffer.GetAddressOf(), &stride, &offset);

		_shaders.Bind(Shader::VolumetricClouds);

		// Bind pre-computed noise textures at t5, t6.
		_cloudNoiseTextures.Bind(_context.Get());

		DrawTriangles(3, 0);

		_cloudNoiseTextures.Unbind(_context.Get());

		// Unbind t1 (prev-frame RT) before composite pass.
		if (prevFrameRT && state.ActiveQuality.TemporalReprojection)
		{
			ID3D11ShaderResourceView* nullSRV = nullptr;
			_context->PSSetShaderResources((UINT)TextureRegister::NormalMap, 1, &nullSRV);
		}

		// --- Pass 2: Composite over scene ---
		_context->RSSetViewports(1, &renderView.Viewport);

		// Copy scene to backup RT for hybrid in-shader blending.
		_context->CopyResource(_scenePreCloudBackup.Texture.Get(),
			_renderTarget.Texture.Get());

		_context->OMSetRenderTargets(1, _renderTarget.RenderTargetView.GetAddressOf(),
			_renderTarget.DepthStencilView.Get());

		// Opaque blend — the shader computes the final composited color itself.
		SetBlendMode(BlendMode::Opaque);

		// Bind atmospheric sky CB so the composite shader can fade thin cloud
		// edges toward the sky tint instead of leaving dark low-alpha rims.
		if (_atmosphericSkySettings.Enabled)
		{
			auto* atmoSkyBuf = _cbAtmosphericSky.get();
			_context->PSSetConstantBuffers(10, 1, atmoSkyBuf);
		}

		BindRenderTargetAsTexture(TextureRegister::ColorMap, &renderTarget,
			SamplerStateRegister::LinearClamp);

		// Bind scene backup as texture (t3 = ShadowMap slot, safe during cloud pass).
		BindRenderTargetAsTexture(TextureRegister::ShadowMap, &_scenePreCloudBackup,
			SamplerStateRegister::LinearClamp);

		_shaders.Bind(Shader::VolumetricCloudComposite);
		DrawTriangles(3, 0);

		// --- Cleanup ---
		_context->IASetInputLayout(_inputLayout.Get());
		_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		ID3D11ShaderResourceView* nullSRVs[4] = { nullptr, nullptr, nullptr, nullptr };
		_context->PSSetShaderResources((UINT)TextureRegister::ColorMap, 4, nullSRVs);

		SetBlendMode(BlendMode::Opaque);
		SetDepthState(DepthState::Write);
		SetCullMode(CullMode::CounterClockwise);

		_context->RSSetViewports(1, &renderView.Viewport);
		_context->OMSetRenderTargets(1, _renderTarget.RenderTargetView.GetAddressOf(),
			_renderTarget.DepthStencilView.Get());
	}

	// ========================================================================
	// Dual-layer lens flare occlusion
	// ========================================================================

	void Renderer::UpdateDualCloudLensFlareOcclusion(RenderView& renderView)
	{
		// Update layer A occlusion.
		float transA = ComputeSingleLayerOcclusion(
			g_SkyCloudSystem.GetCloudARenderSettings(),
			_cloudState, _cloudOcclusionTarget, _cloudRenderTarget, renderView);
		g_SkyCloudSystem.SetLayerTransmittance(0, transA);

		// Update layer B occlusion.
		float transB = ComputeSingleLayerOcclusion(
			g_SkyCloudSystem.GetCloudBRenderSettings(),
			_cloudStateB, _cloudOcclusionTargetB, _cloudRenderTargetB, renderView);
		g_SkyCloudSystem.SetLayerTransmittance(1, transB);
	}

	float Renderer::ComputeSingleLayerOcclusion(
		const CloudRenderSettings& settings,
		CloudRuntimeState& state,
		RenderTarget2D& occlusionTarget,
		RenderTarget2D& cloudColorTarget,
		RenderView& renderView)
	{
		bool occIsAlto = (settings.CloudType == 1);
		// Aurora layers (CloudType 2) are rendered by a separate pass — skip occlusion.
		if (!settings.Enabled || settings.CloudType == 2 || (!occIsAlto && settings.Coverage < 0.001f))
			return 1.0f; // Fully visible (no clouds).

		// Throttle updates.
		constexpr int OCCLUSION_UPDATE_INTERVAL = 3;
		state.FlareOcclusion.CacheValidFrames++;

		if (state.FlareOcclusion.CacheValidFrames < OCCLUSION_UPDATE_INTERVAL)
			return state.FlareOcclusion.SmoothedTransmittance;

		state.FlareOcclusion.CacheValidFrames = 0;

		// Fill CB with this layer's settings.
		UpdateVolumetricCloudBuffer(settings, state, renderView);

		// Render occlusion to 1x1 target.
		float clearColor[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
		_context->ClearRenderTargetView(occlusionTarget.RenderTargetView.Get(), clearColor);
		_context->OMSetRenderTargets(1, occlusionTarget.RenderTargetView.GetAddressOf(), nullptr);

		D3D11_VIEWPORT occViewport = {};
		occViewport.Width    = 1.0f;
		occViewport.Height   = 1.0f;
		occViewport.MinDepth = 0.0f;
		occViewport.MaxDepth = 1.0f;
		_context->RSSetViewports(1, &occViewport);

		BindConstantBufferPS(ConstantBufferRegister::VolumetricCloud, _cbVolumetricCloud.get());
		BindConstantBufferVS(ConstantBufferRegister::VolumetricCloud, _cbVolumetricCloud.get());

		SetBlendMode(BlendMode::Opaque);
		SetCullMode(CullMode::CounterClockwise);
		SetDepthState(DepthState::None);

		_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		_context->IASetInputLayout(_fullscreenTriangleInputLayout.Get());

		unsigned int stride = sizeof(PostProcessVertex);
		unsigned int offset = 0;
		_context->IASetVertexBuffers(0, 1,
			_fullscreenTriangleVertexBuffer.Buffer.GetAddressOf(), &stride, &offset);

		// Bind this layer's cloud half-res RT as t0 so PSCloudOcclusion can
		// sample cloud alpha in the vicinity of the sun's projected screen position.
		BindRenderTargetAsTexture(TextureRegister::ColorMap, &cloudColorTarget,
			SamplerStateRegister::LinearClamp);

		_shaders.Bind(Shader::VolumetricCloudOcclusion);

		_cloudNoiseTextures.Bind(_context.Get());

		DrawTriangles(3, 0);

		_cloudNoiseTextures.Unbind(_context.Get());

		// Unbind t0.
		ID3D11ShaderResourceView* nullSRV = nullptr;
		_context->PSSetShaderResources((UINT)TextureRegister::ColorMap, 1, &nullSRV);

		// Readback.
		D3D11_TEXTURE2D_DESC stagingDesc = {};
		stagingDesc.Width              = 1;
		stagingDesc.Height             = 1;
		stagingDesc.MipLevels          = 1;
		stagingDesc.ArraySize          = 1;
		stagingDesc.Format             = DXGI_FORMAT_R16_FLOAT;
		stagingDesc.SampleDesc.Count   = 1;
		stagingDesc.Usage              = D3D11_USAGE_STAGING;
		stagingDesc.CPUAccessFlags     = D3D11_CPU_ACCESS_READ;

		ComPtr<ID3D11Texture2D> stagingTexture;
		_device->CreateTexture2D(&stagingDesc, nullptr, stagingTexture.GetAddressOf());
		_context->CopyResource(stagingTexture.Get(), occlusionTarget.Texture.Get());

		D3D11_MAPPED_SUBRESOURCE mapped;
		if (SUCCEEDED(_context->Map(stagingTexture.Get(), 0, D3D11_MAP_READ, 0, &mapped)))
		{
			DirectX::PackedVector::HALF halfVal =
				*reinterpret_cast<DirectX::PackedVector::HALF*>(mapped.pData);
			float transmittance = DirectX::PackedVector::XMConvertHalfToFloat(halfVal);
			state.FlareOcclusion.CloudTransmittance = std::clamp(transmittance, 0.0f, 1.0f);
			_context->Unmap(stagingTexture.Get(), 0);
		}

		// Temporal smoothing.
		constexpr float SMOOTH_FACTOR = 0.6f;
		float prev = state.FlareOcclusion.SmoothedTransmittance;
		float curr = state.FlareOcclusion.CloudTransmittance;
		state.FlareOcclusion.SmoothedTransmittance = prev + (curr - prev) * SMOOTH_FACTOR;

		// Restore pipeline state.
		_context->IASetInputLayout(_inputLayout.Get());
		_context->RSSetViewports(1, &renderView.Viewport);
		_context->OMSetRenderTargets(1, _renderTarget.RenderTargetView.GetAddressOf(),
			_renderTarget.DepthStencilView.Get());

		return state.FlareOcclusion.SmoothedTransmittance;
	}
}
