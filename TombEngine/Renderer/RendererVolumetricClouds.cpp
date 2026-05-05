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
#include "Renderer/VolumetricCloud/CloudNoiseTexture.h"
#include "Renderer/ConstantBuffers/VolumetricCloudBuffer.h"
#include "Renderer/Moon/MoonSettings.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Scripting/Include/ScriptInterfaceLevel.h"
#include "Scripting/Internal/TEN/Flow/Level/FlowLevel.h"
#include "Sound/sound.h"
#include "Sound/sound_effects.h"
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
		_cbVolumetricCloud = CreateConstantBuffer<CVolumetricCloudBuffer>();

		// Render targets are created/resized in ResizeVolumetricCloudTargets().
		ResizeVolumetricCloudTargets();

		// Generate tileable 3D/2D noise textures for the cloud shader.
		_cloudNoiseTextures.Initialize(_graphicsDevice.get());

		// Initialize dual volumetric cloud layer B targets.
		InitializeDualVolumetricClouds();
	}

	void Renderer::ResizeVolumetricCloudTargets()
	{
		// Determine cloud render resolution based on current quality settings.
		float scale = _cloudState.ActiveQuality.RenderResolutionScale;
		int w = std::max(1, (int)(_graphicsDevice->GetScreenWidth() * scale));
		int h = std::max(1, (int)(_graphicsDevice->GetScreenHeight() * scale));

		// Half-res RGBA16F target for cloud color + opacity.
		_cloudRenderTarget = _graphicsDevice->CreateRenderSurface2D(
			w, h,
			SurfaceFormat::SF_RGBA16_Float,
			false,   // not typeless
			DepthFormat::None);  // no depth

		// Previous frame's cloud result for temporal checkerboard reprojection.
		// Same size/format as the main cloud RT; holds last frame's fully-resolved
		// half-res cloud image so the shader can reuse it for skipped checkerboard pixels.
		_cloudPrevFrameRT = _graphicsDevice->CreateRenderSurface2D(
			w, h,
			SurfaceFormat::SF_RGBA16_Float,
			false,   // not typeless
			DepthFormat::None);  // no depth

		// 1x1 target for lens flare occlusion transmittance readback.
		_cloudOcclusionTarget = _graphicsDevice->CreateRenderSurface2D(
			1, 1,
			SurfaceFormat::SF_R16_Float,
			false,
			DepthFormat::None);

		// Async readback buffer paired with the occlusion target.
		_cloudOcclusionReadback = _graphicsDevice->CreateGpuReadbackBuffer(
			1, 1, SurfaceFormat::SF_R16_Float);

		// Full-res backup of the scene (sky) before cloud compositing.
		// Needed so the cloud composite shader can read the background
		// and compute hybrid screen/alpha blending in-shader.
		_scenePreCloudBackup = _graphicsDevice->CreateRenderSurface2D(
			_graphicsDevice->GetScreenWidth(), _graphicsDevice->GetScreenHeight(),
			SurfaceFormat::SF_RGBA8_Unorm,
			false,
			DepthFormat::None);
	}

	// ========================================================================
	// Per-frame constant buffer update
	// ========================================================================

	void Renderer::UpdateVolumetricCloudBuffer(const CloudRenderSettings& settings,
	                                             const CloudRuntimeState& runtimeState,
	                                             RenderView& view)
	{
		// Use runtimeState.ActiveQuality (not _cloudState) so each layer's own quality
		// settings are applied — fixes CloudB always reading CloudA's quality which
		// caused TemporalEnabled/FrameIndex to always use CloudA's FrameCounter (= 0).
		const auto& q = runtimeState.ActiveQuality;

		_stVolumetricCloud.CloudBottomHeight = settings.CloudBottomHeight;
		_stVolumetricCloud.CloudTopHeight    = settings.CloudBottomHeight + settings.CloudThickness;
		_stVolumetricCloud.CloudThickness    = settings.CloudThickness;
		_stVolumetricCloud.Coverage          = settings.Coverage;

		_stVolumetricCloud.CloudDensity  = 0.0f;  // No-op for AltocumulusMid.
		_stVolumetricCloud.ShapeScale    = 0.0f;
		_stVolumetricCloud.DetailScale   = 0.0f;
		_stVolumetricCloud.DetailStrength = 0.0f;

		_stVolumetricCloud.WeatherScale      = 0.00002f; // Default; used for lightning coverage suppression.
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
		_stVolumetricCloud.DebugView          = (int)runtimeState.DebugView;

		float scale = q.RenderResolutionScale;
		float w = (float)std::max(1, (int)(_graphicsDevice->GetScreenWidth() * scale));
		float h = (float)std::max(1, (int)(_graphicsDevice->GetScreenHeight() * scale));
		_stVolumetricCloud.CloudRenderSize    = Vector2(w, h);
		_stVolumetricCloud.InvCloudRenderSize = Vector2(1.0f / w, 1.0f / h);

		// TemporalEnabled encoding:
		//   0 = temporal off (always raymarch every pixel)
		//   1 = temporal on, warmup active (copy prev-frame RT but no skip yet)
		//   2 = temporal on, warmup done (checkerboard skip active)
		// Use runtimeState (not _cloudState) so each layer tracks its own frame counter.
		// Previously hardcoded to _cloudState caused CloudB to always read FrameCounter=0
		// (CloudA inactive) → TemporalEnabled stuck at 1 (WARMUP) every frame → Prio 4 dead.
		_stVolumetricCloud.TemporalEnabled = q.TemporalReprojection
		                                   ? (runtimeState.FrameCounter > 1 ? 2 : 1)
		                                   : 0;
		_stVolumetricCloud.FrameIndex      = (float)(runtimeState.FrameCounter % 256);

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
		_stVolumetricCloud.AltoFbmScale         = settings.AltoFbmScale;
		// On Low quality (DetailNoiseEnabled=false) curl warp is suppressed: it costs
		// 1-3 extra texture fetches per march step and is not worth it at Low fidelity.
		_stVolumetricCloud.CurlWarpStrength     = q.DetailNoiseEnabled ? settings.CurlWarpStrength : 0.0f;
		_stVolumetricCloud.EvoAccumOffset        = runtimeState.EvoAccumOffset;
		_stVolumetricCloud.FlowAccumOffset       = runtimeState.FlowAccumOffset;
		_stVolumetricCloud.WindAccumOffsetScaled = runtimeState.FreezeWind      ? 0.0f : runtimeState.WindAccumOffsetScaled;
		_stVolumetricCloud.EvoAccumOffsetScaled  = runtimeState.FreezeEvolution ? 0.0f : runtimeState.EvoAccumOffsetScaled;
		_stVolumetricCloud._PadRow31_0           = 0.0f;
		_stVolumetricCloud._PadRow31_1           = 0.0f;
		_stVolumetricCloud.UpsampleSpatialSigma2 = settings.UpsampleSpatialSigma2;
		// Quality-dependent widening of the temporal stability bands.
		// Edge pixels with alpha in [Low, High] bypass checkerboard reuse and do
		// a fresh raymarch every frame.  At Low/Medium the per-step count is small,
		// so cloud-space jitter and per-frame Evo/Flow advection produce visible
		// alpha variance at thin cloud borders.  Widening the bands pushes more
		// borderline pixels into the reuse path; motion still refreshes them
		// because windEvoBoost ramps the EMA blend factor automatically.
		float alphaBandShrink = 0.0f;
		if (q.PrimaryStepCount <= 12)
			alphaBandShrink = 0.10f; // Medium: 0.05/0.95 -> 0.15/0.85
		else if (q.PrimaryStepCount <= 8)
			alphaBandShrink = 0.15f; // Low: even wider
		_stVolumetricCloud.TemporalAlphaLow      = settings.TemporalAlphaLow + alphaBandShrink;
		_stVolumetricCloud.TemporalAlphaHigh     = settings.TemporalAlphaHigh - alphaBandShrink;
		_stVolumetricCloud.AltoJitterAbsCap      = settings.AltoJitterAbsCap;

		// Previous frame's ViewProjection for temporal reprojection.
		// Clouds are at infinite distance so only camera rotation matters;
		// the translation component is negligible at sky-dome scale (1e6 units).
		_stVolumetricCloud.PrevViewProjection = runtimeState.PrevViewProjection;

		// Compute per-frame cloud-noise displacement to drive the EMA blend factor
		// and the hard temporal-disable guard.
		// noiseDisplace approximates how far cloud UVs have shifted this frame.
		{
			float deltaWind     = runtimeState.WindAccumOffset - runtimeState.PrevWindAccumOffset;
			float deltaEvo      = runtimeState.EvoAccumOffset  - runtimeState.PrevEvoAccumOffset;
			// CloudMorph progression also evolves the on-screen result every frame.
			// Treating it as motion (analogous to wind) lets the EMA TRACK the morph
			// progression instead of averaging successive morph stages together
			// (which would smear the cross-fade into blur — the "newly-revealed
			// pixels look sharper than the rest" artifact during morphs).
			float deltaDissolve  = settings.DissolvePhase  - runtimeState.PrevDissolvePhase;
			float deltaFormation = settings.FormationPhase - runtimeState.PrevFormationPhase;
			float morphDisplace  = (settings.MorphActive > 0.5f)
			                     ? (std::abs(deltaDissolve) + std::abs(deltaFormation)) * 1.0f
			                     : 0.0f;
			float noiseDisplace = std::abs(deltaWind) * 2.032f
			                    + std::abs(deltaEvo)  * 2.032f
			                    + morphDisplace;

			// Camera rotation:
			// The shader reprojects temporal history through PrevViewProjection so it
			// correctly handles any camera rotation without stale-UV artifacts.  We
			// therefore do NOT hard-disable temporal on camera rotation.
			//
			// We do raise TemporalBlendFactor slightly so that newly-exposed sky regions
			// (frustum edges that were off-screen last frame, where hasValidHistory=false)
			// refresh a bit faster.  However, the boost is capped well below 1.0:
			// reaching 1.0 would produce raw unaccumulated raymarcher output which has a
			// bimodal alpha distribution (pixels either fully hit a cloud or fully miss it).
			// For semi-transparent presets like Cirrustratus, that bimodal output looks
			// dense/opaque — the opposite of the intended transparent veil.  The temporal
			// EMA is what creates the smooth veil by averaging stochastic samples over time.
			//
			// Keep the cap small: PrevViewProjection already corrects existing pixels,
			// so the boost only needs to handle the thin frustum-edge band of newly
			// visible pixels. A larger cap would re-introduce flicker on edge pixels
			// (alpha in [Low, High]) that bypass checkerboard reuse and re-evaluate
			// fresh every frame during rotation.
			//
			// camMotion = 1 - dot(prevFwd, currFwd):
			//   ~0.000610 = 2 deg/frame (120 deg/sec at 60 fps)
			float camDot    = runtimeState.PrevCameraForward.Dot(view.Camera.WorldDirection);
			camDot          = std::max(-1.0f, std::min(1.0f, camDot));
			float camMotion = 1.0f - camDot;

			constexpr float kCamRampEnd  = 0.000610f; // 1 - cos(2 deg)
			// Max blend factor allowed from camera rotation alone.
			// Lowered from 0.65 to 0.25 — reprojection handles the bulk of rotation;
			// only the small frustum-edge band needs faster refresh.
			constexpr float kCamBlendMax = 0.25f;

			// Dynamic EMA blend factor: ramp from the quality-preset base (strong smoothing
			// for static clouds) toward 1.0 as wind/evolution advects cloud content.
			// Camera rotation contributes a smaller, capped boost so temporal history is
			// never fully discarded just because the camera moved.
			const float kBaseBlend = q.TemporalBaseBlend;
			const float kRampEnd   = 0.05f;
			float windEvoBoost = (1.0f - kBaseBlend) * (noiseDisplace / kRampEnd);
			float camBoost     = (kCamBlendMax - kBaseBlend) * std::min(1.0f, camMotion / kCamRampEnd);

			// Note on CloudMorph: morphDisplace above feeds into noiseDisplace,
			// which drives windEvoBoost. This makes the EMA blend factor ramp up
			// during morph so accumulation tracks the morph progression — without
			// it, the EMA would average together multiple morph stages and the
			// result would visibly blur (sharp edges only at frustum-edge regions
			// freshly revealed by camera rotation, then smoothed back into blur).

			float blend = kBaseBlend + std::max(windEvoBoost, camBoost);

			_stVolumetricCloud.TemporalBlendFactor = q.TemporalReprojection
				? std::min(1.0f, blend)
				: 1.0f;

			// Hard guard: disable temporal only when wind/evolution has displaced cloud
			// noise-space UVs so far that the reprojected history no longer matches.
			// Camera rotation is intentionally excluded here — PrevViewProjection handles it.
			if (noiseDisplace > 0.10f)
				_stVolumetricCloud.TemporalEnabled = 0;
		}

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
		UpdateConstantBuffer(&_stVolumetricCloud, _cbVolumetricCloud.get());
	}

	// ========================================================================
	// Main cloud drawing
	// ========================================================================

	// Reproduces the HLSL hash: frac(sin(x) * 43758.5453).
	static float LightningHash(float x)
	{
		float v = std::sin(x) * 43758.5453f;
		return v - std::floor(v);
	}

	// Checks if a bolt fires for the current flash cycle and, if so, schedules a
	// thunder sound delayed by the estimated horizontal distance to the strike.
	void Renderer::UpdateLightningThunder(const CloudRenderSettings& settings, CloudRuntimeState& state, float dt)
	{
		// Tick down any pending thunder sound.
		if (state.LightningThunderCountdown >= 0.0f)
		{
			state.LightningThunderCountdown -= dt;
			if (state.LightningThunderCountdown <= 0.0f)
			{
				SoundEffect(SFX_TR4_THUNDER_RUMBLE, nullptr);
				state.LightningThunderCountdown = -1.0f;
			}
		}

		float flashCycle = std::floor(state.AccumulatedTime * settings.LightningInternalSpeed);

		// Initialize on first run to avoid spurious thunder on game start.
		if (state.LightningPrevFlashCycle < 0.0f)
		{
			state.LightningPrevFlashCycle = flashCycle;
			return;
		}

		if (flashCycle == state.LightningPrevFlashCycle)
			return;

		state.LightningPrevFlashCycle = flashCycle;

		// Check if this cycle fires a visible bolt (identical conditions to the shader).
		float flashRand = LightningHash(flashCycle * 127.1f + 311.7f);
		if (flashRand >= settings.LightningInternalFreq)
			return;

		float boltGateRand = LightningHash(flashCycle * 179.3f + 43.7f);
		if (boltGateRand >= settings.LightningStrikeFreq)
			return;

		// Compute bolt sky-space XZ position (camera-relative, identical to shader).
		float altoExtentXZ = std::max(settings.CloudBottomHeight * 0.8f, 25000.0f);
		float boltSkyX = (LightningHash(flashCycle * 73.1f + 1.3f) - 0.5f) * altoExtentXZ;
		float boltSkyZ = (LightningHash(flashCycle * 53.3f + 3.7f) - 0.5f) * altoExtentXZ;

		float boltDist     = std::sqrt(boltSkyX * boltSkyX + boltSkyZ * boltSkyZ);
		float maxDist      = altoExtentXZ * 0.5f * 1.41421356f;
		float boltDistNorm = std::min(boltDist / std::max(maxDist, 1.0f), 1.0f);

		// Schedule thunder: 1 second for a nearby strike, up to 8 seconds for a distant one.
		// Only one thunder event is pending at a time; further bolts are silently skipped.
		if (state.LightningThunderCountdown < 0.0f)
			state.LightningThunderCountdown = 1.0f + boltDistNorm * 7.0f;
	}

	// ========================================================================
	// Lens flare occlusion
	// ========================================================================

	void Renderer::UpdateCloudLensFlareOcclusion(RenderView& renderView)
	{
		// Legacy single-layer path. Delegates to the unified implementation;
		// when GetActiveVolumetricCloudSettings() returns nullptr the call
		// returns 1.0f (no occlusion) without touching the GPU.
		const CloudRenderSettings* activeSettings = GetActiveVolumetricCloudSettings();
		if (activeSettings == nullptr)
		{
			_cloudState.FlareOcclusion.SmoothedTransmittance = 1.0f;
			return;
		}

		_cloudState.FlareOcclusion.SmoothedTransmittance = ComputeSingleLayerOcclusion(
			*activeSettings, _cloudState,
			_cloudOcclusionTarget.get(), _cloudRenderTarget.get(),
			_cloudOcclusionReadback.get(), renderView);
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
		// Legacy single-layer Lua path removed; cloud rendering now flows exclusively
		// through g_SkyCloudSystem (level.dynamicSky.Clouds in Gameflow.lua).
		return nullptr;
	}
}
