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
		int w = std::max(1, (int)(_graphicsDevice->GetScreenWidth() * scale));
		int h = std::max(1, (int)(_graphicsDevice->GetScreenHeight() * scale));

		_cloudRenderTargetB = _graphicsDevice->CreateRenderSurface2D(
			w, h,
			SurfaceFormat::SF_RGBA16_Float,
			false,
			DepthFormat::None);

		// Previous-frame RT for temporal checkerboard on layer B.
		_cloudPrevFrameRTB = _graphicsDevice->CreateRenderSurface2D(
			w, h,
			SurfaceFormat::SF_RGBA16_Float,
			false,
			DepthFormat::None);

		// 1x1 occlusion target for layer B (must match the source size of the
		// async readback so CopyResource succeeds — DX11 requires identical dims).
		_cloudOcclusionTargetB = _graphicsDevice->CreateRenderSurface2D(
			1, 1,
			SurfaceFormat::SF_R16_Float,
			false,
			DepthFormat::None);

		_cloudOcclusionReadbackB = _graphicsDevice->CreateGpuReadbackBuffer(
			1, 1, SurfaceFormat::SF_R16_Float);
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
		// Draw Cloud Layer A (higher / thinner — composited first = behind).
		if (g_SkyCloudSystem.IsCloudAActive())
		{
			auto settingsA = g_SkyCloudSystem.GetCloudARenderSettings();
			DrawSingleVolumetricCloudLayer(
				settingsA, _cloudState, _cloudRenderTarget.get(), renderView, _cloudPrevFrameRT.get());
		}
		else
		{
			_graphicsDevice->ClearRenderTarget2D(_cloudRenderTarget->GetRenderTarget(), Colors::Transparent);

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
				settingsB, _cloudStateB, _cloudRenderTargetB.get(), renderView, _cloudPrevFrameRTB.get());
		}
		else
		{
			_graphicsDevice->ClearRenderTarget2D(_cloudRenderTargetB->GetRenderTarget(), Colors::Transparent);
			
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
		IRenderSurface2D* renderTarget,
		RenderView& renderView,
		IRenderSurface2D* prevFrameRT,
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

		// Quality can only change at level load, so just cache it per-frame.
		state.ActiveQuality = GetQualityParams(g_SkyCloudSystem.GetGlobalQuality());

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
				_graphicsDevice->ClearRenderTarget2D(prevFrameRT->GetRenderTarget(), Colors::Transparent);
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
			float dt = 1.0f / std::max(_graphicsDevice->GetRefreshRate(), 30);
			if (!state.FreezeEvolution)
				state.AccumulatedTime += dt;
			if (!state.FreezeWind)
				state.WindAccumOffset += settings.WindSpeed * dt;

			// Pre-integrate EvolutionSpeed * dt so that on-screen cloud advection
			// stays continuous when EvolutionSpeed changes between presets. Using
			// CloudTime * EvolutionSpeed_now directly causes a time-lapse jump on
			// transitions (e.g. Altocumulus EvSpd=0 -> Thunderstorm EvSpd=1.276):
			// the product retroactively rescales all elapsed time at the new rate.
			// With pre-integration, past frames keep their own contribution and
			// only future frames advance at the new rate.
			// Scaling factors (0.05 and 0.16) match the historical multipliers in
			// the shader (CloudTime * EvSpd * 0.05/0.16) so visible motion speed
			// is unchanged at steady state.
			if (!state.FreezeEvolution)
			{
				state.EvoAccumOffset  += settings.EvolutionSpeed * dt * 0.05f;
				state.FlowAccumOffset += settings.EvolutionSpeed * dt * 0.16f;
			}

			// Pre-integrate (WindSpeed * AltoFbmScale * dt) and (EvolutionSpeed *
			// AltoFbmScale * dt * 0.05) so AltoFbmScale changes between presets do
			// not retroactively rescale all accumulated past wind+evo motion in FBM
			// space (which reads as time-lapse during transitions where AltoFbmScale
			// differs significantly between source and target presets).
			// FbmScale is clamped to a sensible minimum so the integral never freezes.
			float fbmScaleSafe = std::max(settings.AltoFbmScale, 0.001f);
			if (!state.FreezeWind)
				state.WindAccumOffsetScaled += settings.WindSpeed * fbmScaleSafe * dt;
			if (!state.FreezeEvolution)
				state.EvoAccumOffsetScaled += settings.EvolutionSpeed * fbmScaleSafe * dt * 0.05f;

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
			_graphicsDevice->CopyTextureResource(renderTarget->GetRenderTarget(), prevFrameRT->GetRenderTarget());
		
		_graphicsDevice->ClearRenderTarget2D(renderTarget->GetRenderTarget(), Colors::Transparent);
		_graphicsDevice->BindRenderTarget(renderTarget->GetRenderTarget(), nullptr);

		RendererViewport cloudViewport = {};
		cloudViewport.Width    = _stVolumetricCloud.CloudRenderSize.x;
		cloudViewport.Height   = _stVolumetricCloud.CloudRenderSize.y;
		cloudViewport.MinDepth = 0.0f;
		cloudViewport.MaxDepth = 1.0f;
		_graphicsDevice->SetViewport(cloudViewport);

		BindConstantBuffer(ShaderStage::PixelShader, ConstantBufferRegister::VolumetricCloud, _cbVolumetricCloud.get());
		BindConstantBuffer(ShaderStage::VertexShader, ConstantBufferRegister::VolumetricCloud, _cbVolumetricCloud.get());

		if (_atmosphericSkySettings.Enabled)
		{
			auto* atmoSkyBuf = _cbAtmosphericSky.get();
			BindConstantBuffer(ShaderStage::PixelShader, ConstantBufferRegister::AtmosphericSky, atmoSkyBuf);
		}

		// Bind previous-frame RT as t1 for temporal pixel reuse.
		if (prevFrameRT && state.ActiveQuality.TemporalReprojection)
		{
			BindRenderTargetAsTexture(TextureRegister::NormalMap, prevFrameRT->GetRenderTarget(),
				SamplerStateRegister::LinearClamp);
		}

		SetBlendMode(BlendMode::Opaque);
		SetCullMode(CullMode::CounterClockwise);
		SetDepthState(DepthState::None);

		_graphicsDevice->SetPrimitiveType(PrimitiveType::TriangleList);
		_graphicsDevice->SetInputLayout(_fullScreenVertexInputLayout.get());
		_graphicsDevice->BindVertexBuffer(_fullscreenTriangleVertexBuffer.get());
		
		_shaders.Bind(Shader::VolumetricClouds);

		// Bind pre-computed noise textures at t5, t6.
		_cloudNoiseTextures.Bind(_graphicsDevice.get());

		DrawTriangles(3, 0);

		_cloudNoiseTextures.Unbind(_graphicsDevice.get());

		// Unbind t1 (prev-frame RT) before composite pass.
		if (prevFrameRT && state.ActiveQuality.TemporalReprojection)
			_graphicsDevice->UnbindTexture(ShaderStage::PixelShader, TextureRegister::NormalMap);

		// --- Pass 2: Composite over scene ---
		_graphicsDevice->SetViewport(renderView.Viewport);

		// Copy scene to backup RT for hybrid in-shader blending.
		_graphicsDevice->CopyTextureResource(_renderTarget->GetRenderTarget(), _scenePreCloudBackup->GetRenderTarget());

		_graphicsDevice->BindRenderTarget(_renderTarget->GetRenderTarget(), _renderTarget->GetDepthTarget());

		// Opaque blend — the shader computes the final composited color itself.
		SetBlendMode(BlendMode::Opaque);

		// Bind atmospheric sky CB so the composite shader can fade thin cloud
		// edges toward the sky tint instead of leaving dark low-alpha rims.
		if (_atmosphericSkySettings.Enabled)
		{
			auto* atmoSkyBuf = _cbAtmosphericSky.get();
			_graphicsDevice->BindConstantBuffer(ShaderStage::PixelShader, ConstantBufferRegister::AtmosphericSky, atmoSkyBuf);
		}

		BindRenderTargetAsTexture(TextureRegister::ColorMap, renderTarget->GetRenderTarget(),
			SamplerStateRegister::LinearClamp);

		// Bind scene backup as texture (t3 = ShadowMap slot, safe during cloud pass).
		BindRenderTargetAsTexture(TextureRegister::ShadowMap, _scenePreCloudBackup->GetRenderTarget(),
			SamplerStateRegister::LinearClamp);

		_shaders.Bind(Shader::VolumetricCloudComposite);
		DrawTriangles(3, 0);

		// --- Cleanup ---
		_graphicsDevice->SetInputLayout(_vertexInputLayout.get());
		_graphicsDevice->SetPrimitiveType(PrimitiveType::TriangleList);

		// Drop SRVs at slots 0..3 (ColorMap, NormalMap, CausticsMap, ShadowMap).
		_graphicsDevice->UnbindTexture(ShaderStage::PixelShader, TextureRegister::ColorMap);
		_graphicsDevice->UnbindTexture(ShaderStage::PixelShader, TextureRegister::NormalMap);
		_graphicsDevice->UnbindTexture(ShaderStage::PixelShader, TextureRegister::CausticsMap);
		_graphicsDevice->UnbindTexture(ShaderStage::PixelShader, TextureRegister::ShadowMap);

		SetBlendMode(BlendMode::Opaque);
		SetDepthState(DepthState::Write);
		SetCullMode(CullMode::CounterClockwise);

		_graphicsDevice->SetViewport(renderView.Viewport);
		_graphicsDevice->BindRenderTarget(_renderTarget->GetRenderTarget(), _depthRenderTarget->GetDepthTarget());
	}

	// ========================================================================
	// Dual-layer lens flare occlusion
	// ========================================================================

	void Renderer::UpdateDualCloudLensFlareOcclusion(RenderView& renderView)
	{
		// Update layer A occlusion.
		float transA = ComputeSingleLayerOcclusion(
			g_SkyCloudSystem.GetCloudARenderSettings(),
			_cloudState, _cloudOcclusionTarget.get(), _cloudRenderTarget.get(),
			_cloudOcclusionReadback.get(), renderView);
		g_SkyCloudSystem.SetLayerTransmittance(0, transA);

		// Update layer B occlusion.
		float transB = ComputeSingleLayerOcclusion(
			g_SkyCloudSystem.GetCloudBRenderSettings(),
			_cloudStateB, _cloudOcclusionTargetB.get(), _cloudRenderTargetB.get(),
			_cloudOcclusionReadbackB.get(), renderView);
		g_SkyCloudSystem.SetLayerTransmittance(1, transB);
	}

	float Renderer::ComputeSingleLayerOcclusion(
		const CloudRenderSettings& settings,
		CloudRuntimeState& state,
		IRenderSurface2D* occlusionTarget,
		IRenderSurface2D* cloudColorTarget,
		IGpuReadbackBuffer* readback,
		RenderView& renderView)
	{
		bool occIsAlto = (settings.CloudType == 1);
		// Aurora layers (CloudType 2) are rendered by a separate pass — skip occlusion.
		if (!settings.Enabled || settings.CloudType == 2 || (!occIsAlto && settings.Coverage < 0.001f))
			return 1.0f; // Fully visible (no clouds).

		// Drain any completed readback first — when ready, this updates
		// CloudTransmittance with the value submitted ~2 throttled-updates ago.
		if (readback != nullptr)
		{
			uint16_t halfBits = 0;
			if (readback->TryRead(&halfBits, sizeof(halfBits)))
			{
				DirectX::PackedVector::HALF halfVal = halfBits;
				float transmittance = DirectX::PackedVector::XMConvertHalfToFloat(halfVal);
				state.FlareOcclusion.CloudTransmittance = std::clamp(transmittance, 0.0f, 1.0f);

				// Temporal smoothing — only when we receive new data, so the
				// visible value advances at the cadence of completed readbacks.
				constexpr float SMOOTH_FACTOR = 0.6f;
				float prev = state.FlareOcclusion.SmoothedTransmittance;
				float curr = state.FlareOcclusion.CloudTransmittance;
				state.FlareOcclusion.SmoothedTransmittance = prev + (curr - prev) * SMOOTH_FACTOR;
			}
		}

		// Throttle the (expensive) occlusion render pass.
		constexpr int OCCLUSION_UPDATE_INTERVAL = 3;
		state.FlareOcclusion.CacheValidFrames++;

		if (state.FlareOcclusion.CacheValidFrames < OCCLUSION_UPDATE_INTERVAL)
			return state.FlareOcclusion.SmoothedTransmittance;

		state.FlareOcclusion.CacheValidFrames = 0;

		// Fill CB with this layer's settings.
		UpdateVolumetricCloudBuffer(settings, state, renderView);

		// Render occlusion to 1x1 target.
		_graphicsDevice->ClearRenderTarget2D(occlusionTarget->GetRenderTarget(), Colors::Transparent);
		_graphicsDevice->BindRenderTarget(occlusionTarget->GetRenderTarget(), nullptr);

		RendererViewport occViewport = {};
		occViewport.Width    = 1.0f;
		occViewport.Height   = 1.0f;
		occViewport.MinDepth = 0.0f;
		occViewport.MaxDepth = 1.0f;
		_graphicsDevice->SetViewport(occViewport);

		BindConstantBuffer(ShaderStage::PixelShader, ConstantBufferRegister::VolumetricCloud, _cbVolumetricCloud.get());
		BindConstantBuffer(ShaderStage::VertexShader, ConstantBufferRegister::VolumetricCloud, _cbVolumetricCloud.get());

		SetBlendMode(BlendMode::Opaque);
		SetCullMode(CullMode::CounterClockwise);
		SetDepthState(DepthState::None);

		_graphicsDevice->SetPrimitiveType(PrimitiveType::TriangleList);
		_graphicsDevice->SetInputLayout(_fullScreenVertexInputLayout.get());
		_graphicsDevice->BindVertexBuffer(_fullscreenTriangleVertexBuffer.get());

		// Bind this layer's cloud half-res RT as t0 so PSCloudOcclusion can
		// sample cloud alpha in the vicinity of the sun's projected screen position.
		BindRenderTargetAsTexture(TextureRegister::ColorMap, cloudColorTarget->GetRenderTarget(),
			SamplerStateRegister::LinearClamp);

		_shaders.Bind(Shader::VolumetricCloudOcclusion);

		_cloudNoiseTextures.Bind(_graphicsDevice.get());

		DrawTriangles(3, 0);

		_cloudNoiseTextures.Unbind(_graphicsDevice.get());

		// Unbind t0 so the source RT isn't held as SRV while we copy from it.
		_graphicsDevice->UnbindTexture(ShaderStage::PixelShader, TextureRegister::ColorMap);

		// Schedule async readback. Result will be available a few frames later
		// via TryRead at the top of this function — never stalls the CPU.
		if (readback != nullptr)
			readback->SubmitCopy(occlusionTarget->GetRenderTarget());

		// Restore pipeline state for the rest of the frame.
		_graphicsDevice->SetInputLayout(_vertexInputLayout.get());
		_graphicsDevice->SetViewport(renderView.Viewport);

		return state.FlareOcclusion.SmoothedTransmittance;
	}
}
