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
#include "Renderer/Moon/MoonSettings.h"
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

		// Initialize dual volumetric cloud layer B targets.
		InitializeDualVolumetricClouds();
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

		// Full-res backup of the scene (sky) before cloud compositing.
		// Needed so the cloud composite shader can read the background
		// and compute hybrid screen/alpha blending in-shader.
		_scenePreCloudBackup = RenderTarget2D(
			_device.Get(), _screenWidth, _screenHeight,
			DXGI_FORMAT_R8G8B8A8_UNORM,
			false,
			DXGI_FORMAT_UNKNOWN);
	}

	// ========================================================================
	// Per-frame constant buffer update
	// ========================================================================

	void Renderer::UpdateVolumetricCloudBuffer(const CloudRenderSettings& settings,
	                                             const CloudRuntimeState& runtimeState,
	                                             RenderView& view)
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
		// WindSpeed CB slot carries the pre-integrated wind offset (not speed).
		// This avoids the backwards-motion artifact: when WindSpeed transitions to a
		// lower value, CloudTime*WindSpeed_new < CloudTime*WindSpeed_old causing
		// clouds to jump backwards. With accumulation the offset only grows or slows.
		_stVolumetricCloud.WindSpeed         = runtimeState.FreezeWind ? 0.0f : runtimeState.WindAccumOffset;
		_stVolumetricCloud.EvolutionSpeed    = runtimeState.FreezeEvolution ? 0.0f : settings.EvolutionSpeed;

		_stVolumetricCloud.WindDirection     = settings.WindDirection;
		_stVolumetricCloud.Time              = runtimeState.AccumulatedTime;
		_stVolumetricCloud.JitterStrength    = settings.JitterStrength;

		// Light direction: prefer existing lens flare direction, fallback to settings.
		auto* levelPtr = g_GameFlow->GetLevel(CurrentLevel);
		Vector3 sunLightDir(0.3f, 0.85f, 0.2f);
		Vector3 sunLightColor(1.0f, 0.95f, 0.85f);
		float   sunElevLens = 1.0f;

		if (levelPtr->GetLensFlareEnabled())
		{
			// GetLensFlarePitch/Yaw return TEN short angles (65536 = 360 degrees).
			constexpr float SHORT_TO_RAD = (DirectX::XM_2PI / 65536.0f);
			float pitch = (float)levelPtr->GetLensFlarePitch() * SHORT_TO_RAD;
			float yaw   = (float)levelPtr->GetLensFlareYaw()   * SHORT_TO_RAD;

			// Negate Y so the direction points toward the sun in TEN's Y-down world
			// (positive pitch = sun above horizon = negative Y = up).
			sunLightDir = Vector3(
				std::cos(pitch) * std::sin(yaw),
				-std::sin(pitch),
				std::cos(pitch) * std::cos(yaw));
			sunLightDir.Normalize();

			auto flareColor = levelPtr->GetLensFlareEvaluatedColor();
			sunLightColor = Vector3(
				flareColor.x,
				flareColor.y,
				flareColor.z);

			// Apply atmospheric sky gradient so cloud light matches the sky dome.
			const auto& atmo = _atmosphericSkySettings;
			sunElevLens = std::sin(pitch);
			float sunInfl = std::max(0.0f, 1.0f - sunElevLens * atmo.SunElevationRampSpeed);
			float blend   = sunInfl * atmo.SunWarmInfluence;
			sunLightColor.x = 1.0f + (sunLightColor.x - 1.0f) * blend;
			sunLightColor.y = 1.0f + (sunLightColor.y - 1.0f) * blend;
			sunLightColor.z = 1.0f + (sunLightColor.z - 1.0f) * blend;
		}
		else if (settings.LightDirection.LengthSquared() > 0.001f)
		{
			sunLightDir = settings.LightDirection;
			sunLightDir.Normalize();
			sunLightColor = Vector3(1.0f, 0.95f, 0.85f);
		}
		else
		{
			sunLightDir.Normalize();
		}

		// --- Moon/night cloud lighting blend ---
		// At nighttime, blend the cloud light source from sun to moon.
		float dayNightBlend = ComputeDayNightBlend(sunElevLens);
		const auto& moon = _moonSettings;

		if (moon.Enabled && dayNightBlend > 0.01f)
		{
			// Build moon direction from pitch/yaw.
			float moonPitchRad = moon.Pitch * (DirectX::XM_PI / 180.0f);
			float moonYawRad   = moon.Yaw   * (DirectX::XM_PI / 180.0f);
			Vector3 moonLightDir(
				std::cos(moonPitchRad) * std::sin(moonYawRad),
				-std::sin(moonPitchRad),
				std::cos(moonPitchRad) * std::cos(moonYawRad));
			moonLightDir.Normalize();

			// Moon phase brightness (same computation as in RendererAtmosphericSky.cpp).
			float moonPhase = ComputeMoonPhase(sunLightDir, moonLightDir);
			float phaseBrightness = moonPhase * moonPhase * (3.0f - 2.0f * moonPhase);

			// Moonlight color: cool bluish-white, dimmed by phase.
			Vector3 moonLightColor(
				moon.BaseColorR * moon.CloudLightIntensity * phaseBrightness,
				moon.BaseColorG * moon.CloudLightIntensity * phaseBrightness,
				moon.BaseColorB * moon.CloudLightIntensity * phaseBrightness);

			// Blend direction and color from sun to moon based on day/night factor.
			_stVolumetricCloud.LightDirection = Vector3::Lerp(sunLightDir, moonLightDir, dayNightBlend);
			_stVolumetricCloud.LightDirection.Normalize();
			_stVolumetricCloud.LightColor = Vector3::Lerp(sunLightColor, moonLightColor, dayNightBlend);
		}
		else
		{
			// Day or moon disabled: use sun lighting.
			_stVolumetricCloud.LightDirection = sunLightDir;
			_stVolumetricCloud.LightColor     = sunLightColor;
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

		// Fading parameters.
		_stVolumetricCloud.HorizonFade   = settings.HorizonFade;
		_stVolumetricCloud.DistanceFade  = settings.DistanceFade;
		_stVolumetricCloud.CloudType     = settings.CloudType;
		// Normal composite pass: full opacity.
		// BleedPassStrength is 0 for normal draws; doBleedOverlay sets it to bleedStrength.
		_stVolumetricCloud.CloudCompositeScale = 1.0f;
		_stVolumetricCloud.CloudIsBleedPass    = settings.BleedPassStrength;

		_stVolumetricCloud.AltoBillowStrength = settings.AltoBillowStrength;
		_stVolumetricCloud.AltoCovSoftWidth   = settings.AltoCovSoftWidth;
		_stVolumetricCloud.AltoAbsorption     = settings.AltoAbsorption;
		_stVolumetricCloud.AltoCloudSize      = settings.AltoCloudSize;

		_stVolumetricCloud.AltoCloudAmount    = settings.AltoCloudAmount;
		_stVolumetricCloud.AltoCloudBrightness = settings.AltoCloudBrightness;
		_stVolumetricCloud.AltoFbmLacunarity  = settings.AltoFbmLacunarity;
		_stVolumetricCloud.AltoFbmGain        = settings.AltoFbmGain;

		_stVolumetricCloud.AltoCloudColor     = Vector3(settings.AltoCloudColorR,
		                                                settings.AltoCloudColorG,
		                                                settings.AltoCloudColorB);
		_stVolumetricCloud.AltoThickness      = settings.AltoThickness;
		_stVolumetricCloud.AltoCloudColorDark = Vector3(settings.AltoCloudColorDarkR,
		                                                settings.AltoCloudColorDarkG,
		                                                settings.AltoCloudColorDarkB);
		_stVolumetricCloud.AltoBottomSoftness = settings.AltoBottomSoftness;

		// AltocumulusMid sky-height redistribution parameters
		_stVolumetricCloud.AltoZenithBias        = settings.AltoZenithBias;
		_stVolumetricCloud.AltoHorizonWidth      = settings.AltoHorizonWidth;
		_stVolumetricCloud.AltoBleedDepth        = settings.AltoBleedDepth;
		_stVolumetricCloud.DissolvePhase          = settings.DissolvePhase;
		_stVolumetricCloud.AltoHeightBlendPower  = settings.AltoHeightBlendPower;

		// Sun elevation for cloud day/night lighting.
		// Compute from lens flare pitch (same source as LightDirection).
		float cloudSunElev = 1.0f; // Default: high sun.
		if (levelPtr->GetLensFlareEnabled())
		{
			constexpr float SHORT_TO_RAD = (DirectX::XM_2PI / 65536.0f);
			float pitch = (float)levelPtr->GetLensFlarePitch() * SHORT_TO_RAD;
			cloudSunElev = std::sin(pitch);
		}
		_stVolumetricCloud.SunElevation        = cloudSunElev;
		_stVolumetricCloud.CloudNightAmbient   = _atmosphericSkySettings.CloudNightAmbient;
		_stVolumetricCloud.CloudTwilightAmbient = _atmosphericSkySettings.CloudTwilightAmbient;

		// Lightning parameters
		_stVolumetricCloud.LightningEnabled       = settings.LightningEnabled;
		_stVolumetricCloud.LightningStrikeFreq    = settings.LightningStrikeFreq;
		_stVolumetricCloud.LightningInternalFreq  = settings.LightningInternalFreq;
		_stVolumetricCloud.LightningPad           = 0.0f;
		_stVolumetricCloud.LightningSpeed         = settings.LightningSpeed;
		_stVolumetricCloud.LightningInternalSpeed = settings.LightningInternalSpeed;
		_stVolumetricCloud.LightningGlowIntensity = settings.LightningGlowIntensity;
		_stVolumetricCloud.LightningFlashIntensity = settings.LightningFlashIntensity;
		_stVolumetricCloud.LightningBoltColor     = Vector3(settings.LightningBoltColorR,
		                                                    settings.LightningBoltColorG,
		                                                    settings.LightningBoltColorB);
		_stVolumetricCloud.LightningAmbientContrib        = settings.LightningAmbientContrib;
		_stVolumetricCloud.LightningBoltLengthScale    = settings.LightningBoltLengthScale;
		_stVolumetricCloud.LightningBoltThicknessScale = settings.LightningBoltThicknessScale;

		// Atmospheric sun-lighting multipliers (from AtmosphericSkySettings).
		_stVolumetricCloud.CloudSunLightIntensity      = _atmosphericSkySettings.CloudSunLightIntensity;
		_stVolumetricCloud.CloudAmbientIntensity       = _atmosphericSkySettings.CloudAmbientIntensity;
		_stVolumetricCloud.CloudSilverliningStrength   = _atmosphericSkySettings.CloudSilverliningStrength;
		_stVolumetricCloud.CloudForwardScatterStrength = _atmosphericSkySettings.CloudForwardScatterStrength;
		_stVolumetricCloud.CloudLightAbsorption        = _atmosphericSkySettings.CloudLightAbsorption;
		_stVolumetricCloud.CloudSunWarmthInfluence     = _atmosphericSkySettings.CloudSunWarmthInfluence;
		// CloudIsBleedPass is set to 0.0f directly above (normal pass).
		_stVolumetricCloud.DriftOutProgress             = settings.DriftOutProgress;

		// Compositor hybrid-blend thresholds (global, adjustable via debug slider).
		_stVolumetricCloud.BlendThresholdHigh      = settings.BlendThresholdHigh;
		_stVolumetricCloud.BlendThresholdHighWidth = settings.BlendThresholdHighWidth;
		_stVolumetricCloud.BlendThresholdLow       = settings.BlendThresholdLow;

		// Moonlight direct illumination factor for the cloud shader.
		// At night the CPU blends LightDirection/LightColor from sun→moon,
		// but the shader's sunFade kills direct lighting. This factor tells
		// the shader how much direct/silver/forward-scatter to restore.
		{
			float moonFactor = 0.0f;
			if (moon.Enabled && dayNightBlend > 0.01f)
			{
				float moonPitchRad = moon.Pitch * (DirectX::XM_PI / 180.0f);
				float moonYawRad   = moon.Yaw   * (DirectX::XM_PI / 180.0f);
				Vector3 moonLightDir(
					std::cos(moonPitchRad) * std::sin(moonYawRad),
					-std::sin(moonPitchRad),
					std::cos(moonPitchRad) * std::cos(moonYawRad));
				moonLightDir.Normalize();
				float moonPhase       = ComputeMoonPhase(sunLightDir, moonLightDir);
				float phaseBrightness = moonPhase * moonPhase * (3.0f - 2.0f * moonPhase);
				moonFactor = dayNightBlend * phaseBrightness;
			}
			_stVolumetricCloud.CloudMoonLightFactor = moonFactor;
		}

		// ---- Sunset underside cloud lighting ----
		// Compute sunset color gradient and activation intensity based on sun elevation.
		// The effect activates when the sun is low (near horizon or slightly below).
		//
		// Elevation zones (cloudSunElev = sin(pitch)):
		//   0.15 .. 0.05  : early sunset — yellow/amber tones fade in
		//   0.05 .. -0.02 : deep sunset  — orange/red peak intensity
		//  -0.02 .. -0.10 : post-sunset  — red/magenta, fading out
		//  below  -0.10   : night — no sunset underside
		{
			const auto& atmo = _atmosphericSkySettings;
			float sunE = cloudSunElev;

			// Activation envelope: bell-shaped curve centered around sunE ≈ 0.0.
			// Ramps up from 0.15, peaks at ~0.0, fades out below -0.10.
			float sunsetOnset   = std::clamp((0.15f - sunE) * 8.0f,  0.0f, 1.0f);  // 0 at sunE=0.15, 1 at sunE≈0.03
			float sunsetFadeOut = std::clamp((sunE + 0.10f) * 10.0f, 0.0f, 1.0f);  // 1 at sunE>0, 0 at sunE=-0.10
			float sunsetActivation = sunsetOnset * sunsetFadeOut;
			// Smoothstep the activation for a gentle transition.
			sunsetActivation = sunsetActivation * sunsetActivation * (3.0f - 2.0f * sunsetActivation);

			// Color gradient based on sun elevation:
			// Map elevation to a 0→1 parameter where 0 = early sunset (warm yellow)
			// and 1 = deep/post-sunset (magenta/pink).
			float colorT = std::clamp((0.10f - sunE) * 6.0f, 0.0f, 1.0f); // 0 at sunE=0.10, 1 at sunE≈-0.07

			// Four-stop color gradient: yellow → orange → red → magenta
			Vector3 cYellow (1.0f,  0.85f, 0.35f);
			Vector3 cOrange (1.0f,  0.55f, 0.15f);
			Vector3 cRed    (1.0f,  0.25f, 0.10f);
			Vector3 cMagenta(0.90f, 0.25f, 0.45f);

			Vector3 sunsetColor;
			if (colorT < 0.333f)
			{
				float s = colorT / 0.333f;
				sunsetColor = Vector3::Lerp(cYellow, cOrange, s);
			}
			else if (colorT < 0.666f)
			{
				float s = (colorT - 0.333f) / 0.333f;
				sunsetColor = Vector3::Lerp(cOrange, cRed, s);
			}
			else
			{
				float s = (colorT - 0.666f) / 0.334f;
				sunsetColor = Vector3::Lerp(cRed, cMagenta, s);
			}

			_stVolumetricCloud.SunsetUndersideColor     = sunsetColor;
			_stVolumetricCloud.SunsetUndersideIntensity  = sunsetActivation * atmo.SunsetUndersideIntensity;
			_stVolumetricCloud.SunsetUndersideSpread     = atmo.SunsetUndersideSpread;
			_stVolumetricCloud.SunsetUndersideHeightFade = atmo.SunsetUndersideHeightFade;
			_stVolumetricCloud.SunsetPad0 = 0.0f;
			_stVolumetricCloud.FormationPhase = settings.FormationPhase;
		}

		// CloudMorph dual-density source params.
		_stVolumetricCloud.MorphSrcCloudSize    = settings.MorphSrcCloudSize;
		_stVolumetricCloud.MorphSrcCloudAmount  = settings.MorphSrcCloudAmount;
		_stVolumetricCloud.MorphSrcBillowStr    = settings.MorphSrcBillowStr;
		_stVolumetricCloud.MorphSrcCovSoftWidth = settings.MorphSrcCovSoftWidth;
		_stVolumetricCloud.MorphSrcFbmLac       = settings.MorphSrcFbmLac;
		_stVolumetricCloud.MorphSrcFbmGain      = settings.MorphSrcFbmGain;
		_stVolumetricCloud.MorphSrcBottomSoft   = settings.MorphSrcBottomSoft;
		_stVolumetricCloud.MorphSrcZenithBias   = settings.MorphSrcZenithBias;
		_stVolumetricCloud.MorphSrcEvolutionSpd = settings.MorphSrcEvolutionSpd;
		_stVolumetricCloud.MorphSrcHorizonWidth = settings.MorphSrcHorizonWidth;
		_stVolumetricCloud.MorphActive          = settings.MorphActive;
		_stVolumetricCloud.MorphPad0            = 0.0f;

		// Project the global lens flare's world position to screen UV so PSCloudOcclusion
		// can sample the cloud render target around the sun's actual screen position.
		_stVolumetricCloud.SunScreenUV = Vector2(-1.0f, -1.0f); // default: no sun / off-screen
		if (!view.LensFlaresToDraw.empty() && view.LensFlaresToDraw[0].IsGlobal)
		{
			const auto& sunPos = view.LensFlaresToDraw[0].Position;
			auto clip = Vector4::Transform(
				Vector4(sunPos.x, sunPos.y, sunPos.z, 1.0f),
				view.Camera.ViewProjection);
			if (clip.w > 0.001f)
			{
				float ndcX = clip.x / clip.w;
				float ndcY = clip.y / clip.w;
				if (ndcX > -1.0f && ndcX < 1.0f && ndcY > -1.0f && ndcY < 1.0f)
				{
					_stVolumetricCloud.SunScreenUV = Vector2(
						ndcX *  0.5f + 0.5f,   // U: 0 = left,  1 = right
						ndcY * -0.5f + 0.5f);  // V: 0 = top,   1 = bottom (DX convention)
				}
			}
		}
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
		// AltocumulusMid (CloudType==1) uses its own AltoCloudAmount, not shared Coverage.
		// Aurora (CloudType==2) is rendered by a separate pass — skip here.
		if (activeSettings->CloudType == 2)
			return;
		bool isAlto = (activeSettings->CloudType == 1);
		if (!isAlto && activeSettings->Coverage < 0.001f)
			return;

		// Resolve quality params — resize render target if the resolution scale changed.
		auto newQuality = GetQualityParams(activeSettings->Quality);
		if (newQuality.RenderResolutionScale != _cloudState.ActiveQuality.RenderResolutionScale)
		{
			_cloudState.ActiveQuality = newQuality;
			ResizeVolumetricCloudTargets();
		}
		else
		{
			_cloudState.ActiveQuality = newQuality;
		}

		// Update accumulated times — evolution time and wind offset are accumulated
		// separately so each can be frozen independently, and so wind offset is
		// always monotonically non-decreasing (prevents backwards cloud motion).
		float dt = 1.0f / std::max(_refreshRate, 30);
		if (!_cloudState.FreezeEvolution)
			_cloudState.AccumulatedTime += dt;
		if (!_cloudState.FreezeWind)
			_cloudState.WindAccumOffset += activeSettings->WindSpeed * dt;
		_cloudState.FrameCounter++;

		// Update constant buffer.
		UpdateVolumetricCloudBuffer(*activeSettings, _cloudState, renderView);

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

		if (_atmosphericSkySettings.Enabled)
		{
			auto* atmoSkyBuf = _cbAtmosphericSky.get();
			_context->PSSetConstantBuffers(10, 1, atmoSkyBuf);
		}

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

		// Copy the current scene (sky/stars/horizon) to backup RT so the
		// composite shader can read the background for hybrid blending.
		_context->CopyResource(_scenePreCloudBackup.Texture.Get(),
			_renderTarget.Texture.Get());

		_context->OMSetRenderTargets(1, _renderTarget.RenderTargetView.GetAddressOf(),
			_renderTarget.DepthStencilView.Get());

		// Opaque blend — the shader computes the final composited color itself
		// using hybrid screen/alpha blending with the background texture.
		SetBlendMode(BlendMode::Opaque);

		// Bind atmospheric sky CB so the composite shader can fade thin cloud
		// edges toward the actual sky tint instead of a dark residual color.
		if (_atmosphericSkySettings.Enabled)
		{
			auto* atmoSkyBuf = _cbAtmosphericSky.get();
			_context->PSSetConstantBuffers(10, 1, atmoSkyBuf);
		}

		// Bind cloud render result as texture (t0).
		BindRenderTargetAsTexture(TextureRegister::ColorMap, &_cloudRenderTarget,
			SamplerStateRegister::LinearClamp);

		// Bind scene backup as texture (t3 = ShadowMap slot, safe during cloud pass).
		BindRenderTargetAsTexture(TextureRegister::ShadowMap, &_scenePreCloudBackup,
			SamplerStateRegister::LinearClamp);

		_shaders.Bind(Shader::VolumetricCloudComposite);
		DrawTriangles(3, 0);

		// --- Cleanup: restore all IA and pipeline state changed by the cloud pass ---
		_context->IASetInputLayout(_inputLayout.Get());
		_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// Unbind cloud RT and scene backup SRVs.
		ID3D11ShaderResourceView* nullSRVs[4] = { nullptr, nullptr, nullptr, nullptr };
		_context->PSSetShaderResources((UINT)TextureRegister::ColorMap, 4, nullSRVs);

		// Reset render states.
		SetBlendMode(BlendMode::Opaque);
		SetDepthState(DepthState::Write);
		SetCullMode(CullMode::CounterClockwise);

		// Restore full-res viewport and main render target.
		_context->RSSetViewports(1, &renderView.Viewport);
		_context->OMSetRenderTargets(1, _renderTarget.RenderTargetView.GetAddressOf(),
			_renderTarget.DepthStencilView.Get());
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

		// Bind the cloud half-res render target as t0 so PSCloudOcclusion can
		// sample cloud alpha around the sun's projected screen position.
		BindRenderTargetAsTexture(TextureRegister::ColorMap, &_cloudRenderTarget,
			SamplerStateRegister::LinearClamp);

		_shaders.Bind(Shader::VolumetricCloudOcclusion);
		DrawTriangles(3, 0);

		// Unbind t0 so the cloud RT isn't held as SRV while it may be reused.
		ID3D11ShaderResourceView* nullSRV = nullptr;
		_context->PSSetShaderResources((UINT)TextureRegister::ColorMap, 1, &nullSRV);

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
		constexpr float SMOOTH_FACTOR = 0.6f;
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
