// RendererVolumetricClouds.cpp — Volumetric cloud rendering integration.
//
// This file implements the renderer-side logic for the volumetric cloud system:
//   - Initialization: render targets, constant buffers, shader loading
//   - Per-frame update: fill constant buffer from settings
//   - Draw calls: cloud pass, composite pass, occlusion query
//   - Lens flare occlusion integration
//
// Called from the main render pipeline in RendererDraw.cpp.

#include "framework.h"
#include "Renderer/Renderer.h"

#include <DirectXPackedVector.h>
#include "Game/control/control.h"
#include "Game/camera.h"
#include "Renderer/VolumetricCloud/VolumetricCloud.h"
#include "Renderer/ConstantBuffers/VolumetricCloudBuffer.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Scripting/Include/ScriptInterfaceLevel.h"
#include "Scripting/Internal/TEN/Flow/Level/FlowLevel.h"
#include "Specific/level.h"

using namespace TEN::Renderer::VolumetricCloud;
using namespace TEN::Renderer::ConstantBuffers;

namespace TEN::Renderer
{
	// ========================================================================
	// Initialization
	// ========================================================================

	void Renderer::InitializeVolumetricClouds()
	{
		// Create the constant buffer.
		_cbVolumetricCloud = ConstantBuffer<CVolumetricCloudBuffer>(_device.Get());

		// Render targets are created/resized in ResizeVolumetricCloudTargets().
		ResizeVolumetricCloudTargets();
	}

	void Renderer::ResizeVolumetricCloudTargets()
	{
		// Determine cloud render resolution based on current quality settings.
		float scale = _cloudState.ActiveQuality.RenderResolutionScale;
		int w = std::max(1, (int)(_screenWidth * scale));
		int h = std::max(1, (int)(_screenHeight * scale));

		// Half-res RGBA16F target for cloud color + opacity.
		_cloudRenderTarget = RenderTarget2D(
			_device.Get(), w, h,
			DXGI_FORMAT_R16G16B16A16_FLOAT,
			false,   // not typeless
			DXGI_FORMAT_UNKNOWN);  // no depth

		// 1x1 target for lens flare occlusion transmittance readback.
		_cloudOcclusionTarget = RenderTarget2D(
			_device.Get(), 1, 1,
			DXGI_FORMAT_R16_FLOAT,
			false,
			DXGI_FORMAT_UNKNOWN);
	}

	// ========================================================================
	// Per-frame constant buffer update
	// ========================================================================

	void Renderer::UpdateVolumetricCloudBuffer(const CloudRenderSettings& settings, RenderView& view)
	{
		auto& q = _cloudState.ActiveQuality;

		_stVolumetricCloud.CloudBottomHeight = settings.CloudBottomHeight;
		_stVolumetricCloud.CloudTopHeight    = settings.CloudBottomHeight + settings.CloudThickness;
		_stVolumetricCloud.CloudThickness    = settings.CloudThickness;
		_stVolumetricCloud.Coverage          = settings.Coverage;

		_stVolumetricCloud.Density           = settings.Density;
		_stVolumetricCloud.ShapeScale        = settings.Noise.ShapeScale;
		_stVolumetricCloud.DetailScale       = settings.Noise.DetailScale;
		_stVolumetricCloud.DetailStrength    = settings.Noise.DetailStrength;

		_stVolumetricCloud.WeatherScale      = settings.Noise.WeatherScale;
		_stVolumetricCloud.Absorption        = settings.Absorption;
		_stVolumetricCloud.AmbientContrib    = settings.AmbientContrib;
		_stVolumetricCloud.SilverliningStr   = settings.SilverliningStr;

		_stVolumetricCloud.PhaseForward      = settings.PhaseForward;
		_stVolumetricCloud.PhaseBackward     = settings.PhaseBackward;
		_stVolumetricCloud.WindSpeed         = _cloudState.FreezeWind ? 0.0f : settings.WindSpeed;
		_stVolumetricCloud.EvolutionSpeed    = _cloudState.FreezeEvolution ? 0.0f : settings.EvolutionSpeed;

		_stVolumetricCloud.WindDirection     = settings.WindDirection;
		_stVolumetricCloud.Time              = _cloudState.AccumulatedTime;
		_stVolumetricCloud.JitterStrength    = settings.JitterStrength;

		// Light direction: prefer existing lens flare direction, fallback to settings.
		auto* levelPtr = g_GameFlow->GetLevel(CurrentLevel);
		if (levelPtr->GetLensFlareEnabled())
		{
			// Reconstruct light direction from lens flare pitch/yaw.
			float pitch = DirectX::XMConvertToRadians((float)levelPtr->GetLensFlarePitch());
			float yaw   = DirectX::XMConvertToRadians((float)levelPtr->GetLensFlareYaw());

			_stVolumetricCloud.LightDirection = Vector3(
				std::cos(pitch) * std::sin(yaw),
				std::sin(pitch),
				std::cos(pitch) * std::cos(yaw));
			_stVolumetricCloud.LightDirection.Normalize();

			auto flareColor = levelPtr->GetLensFlareColor();
			_stVolumetricCloud.LightColor = Vector3(
				flareColor.x,
				flareColor.y,
				flareColor.z);
		}
		else if (settings.LightDirection.LengthSquared() > 0.001f)
		{
			_stVolumetricCloud.LightDirection = settings.LightDirection;
			_stVolumetricCloud.LightDirection.Normalize();
			_stVolumetricCloud.LightColor = Vector3(1.0f, 0.95f, 0.85f);
		}
		else
		{
			// Default: high sun.
			_stVolumetricCloud.LightDirection = Vector3(0.3f, 0.85f, 0.2f);
			_stVolumetricCloud.LightDirection.Normalize();
			_stVolumetricCloud.LightColor = Vector3(1.0f, 0.95f, 0.85f);
		}

		_stVolumetricCloud.PrimaryStepCount   = q.PrimaryStepCount;
		_stVolumetricCloud.ShadowStepCount    = q.ShadowStepCount;
		_stVolumetricCloud.DetailNoiseEnabled  = q.DetailNoiseEnabled ? 1 : 0;
		_stVolumetricCloud.DebugView          = (int)_cloudState.DebugView;

		float scale = q.RenderResolutionScale;
		float w = (float)std::max(1, (int)(_screenWidth * scale));
		float h = (float)std::max(1, (int)(_screenHeight * scale));
		_stVolumetricCloud.CloudRenderSize    = Vector2(w, h);
		_stVolumetricCloud.InvCloudRenderSize = Vector2(1.0f / w, 1.0f / h);

		_stVolumetricCloud.TemporalEnabled = q.TemporalReprojection ? 1 : 0;
		_stVolumetricCloud.FrameIndex      = (float)(_cloudState.FrameCounter % 256);

		// Earth radius for spherical shell curvature.
		// Using a moderate value that gives visible curvature at cloud altitudes.
		_stVolumetricCloud.EarthRadius   = 600000.0f;
		_stVolumetricCloud.PlanetCenterY = -(600000.0f);

		UpdateConstantBuffer(_stVolumetricCloud, _cbVolumetricCloud);
	}

	// ========================================================================
	// Main cloud drawing
	// ========================================================================

	void Renderer::DrawVolumetricClouds(RenderView& renderView)
	{
		// Check if any cloud layer uses volumetric mode.
		const CloudRenderSettings* activeSettings = GetActiveVolumetricCloudSettings();
		if (!activeSettings || !activeSettings->Enabled)
			return;

		// Quick early-out: coverage is zero means perfectly clear sky.
		if (activeSettings->Coverage < 0.001f)
			return;

		// Resolve quality params.
		_cloudState.ActiveQuality = GetQualityParams(activeSettings->Quality);

		// Update accumulated time.
		float dt = 1.0f / std::max(_refreshRate, 30);
		if (!_cloudState.FreezeWind && !_cloudState.FreezeEvolution)
			_cloudState.AccumulatedTime += dt;
		_cloudState.FrameCounter++;

		// Update constant buffer.
		UpdateVolumetricCloudBuffer(*activeSettings, renderView);

		// --- Pass 1: Render clouds to half-res target ---
		float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		_context->ClearRenderTargetView(_cloudRenderTarget.RenderTargetView.Get(), clearColor);
		_context->OMSetRenderTargets(1, _cloudRenderTarget.RenderTargetView.GetAddressOf(), nullptr);

		// Set viewport to cloud render resolution.
		D3D11_VIEWPORT cloudViewport = {};
		cloudViewport.Width    = _stVolumetricCloud.CloudRenderSize.x;
		cloudViewport.Height   = _stVolumetricCloud.CloudRenderSize.y;
		cloudViewport.MinDepth = 0.0f;
		cloudViewport.MaxDepth = 1.0f;
		_context->RSSetViewports(1, &cloudViewport);

		// Bind cloud constant buffer to b9.
		BindConstantBufferPS(ConstantBufferRegister::VolumetricCloud, _cbVolumetricCloud.get());
		BindConstantBufferVS(ConstantBufferRegister::VolumetricCloud, _cbVolumetricCloud.get());

		// Bind fullscreen triangle.
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

		// --- Pass 2: Composite clouds over scene ---
		// Restore full-res viewport.
		_context->RSSetViewports(1, &renderView.Viewport);
		_context->OMSetRenderTargets(1, _renderTarget.RenderTargetView.GetAddressOf(),
			_renderTarget.DepthStencilView.Get());

		// Blend clouds over the existing sky using alpha blending.
		SetBlendMode(BlendMode::AlphaBlend);

		// Bind cloud render result as texture.
		BindRenderTargetAsTexture(TextureRegister::ColorMap, &_cloudRenderTarget,
			SamplerStateRegister::LinearClamp);

		_shaders.Bind(Shader::VolumetricCloudComposite);
		DrawTriangles(3, 0);

		SetBlendMode(BlendMode::Opaque);
	}

	// ========================================================================
	// Lens flare occlusion
	// ========================================================================

	void Renderer::UpdateCloudLensFlareOcclusion(RenderView& renderView)
	{
		const CloudRenderSettings* activeSettings = GetActiveVolumetricCloudSettings();
		if (!activeSettings || !activeSettings->Enabled || activeSettings->Coverage < 0.001f)
		{
			_cloudState.FlareOcclusion.SmoothedTransmittance = 1.0f;
			return;
		}

		// Only recalculate every few frames to save cost (light/camera don't change rapidly).
		constexpr int OCCLUSION_UPDATE_INTERVAL = 3;
		_cloudState.FlareOcclusion.CacheValidFrames++;

		if (_cloudState.FlareOcclusion.CacheValidFrames < OCCLUSION_UPDATE_INTERVAL)
			return;

		_cloudState.FlareOcclusion.CacheValidFrames = 0;

		// Render a single-pixel occlusion query.
		float clearColor[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
		_context->ClearRenderTargetView(_cloudOcclusionTarget.RenderTargetView.Get(), clearColor);
		_context->OMSetRenderTargets(1, _cloudOcclusionTarget.RenderTargetView.GetAddressOf(), nullptr);

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

		_shaders.Bind(Shader::VolumetricCloudOcclusion);
		DrawTriangles(3, 0);

		// Read back the transmittance value.
		// Use a staging texture for GPU -> CPU readback.
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

		_context->CopyResource(stagingTexture.Get(), _cloudOcclusionTarget.Texture.Get());

		D3D11_MAPPED_SUBRESOURCE mapped;
		if (SUCCEEDED(_context->Map(stagingTexture.Get(), 0, D3D11_MAP_READ, 0, &mapped)))
		{
			// Convert single R16_FLOAT half-precision value to float.
			DirectX::PackedVector::HALF halfVal = *reinterpret_cast<DirectX::PackedVector::HALF*>(mapped.pData);
			float transmittance = DirectX::PackedVector::XMConvertHalfToFloat(halfVal);
			_cloudState.FlareOcclusion.CloudTransmittance = std::clamp(transmittance, 0.0f, 1.0f);
			_context->Unmap(stagingTexture.Get(), 0);
		}

		// Temporal smoothing to avoid flicker.
		constexpr float SMOOTH_FACTOR = 0.15f;
		float prev = _cloudState.FlareOcclusion.SmoothedTransmittance;
		float curr = _cloudState.FlareOcclusion.CloudTransmittance;
		_cloudState.FlareOcclusion.SmoothedTransmittance = prev + (curr - prev) * SMOOTH_FACTOR;
	}

	float Renderer::GetCloudLensFlareOcclusion() const
	{
		return _cloudState.FlareOcclusion.SmoothedTransmittance;
	}

	// ========================================================================
	// Settings accessor
	// ========================================================================

	const CloudRenderSettings* Renderer::GetActiveVolumetricCloudSettings() const
	{
		// Pull settings from level script.
		// Check if either layer has been set to volumetric mode.
		auto* levelPtr = dynamic_cast<const Level*>(g_GameFlow->GetLevel(CurrentLevel));
		if (!levelPtr)
			return nullptr;

		// Check layer 1 first (primary cloud layer).
		if (levelPtr->HasVolumetricCloudLayer(0))
		{
			auto* vlayer = levelPtr->GetVolumetricCloudLayer(0);
			if (vlayer && vlayer->Settings.Enabled)
			{
				// Copy settings to the mutable cache for renderer use.
				const_cast<Renderer*>(this)->_volumetricCloudSettings = vlayer->Settings;
				return &_volumetricCloudSettings;
			}
		}

		// Check layer 2 as fallback.
		if (levelPtr->HasVolumetricCloudLayer(1))
		{
			auto* vlayer = levelPtr->GetVolumetricCloudLayer(1);
			if (vlayer && vlayer->Settings.Enabled)
			{
				const_cast<Renderer*>(this)->_volumetricCloudSettings = vlayer->Settings;
				return &_volumetricCloudSettings;
			}
		}

		return nullptr;
	}
}
