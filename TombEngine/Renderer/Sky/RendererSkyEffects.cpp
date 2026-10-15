#include "framework.h"
#include "Renderer/Renderer.h"

#include "Game/camera.h"
#include "Game/Lara/lara.h"
#include "Game/Sky/SkyCloudSystem.h"
#include "Renderer/Backend/SceneRenderer.h"
#include "Renderer/Sky/SkyQuality.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Scripting/Include/ScriptInterfaceLevel.h"
#include "Specific/level.h"

// CPU side of the sky effects: advances the cloud, atmosphere, god ray and dust storm state once per frame, fills
// their uniforms and records the draws the scene renderer replays. The backend sky passes only read these records.

using namespace TEN::Renderer::VolumetricCloud;
using namespace TEN::Sky;

namespace TEN::Renderer
{
	bool Renderer::IsSkyboxVisible(const RenderView& view) const
	{
		for (const auto* room : view.RoomsToDraw)
		{
			if (g_Level.Rooms[room->RoomNumber].flags & ENV_FLAG_SKYBOX)
				return true;
		}

		return false;
	}

	bool Renderer::IsHorizonLayerDrawn(int layer)
	{
		auto* level = g_GameFlow->GetLevel(CurrentLevel);
		return (level->GetHorizonEnabled(layer) && level->GetHorizonTransparency(layer) > EPSILON &&
				_moveableObjects[level->GetHorizonObjectID(layer)].has_value());
	}

	Matrix Renderer::GetHorizonWorldMatrix(int layer, const RenderView& view)
	{
		auto* level = g_GameFlow->GetLevel(CurrentLevel);
		auto pos = Vector3::Lerp(level->GetHorizonPrevPosition(layer), level->GetHorizonPosition(layer), GetInterpolationFactor());
		auto orient = EulerAngles::Lerp(level->GetHorizonPrevOrientation(layer), level->GetHorizonOrientation(layer), GetInterpolationFactor());
		return (orient.ToRotationMatrix() * Matrix::CreateTranslation(pos) * Matrix::CreateTranslation(view.Camera.WorldPosition));
	}

	void Renderer::PrepareSkyEffects(RenderView& view)
	{
		_cloudDraws.clear();
		_skyEffects = {};
		_godRayDraw = {};
		_horizonLayers = {};
		_dustStormDraw = {};

		if (IsSkyboxVisible(view))
		{
			if (Lara.Control.Look.OpticRange != 0)
				AlterFOV(ANGLE(DEFAULT_FOV) - Lara.Control.Look.OpticRange, false);

			PrepareAtmosphericSky(view);
			PrepareCloudLayers(view);
			PrepareHorizonLayers(view);
			PrepareCloudBleed(view);
		}

		PrepareGodRays(view);
		PrepareDustStorm(view);
	}

	void Renderer::PrepareAtmosphericSky(RenderView& view)
	{
		// The dome follows the settings before the update, which may enable it for the next frame.
		bool isDomeDrawn = _atmosphericSkySettings.Enabled;
		UpdateAtmosphericSkyBuffer(view);

		_skyEffects.IsDomeDrawn = isDomeDrawn;
		_skyEffects.IsSunMoonDiscDrawn = isDomeDrawn && !g_SkyCloudSystem.IsUnderwaterSkyPresetActive();
		_skyEffects.IsAuroraDrawn = _auroraSettings.Enabled && _stAtmosphericSky.AuroraVisibility >= 0.001f;
		_skyEffects.IsAuroraHalfResolution = _skyEffects.IsAuroraDrawn && _stAtmosphericSky.SkyQualityLevel < 0.5f;
	}

	void Renderer::PrepareCloudLayers(RenderView& view)
	{
		if (!g_SkyCloudSystem.IsCloudAActive() && !g_SkyCloudSystem.IsCloudBActive())
			return;

		for (int layer = 0; layer < 2; layer++)
		{
			auto& state = (layer == 0) ? _cloudState : _cloudStateB;
			bool isActive = (layer == 0) ? g_SkyCloudSystem.IsCloudAActive() : g_SkyCloudSystem.IsCloudBActive();

			// An inactive layer restarts with fresh history when it becomes active again.
			if (!isActive)
			{
				_cloudDraws.push_back(Backend::CloudLayerDraw{ layer, true });
				state.FrameCounter = 0;
				state.PrevCloudType = -1;
				g_SkyCloudSystem.SetLayerTransmittance(layer, 1.0f);
				continue;
			}

			auto settings = (layer == 0) ? g_SkyCloudSystem.GetCloudARenderSettings() : g_SkyCloudSystem.GetCloudBRenderSettings();
			if (!PrepareCloudLayer(settings, state, layer, view, false))
			{
				g_SkyCloudSystem.SetLayerTransmittance(layer, 1.0f);
				continue;
			}

			UpdateCloudOcclusion(layer, state, _cloudDraws.back());
			g_SkyCloudSystem.SetLayerTransmittance(layer, state.FlareOcclusion.SmoothedTransmittance);
		}
	}

	// Fills the layer uniforms and records its draw; the bleed overlay drawn over the horizons does not advance the state.
	bool Renderer::PrepareCloudLayer(const CloudRenderSettings& settings, CloudRuntimeState& state, int layer, RenderView& view, bool isBleed)
	{
		if (!settings.Enabled || settings.CloudType == (int)CloudCategory::Aurora || settings.Coverage < 0.001f)
			return false;

		state.ActiveQuality = GetQualityParams(g_SkyCloudSystem.GetGlobalQuality());

		// A cloud type change invalidates the history, except during a morph, which blends both types continuously.
		bool isHistoryReset = false;
		if (state.PrevCloudType >= 0 && state.PrevCloudType != settings.CloudType && settings.MorphActive <= 0.5f)
		{
			isHistoryReset = true;
			state.FrameCounter = 0;
		}

		state.PrevCloudType = settings.CloudType;

		if (!isBleed)
			AdvanceCloudLayer(settings, state);

		// The buffer reads the previous values, which are stored afterwards.
		UpdateVolumetricCloudBuffer(settings, state, view);

		auto& draw = _cloudDraws.emplace_back();
		draw.Layer = layer;
		draw.IsHistoryReset = isHistoryReset;
		draw.IsRaymarched = true;
		draw.IsHistoryUsed = !isBleed && state.ActiveQuality.TemporalReprojection;
		draw.IsAfterHorizons = isBleed;
		draw.Uniforms = _stVolumetricCloud;

		if (!isBleed)
		{
			state.PrevCameraForward = view.Camera.WorldDirection;
			state.PrevAccumulatedTime = state.AccumulatedTime;
			state.PrevWindAccumOffset = state.WindAccumOffset;
			state.PrevEvoAccumOffset = state.EvoAccumOffset;
			state.PrevDissolvePhase = settings.DissolvePhase;
			state.PrevFormationPhase = settings.FormationPhase;
			state.PrevViewProjection = view.Camera.ViewProjection;
		}

		return true;
	}

	// Wind, evolution and flow offsets are integrated per frame, so changing speeds between presets never rescales the
	// motion of past frames. The 0.05 and 0.16 factors are the historical shader multipliers.
	void Renderer::AdvanceCloudLayer(const CloudRenderSettings& settings, CloudRuntimeState& state)
	{
		float deltaTime = 1.0f / GetFrameRate();
		float fbmScale = std::max(settings.AltoFbmScale, 0.001f);

		if (!state.FreezeEvolution)
		{
			state.AccumulatedTime += deltaTime;
			state.EvoAccumOffset += settings.EvolutionSpeed * deltaTime * 0.05f;
			state.FlowAccumOffset += settings.EvolutionSpeed * deltaTime * 0.16f;
			state.EvoAccumOffsetScaled += settings.EvolutionSpeed * fbmScale * deltaTime * 0.05f;
		}

		if (!state.FreezeWind)
		{
			state.WindAccumOffset += settings.WindSpeed * deltaTime;
			state.WindAccumOffsetScaled += settings.WindSpeed * fbmScale * deltaTime;
		}

		state.FrameCounter++;

		if (settings.LightningEnabled && settings.CloudType == (int)CloudCategory::AltocumulusMid)
			UpdateLightningThunder(settings, state, deltaTime);
	}

	// Smooths the sun transmittance the scene renderer reads back and asks for a new reading every few frames.
	void Renderer::UpdateCloudOcclusion(int layer, CloudRuntimeState& state, Backend::CloudLayerDraw& draw)
	{
		constexpr auto SMOOTH_FACTOR			 = 0.6f;
		constexpr auto OCCLUSION_UPDATE_INTERVAL = 3;

		auto& occlusion = state.FlareOcclusion;

		float transmittance = 1.0f;
		if (Backend::g_SceneRenderer.ReadCloudTransmittance(layer, transmittance))
		{
			occlusion.CloudTransmittance = transmittance;
			occlusion.SmoothedTransmittance += (transmittance - occlusion.SmoothedTransmittance) * SMOOTH_FACTOR;
		}

		occlusion.CacheValidFrames++;
		if (occlusion.CacheValidFrames < OCCLUSION_UPDATE_INTERVAL)
			return;

		occlusion.CacheValidFrames = 0;
		draw.IsOcclusionQueried = true;
	}

	void Renderer::PrepareHorizonLayers(RenderView& view)
	{
		auto* level = g_GameFlow->GetLevel(CurrentLevel);
		const auto& cloudState = g_SkyCloudSystem.GetCurrentState();

		float maxBleed = std::max(cloudState.CloudA.HorizonMeshBleed, cloudState.CloudB.HorizonMeshBleed);
		float maxBleedDepth = std::max(cloudState.CloudA.AltoBleedDepth, cloudState.CloudB.AltoBleedDepth);
		float gradientA = (cloudState.CloudA.Category == CloudCategory::AltocumulusMid) ? cloudState.CloudA.AltoHorizonGradientFade : 0.0f;
		float gradientB = (cloudState.CloudB.Category == CloudCategory::AltocumulusMid) ? cloudState.CloudB.AltoHorizonGradientFade : 0.0f;
		float maxGradient = std::max(gradientA, gradientB);

		for (int layer = 0; layer < 2; layer++)
		{
			if (!IsHorizonLayerDrawn(layer))
				continue;

			auto& horizon = _horizonLayers[layer];
			horizon.IsDrawn = true;
			horizon.Alpha = level->GetHorizonTransparency(layer);
			horizon.GradientFade = maxGradient;
			horizon.GradientRise = _atmosphericSkySettings.HorizonGradientRise[layer];

			// Full opacity would overwrite the bleed overlay and the gradients; 0.99 blends and looks the same.
			if ((maxBleed > 0.001f || maxBleedDepth > 0.001f || maxGradient > 0.001f || horizon.GradientRise > 0.001f) && horizon.Alpha >= 1.0f)
				horizon.Alpha = 0.99f;

			if (horizon.GradientFade > 0.001f || horizon.GradientRise > 0.001f)
				GetHorizonMeshHeightRange(layer, view, horizon.MeshWorldYMin, horizon.MeshWorldYRange);
		}
	}

	// Camera relative height range of the transformed horizon vertices; the bounding sphere of wide horizon rings is
	// dominated by their width.
	void Renderer::GetHorizonMeshHeightRange(int layer, const RenderView& view, float& yMin, float& yRange)
	{
		auto* level = g_GameFlow->GetLevel(CurrentLevel);
		const auto& object = *_moveableObjects[level->GetHorizonObjectID(layer)];
		auto world = GetHorizonWorldMatrix(layer, view);

		float minY = FLT_MAX;
		float maxY = -FLT_MAX;
		for (const auto* mesh : object.ObjectMeshes)
		{
			for (const auto& position : mesh->Positions)
			{
				float y = Vector3::Transform(position, world).y - view.Camera.WorldPosition.y;
				minY = std::min(minY, y);
				maxY = std::max(maxY, y);
			}
		}

		yMin = minY;
		yRange = std::max(maxY - minY, 1.0f);
	}

	// Altocumulus layers are drawn again over the horizon meshes at the bleed intensity, only when a horizon is visible.
	void Renderer::PrepareCloudBleed(RenderView& view)
	{
		if (!_horizonLayers[0].IsDrawn && !_horizonLayers[1].IsDrawn)
			return;

		const auto& cloudState = g_SkyCloudSystem.GetCurrentState();
		for (int layer = 0; layer < 2; layer++)
		{
			bool isActive = (layer == 0) ? g_SkyCloudSystem.IsCloudAActive() : g_SkyCloudSystem.IsCloudBActive();
			if (!isActive)
				continue;

			auto settings = (layer == 0) ? g_SkyCloudSystem.GetCloudARenderSettings() : g_SkyCloudSystem.GetCloudBRenderSettings();
			float meshBleed = (layer == 0) ? cloudState.CloudA.HorizonMeshBleed : cloudState.CloudB.HorizonMeshBleed;
			if (!settings.Enabled || settings.CloudType != (int)CloudCategory::AltocumulusMid)
				continue;

			float bleedIntensity = std::clamp(settings.AltoBleedDepth / 100.0f, 0.0f, 1.0f);
			float bleed = std::max(bleedIntensity, meshBleed);
			if (bleed < 0.001f)
				continue;

			// Deeper bleed pushes the clouds toward the zenith with a softer height ramp, so they flood down over the horizon.
			settings.AltoZenithBias += (-1.0f - settings.AltoZenithBias) * bleedIntensity;
			settings.AltoHeightBlendPower += (1.470f - settings.AltoHeightBlendPower) * bleedIntensity;
			settings.BleedPassStrength = bleed;

			auto& state = (layer == 0) ? _cloudState : _cloudStateB;
			PrepareCloudLayer(settings, state, layer, view, true);
		}
	}

	void Renderer::PrepareGodRays(RenderView& view)
	{
		if (!AreGodRaysEnabled())
			return;

		UpdateGodRayBuffer(view);

		// A sun behind the camera has the -10 sentinel; off screen suns in front still cast rays from the edge.
		if (_stGodRay.SunScreenPos.x < -5.0f)
			return;

		_godRayDraw.IsDrawn = true;
		_godRayDraw.Uniforms = _stGodRay;
	}

	// God rays need a sun or moon ray source, a preset that shows the sun and cloud layers for the occlusion mask; the
	// underwater sky always shows its shafts.
	bool Renderer::AreGodRaysEnabled()
	{
		auto* level = g_GameFlow->GetLevel(CurrentLevel);

		float sunElevation = 1.0f;
		if (level->GetLensFlareEnabled())
			sunElevation = std::sin(TO_RAD(level->GetLensFlarePitch()));

		bool useMoonRays = _moonSettings.Enabled && _moonSettings.GodRays.Enabled && ComputeDayNightBlend(sunElevation) > 0.5f;
		if (!useMoonRays && !_godRaySettings.Enabled)
			return false;

		if (_stAtmosphericSky.UnderwaterSkyVisibility > 0.001f)
			return true;

		const auto* preset = g_SkyCloudSystem.GetPresetDefinition(g_SkyCloudSystem.GetCurrentPreset());
		if (preset != nullptr && !preset->GodRaysEnabled)
			return false;

		// Aurora layers are light effects without cloud geometry, so they give no occlusion mask.
		bool hasCloudsA = g_SkyCloudSystem.IsCloudAActive() && g_SkyCloudSystem.GetCloudARenderSettings().CloudType != (int)CloudCategory::Aurora;
		bool hasCloudsB = g_SkyCloudSystem.IsCloudBActive() && g_SkyCloudSystem.GetCloudBRenderSettings().CloudType != (int)CloudCategory::Aurora;
		return (hasCloudsA || hasCloudsB);
	}

	void Renderer::PrepareDustStorm(RenderView& view)
	{
		if (!_dustStormSettings.Enabled)
			return;

		UpdateDustStormBuffer(view);
		_dustStormDraw = Backend::DustStormDraw{ true, _stDustStorm };
	}
}
