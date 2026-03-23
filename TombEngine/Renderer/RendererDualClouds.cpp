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
		// Draw Cloud Layer A (higher / thinner — composited first = behind).
		if (g_SkyCloudSystem.IsCloudAActive())
		{
			auto settingsA = g_SkyCloudSystem.GetCloudARenderSettings();
			DrawSingleVolumetricCloudLayer(
				settingsA, _cloudState, _cloudRenderTarget, renderView);
		}

		// Draw Cloud Layer B (lower / denser — composited second = in front).
		if (g_SkyCloudSystem.IsCloudBActive())
		{
			auto settingsB = g_SkyCloudSystem.GetCloudBRenderSettings();
			DrawSingleVolumetricCloudLayer(
				settingsB, _cloudStateB, _cloudRenderTargetB, renderView);
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
		bool advanceState)
	{
		bool layerIsAlto = (settings.CloudType == 2);
		if (!settings.Enabled)
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
			// Resize this layer's target.
			float scale = newQuality.RenderResolutionScale;
			int w = std::max(1, (int)(_screenWidth * scale));
			int h = std::max(1, (int)(_screenHeight * scale));
			renderTarget = RenderTarget2D(
				_device.Get(), w, h,
				DXGI_FORMAT_R16G16B16A16_FLOAT,
				false,
				DXGI_FORMAT_UNKNOWN);
		}
		else
		{
			state.ActiveQuality = newQuality;
		}

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
		}

		// Fill constant buffer.
		UpdateVolumetricCloudBuffer(settings, state, renderView);

		// --- Pass 1: Render to half-res target ---
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
		DrawTriangles(3, 0);

		// --- Pass 2: Composite over scene ---
		_context->RSSetViewports(1, &renderView.Viewport);
		_context->OMSetRenderTargets(1, _renderTarget.RenderTargetView.GetAddressOf(),
			_renderTarget.DepthStencilView.Get());

		SetBlendMode(BlendMode::AlphaBlend);

		BindRenderTargetAsTexture(TextureRegister::ColorMap, &renderTarget,
			SamplerStateRegister::LinearClamp);

		_shaders.Bind(Shader::VolumetricCloudComposite);
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
		bool occIsAlto = (settings.CloudType == 2);
		if (!settings.Enabled || (!occIsAlto && settings.Coverage < 0.001f))
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
		DrawTriangles(3, 0);

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
