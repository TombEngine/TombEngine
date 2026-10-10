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
// The draws replay the records of PrepareCloudLayers (RendererSkyEffects.cpp); the
// next renderer computes the lens flare occlusion of each layer.
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
#include "Specific/trutils.h"

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

		// Release previous Layer B targets/buffers before recreation.
		SAFE_DELETE(_cloudRenderTargetB);
		SAFE_DELETE(_cloudPrevFrameRTB);
		SAFE_DELETE(_cloudOcclusionTargetB);
		SAFE_DELETE(_cloudOcclusionReadbackB);

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
	// Draw the volumetric cloud layers prepared by PrepareCloudLayers
	// ========================================================================

	void Renderer::DrawDualVolumetricClouds(RenderView& renderView)
	{
		for (const auto& draw : _nextCloudDraws)
		{
			if (draw.IsAfterHorizons)
				continue;

			// Inactive layers are cleared, so the god ray shader never reads stale clouds.
			if (draw.IsCleared)
			{
				auto* target = (draw.Layer == 0) ? _cloudRenderTarget.get() : _cloudRenderTargetB.get();
				_graphicsDevice->ClearRenderTarget2D(target->GetRenderTarget(), Colors::Transparent);
				continue;
			}

			DrawCloudLayer(draw, renderView);
		}
	}

	// ========================================================================
	// Draw a single volumetric cloud layer into its render target
	// ========================================================================

	void Renderer::DrawCloudLayer(const Next::CloudLayerDraw& draw, RenderView& renderView)
	{
		auto* renderTarget = (draw.Layer == 0) ? _cloudRenderTarget.get() : _cloudRenderTargetB.get();
		auto* prevFrameRT = draw.IsAfterHorizons ? nullptr : ((draw.Layer == 0) ? _cloudPrevFrameRT.get() : _cloudPrevFrameRTB.get());
		bool skipRaymarch = !draw.IsRaymarched;

		if (draw.IsHistoryReset && prevFrameRT != nullptr)
			_graphicsDevice->ClearRenderTarget2D(prevFrameRT->GetRenderTarget(), Colors::Transparent);

		_stVolumetricCloud = draw.Uniforms;
		UpdateConstantBuffer(&_stVolumetricCloud, _cbVolumetricCloud.get());

		// CB must be bound before Pass 2 regardless of whether Pass 1 runs.
		BindConstantBuffer(ShaderStage::PixelShader, ConstantBufferRegister::VolumetricCloud, _cbVolumetricCloud.get());
		BindConstantBuffer(ShaderStage::VertexShader, ConstantBufferRegister::VolumetricCloud, _cbVolumetricCloud.get());

		if (!skipRaymarch)
		{
			// Temporal checkerboard: copy current RT to prevFrameRT before clearing,
			// so the shader can read last frame's result for skipped checkerboard pixels.
			if (draw.IsHistoryUsed)
				_graphicsDevice->CopyTextureResource(renderTarget->GetRenderTarget(), prevFrameRT->GetRenderTarget());

			_graphicsDevice->ClearRenderTarget2D(renderTarget->GetRenderTarget(), Colors::Transparent);
			_graphicsDevice->BindRenderTarget(renderTarget->GetRenderTarget(), nullptr);

			RendererViewport cloudViewport = {};
			cloudViewport.Width    = _stVolumetricCloud.CloudRenderSize.x;
			cloudViewport.Height   = _stVolumetricCloud.CloudRenderSize.y;
			cloudViewport.MinDepth = 0.0f;
			cloudViewport.MaxDepth = 1.0f;
			_graphicsDevice->SetViewport(cloudViewport);

			if (_atmosphericSkySettings.Enabled)
			{
				auto* atmoSkyBuf = _cbAtmosphericSky.get();
				BindConstantBuffer(ShaderStage::PixelShader, ConstantBufferRegister::AtmosphericSky, atmoSkyBuf);
			}

			// Bind previous-frame RT as t1 for temporal pixel reuse.
			if (draw.IsHistoryUsed)
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
			if (draw.IsHistoryUsed)
				_graphicsDevice->UnbindTexture(ShaderStage::PixelShader, TextureRegister::NormalMap);
		}

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
}
