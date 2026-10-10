#include "framework.h"
#include "Renderer/Renderer.h"

#include "Game/spotcam.h"
#include "Game/Sky/SkyCloudSystem.h"

using namespace TEN::Sky;

namespace TEN::Renderer
{
	float Renderer::GetLensFlareCloudOcclusion() const
	{
		// The cloud layer transmittances are read back by PrepareCloudLayers.
		if (g_SkyCloudSystem.IsCloudAActive() || g_SkyCloudSystem.IsCloudBActive())
			return g_SkyCloudSystem.GetCombinedCloudTransmittance();

		return 1.0f;
	}

	Vector3 Renderer::GetLensFlareColor(const RendererLensFlare& lensFlare, float cloudOcclusion) const
	{
		// Suppress procedural starburst spike and ghost lens artifacts when the scripted lens flare disabled them
		// (e.g. moon-only night levels).
		if (!lensFlare.EffectsEnabled)
			return Vector3::Zero;

		auto flareColor = lensFlare.Color.ToVector3();
		if (!lensFlare.IsGlobal)
			return flareColor;

		// Hide global lens flares while the underwater sky preset is active: the sun is below the surface.
		if (g_SkyCloudSystem.IsUnderwaterSkyPresetActive())
			return Vector3::Zero;

		// Attenuate by cloud transmittance and fade below the horizon regardless of atmospheric sky state.
		// Direction.y = -sin(pitch) in TEN Y-down, so elevation = -Direction.y.
		float sunElevation = -lensFlare.Direction.y;
		float sunBelowFade = std::clamp(1.0f + sunElevation * 8.0f, 0.0f, 1.0f);
		sunBelowFade = sunBelowFade * sunBelowFade * (3.0f - 2.0f * sunBelowFade);
		return flareColor * cloudOcclusion * sunBelowFade;
	}

	PostProcessMode Renderer::GetPostProcessMode()
	{
		return _postProcessMode;
	}

	float Renderer::GetPostProcessStrength()
	{
		return _postProcessStrength;
	}

	Vector3 Renderer::GetPostProcessTint()
	{
		return _postProcessTint;
	}

	void Renderer::SetPostProcessMode(PostProcessMode mode)
	{
		_postProcessMode = mode;
	}

	void Renderer::SetPostProcessStrength(float strength)
	{
		_postProcessStrength = strength;
	}

	void Renderer::SetPostProcessTint(Vector3 tint)
	{
		_postProcessTint = tint;
	}

	DOFState Renderer::GetDOF() const 
	{
		return _lastDOF;
	}

	void Renderer::SetDOF(const DOFState& state, bool save)
	{
		_currentDOF.Mode     = state.Mode;
		_currentDOF.Distance = state.Distance;
		_currentDOF.Range    = std::max(0.0f, state.Range);
		_currentDOF.Strength = std::clamp(state.Strength, 0.0f, 1.0f);

		if (save)
			_lastDOF = _currentDOF;
	}

	void Renderer::RestoreDOF()
	{
		_currentDOF = _lastDOF;
	}

}