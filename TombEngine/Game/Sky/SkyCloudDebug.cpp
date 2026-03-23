// ============================================================================
// SkyCloudDebug.cpp — ImGui debug overlay for the layered sky/cloud system.
//
// Architecture:
//   1. CloudDebugParam metadata describes each slider parameter.
//   2. BuildParamList() populates a vector<CloudDebugParam> from a snapshot.
//   3. DrawParamSlider() renders one slider with mouse-wheel support.
//   4. DrawSkyCloudDebugOverlay() composes the full window with sections:
//        - Weather State  (current/target preset, transition, random)
//        - Preset Switcher (combo box, apply immediate / transition)
//        - Cloud Layer A editor
//        - Cloud Layer B editor
//        - Preset Definition editor (optional)
//
//   Every slider mutates g_SkyCloudSystem's live state directly, so
//   changes are visible in the very next frame — no restart needed.
// ============================================================================

#include "framework.h"
#include "Game/Sky/SkyCloudDebug.h"

#include <imgui.h>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>

#include "Game/control/control.h"
#include "Game/Effects/LensFlareDebug.h"
#include "Renderer/Renderer.h"
#include "Renderer/Aurora/AuroraSettings.h"
#include "Renderer/GodRay/GodRaySettings.h"
#include "Renderer/Moon/MoonSettings.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Scripting/Internal/TEN/Flow/Level/FlowLevel.h"
#include "Specific/level.h"

namespace TEN::Sky
{
	// ====================================================================
	// Helpers
	// ====================================================================

	static const char* CloudCategoryToString(CloudCategory cat)
	{
		switch (cat)
		{
		case CloudCategory::None:                 return "None";
		case CloudCategory::CirrusHigh:           return "CirrusHigh";
		case CloudCategory::AltocumulusMid:       return "AltocumulusMid";
		case CloudCategory::StratocumulusLow:     return "StratocumulusLow";
		case CloudCategory::CumulonimbusVertical:        return "CumulonimbusVertical";
		case CloudCategory::CumulonimbusVerticalBuildUp: return "CumulonimbusVerticalBuildUp";
		case CloudCategory::Aurora:               return "Aurora";
		default:                                         return "Unknown";
		}
	}

	static const char* EasingCurveToString(EasingCurve c)
	{
		switch (c)
		{
		case EasingCurve::Linear:     return "Linear";
		case EasingCurve::SmoothStep: return "SmoothStep";
		case EasingCurve::EaseInOut:  return "EaseInOut";
		case EasingCurve::EaseIn:     return "EaseIn";
		case EasingCurve::EaseOut:    return "EaseOut";
		default:                      return "Unknown";
		}
	}

	// Number of entries in WeatherPresetType (excluding Random and Count).
	static constexpr int REAL_PRESET_COUNT = static_cast<int>(WeatherPresetType::Random);

	// ====================================================================
	// Build parameter list from a VolumetricCloudLayerSnapshot.
	//
	// Each entry stores a pointer into the snapshot, so edits go directly
	// into that snapshot's memory (which IS the live state or a preset).
	// ====================================================================

	static std::vector<CloudDebugParam> BuildParamList(
		VolumetricCloudLayerSnapshot& snap,
		const VolumetricCloudLayerSnapshot* defaults = nullptr)
	{
		auto def = [&](float VolumetricCloudLayerSnapshot::* member) -> float
		{
			return defaults ? defaults->*member : 0.0f;
		};

		const bool isAlto = (snap.Category == CloudCategory::AltocumulusMid);

		std::vector<CloudDebugParam> params;

		//                Label               Ptr                       Min      Max       Step       Fmt         Default
		params.push_back({"Coverage",         &snap.Coverage,           0.0f,    1.0f,     0.01f,    "%.3f",     def(&VolumetricCloudLayerSnapshot::Coverage)});
		params.push_back({"Density",          &snap.Density,            0.0f,    10.0f,    0.1f,     "%.2f",     def(&VolumetricCloudLayerSnapshot::Density)});
		params.push_back({"Bottom Height",    &snap.BottomHeight,       100.0f,  200000.0f, 100.0f,  "%.0f",     def(&VolumetricCloudLayerSnapshot::BottomHeight)});
		if (isAlto)
			params.push_back({"Horizon Width",    &snap.AltoHorizonWidth,   0.0f,    1.0f,     0.01f,    "%.3f",     def(&VolumetricCloudLayerSnapshot::AltoHorizonWidth)});
		else
			params.push_back({"Horizon Width",    &snap.Thickness,          100.0f,  200000.0f, 100.0f,  "%.0f",     def(&VolumetricCloudLayerSnapshot::Thickness)});
		params.push_back({"Wind Dir X",       &snap.WindDirectionX,    -1.0f,    1.0f,     0.01f,    "%.3f",     def(&VolumetricCloudLayerSnapshot::WindDirectionX)});
		params.push_back({"Wind Dir Y",       &snap.WindDirectionY,    -1.0f,    1.0f,     0.01f,    "%.3f",     def(&VolumetricCloudLayerSnapshot::WindDirectionY)});
		params.push_back({"Wind Speed",       &snap.WindSpeed,          0.0f,    8.0f,     0.001f,   "%.4f",     def(&VolumetricCloudLayerSnapshot::WindSpeed)});
		params.push_back({"Evolution Speed",  &snap.EvolutionSpeed,     0.0f,    5.0f,     0.01f,    "%.3f",     def(&VolumetricCloudLayerSnapshot::EvolutionSpeed)});
		// Not used by Alto's self-contained density/lighting path.
		if (!isAlto)
		{
			// Practical ranges for current volumetric shader tuning:
			// ShapeScale remains linear and most useful up to ~0.000135.
			// Above this, the shader applies a soft-cap, so large debug ranges are
			// mostly noise and make the UI harder to tune precisely.
			params.push_back({"Shape Scale",      &snap.ShapeScale,         0.0f,    0.0006f, 0.000001f, "%.6f",     def(&VolumetricCloudLayerSnapshot::ShapeScale)});
			params.push_back({"Detail Scale",     &snap.DetailScale,        0.0f,    0.003f,  0.00001f,  "%.5f",     def(&VolumetricCloudLayerSnapshot::DetailScale)});
			params.push_back({"Detail Strength",  &snap.DetailStrength,     0.0f,    1.0f,     0.01f,    "%.3f",     def(&VolumetricCloudLayerSnapshot::DetailStrength)});
			params.push_back({"Absorption",       &snap.Absorption,         0.0f,    25.0f,    0.1f,     "%.2f",     def(&VolumetricCloudLayerSnapshot::Absorption)});
			params.push_back({"Ambient Contrib",  &snap.AmbientContrib,     0.0f,    1.0f,     0.01f,    "%.3f",     def(&VolumetricCloudLayerSnapshot::AmbientContrib)});
			params.push_back({"Silverlining",     &snap.SilverliningStr,    0.0f,    1.0f,     0.01f,    "%.3f",     def(&VolumetricCloudLayerSnapshot::SilverliningStr)});
		}
		params.push_back({"Horizon Fade",     &snap.HorizonFade,        0.0f,    1.0f,     0.01f,    "%.3f",     def(&VolumetricCloudLayerSnapshot::HorizonFade)});
		params.push_back({"Distance Fade",    &snap.DistanceFade,       0.0f,    1.0f,     0.01f,    "%.3f",     def(&VolumetricCloudLayerSnapshot::DistanceFade)});
		params.push_back({"Horizon Mesh Bleed", &snap.HorizonMeshBleed,  0.0f,    1.0f,     0.01f,    "%.3f",     def(&VolumetricCloudLayerSnapshot::HorizonMeshBleed)});
		// --- Altocumulus-specific parameters (meaningful only when Category == AltocumulusMid) ---
		params.push_back({"Alto Billow Str",  &snap.AltoBillowStrength, 0.0f,    1.0f,     0.01f,    "%.3f",     def(&VolumetricCloudLayerSnapshot::AltoBillowStrength)});
		params.push_back({"Alto Cov Soft W",  &snap.AltoCovSoftWidth,   0.0f,    0.25f,    0.005f,   "%.4f",     def(&VolumetricCloudLayerSnapshot::AltoCovSoftWidth)});
		params.push_back({"Alto Absorption", &snap.AltoAbsorption,      0.1f,    5.0f,     0.05f,    "%.3f",     def(&VolumetricCloudLayerSnapshot::AltoAbsorption)});
		params.push_back({"Alto Cloud Size",  &snap.AltoCloudSize,      0.2f,    5.0f,     0.05f,    "%.3f",     def(&VolumetricCloudLayerSnapshot::AltoCloudSize)});
		params.push_back({"Alto Cloud Amount",&snap.AltoCloudAmount,     0.0f,    1.0f,     0.01f,    "%.3f",     def(&VolumetricCloudLayerSnapshot::AltoCloudAmount)});
		params.push_back({"Alto Brightness",  &snap.AltoCloudBrightness, 0.1f,   4.0f,     0.05f,    "%.3f",     def(&VolumetricCloudLayerSnapshot::AltoCloudBrightness)});
		params.push_back({"Alto Color R",     &snap.AltoCloudColorR,    0.0f,    1.0f,     0.01f,    "%.3f",     def(&VolumetricCloudLayerSnapshot::AltoCloudColorR)});
		params.push_back({"Alto Color G",     &snap.AltoCloudColorG,    0.0f,    1.0f,     0.01f,    "%.3f",     def(&VolumetricCloudLayerSnapshot::AltoCloudColorG)});
		params.push_back({"Alto Color B",     &snap.AltoCloudColorB,    0.0f,    1.0f,     0.01f,    "%.3f",     def(&VolumetricCloudLayerSnapshot::AltoCloudColorB)});
		params.push_back({"Alto Dark Col R",  &snap.AltoCloudColorDarkR, 0.0f,   1.0f,     0.01f,    "%.3f",     def(&VolumetricCloudLayerSnapshot::AltoCloudColorDarkR)});
		params.push_back({"Alto Dark Col G",  &snap.AltoCloudColorDarkG, 0.0f,   1.0f,     0.01f,    "%.3f",     def(&VolumetricCloudLayerSnapshot::AltoCloudColorDarkG)});
		params.push_back({"Alto Dark Col B",  &snap.AltoCloudColorDarkB, 0.0f,   1.0f,     0.01f,    "%.3f",     def(&VolumetricCloudLayerSnapshot::AltoCloudColorDarkB)});
		params.push_back({"Alto FBM Lacun",   &snap.AltoFbmLacunarity,  1.5f,    4.0f,     0.01f,    "%.4f",     def(&VolumetricCloudLayerSnapshot::AltoFbmLacunarity)});
		params.push_back({"Alto FBM Gain",    &snap.AltoFbmGain,        0.1f,    0.9f,     0.01f,    "%.3f",     def(&VolumetricCloudLayerSnapshot::AltoFbmGain)});
		params.push_back({"Alto Thickness",   &snap.AltoThickness,      50.0f, 5000.0f,    50.0f,    "%.0f",     def(&VolumetricCloudLayerSnapshot::AltoThickness)});
		params.push_back({"Alto Bot Soft",     &snap.AltoBottomSoftness,  0.0f,    1.0f,     0.01f,    "%.3f",     def(&VolumetricCloudLayerSnapshot::AltoBottomSoftness)});
		// --- AltocumulusMid cloud distribution: 0=uniform, (+)=more toward horizon, (-)=more toward zenith ---
		params.push_back({"Cloud Distribution", &snap.AltoZenithBias,      -1.0f,   1.0f,     0.01f,    "%.3f",     def(&VolumetricCloudLayerSnapshot::AltoZenithBias)});
		params.push_back({"Alto Height Power",  &snap.AltoHeightBlendPower, 0.25f,  4.0f,     0.05f,    "%.3f",     def(&VolumetricCloudLayerSnapshot::AltoHeightBlendPower)});
		// --- Lightning parameters (Alto and Cumulonimbus) ---
		params.push_back({"Light Strike Freq", &snap.LightningStrikeFreq, 0.0f,    1.0f,     0.01f,    "%.3f",     def(&VolumetricCloudLayerSnapshot::LightningStrikeFreq)});
		params.push_back({"Light Int Freq",    &snap.LightningInternalFreq,0.0f,   1.0f,     0.01f,    "%.3f",     def(&VolumetricCloudLayerSnapshot::LightningInternalFreq)});
		params.push_back({"Light Speed",       &snap.LightningSpeed,      0.1f,    5.0f,     0.05f,    "%.3f",     def(&VolumetricCloudLayerSnapshot::LightningSpeed)});
		params.push_back({"Light Int Speed",   &snap.LightningInternalSpeed,0.1f,  5.0f,     0.05f,    "%.3f",     def(&VolumetricCloudLayerSnapshot::LightningInternalSpeed)});
		params.push_back({"Light Glow Int",    &snap.LightningGlowIntensity,0.0f,  5.0f,     0.1f,     "%.2f",     def(&VolumetricCloudLayerSnapshot::LightningGlowIntensity)});
		params.push_back({"Light Flash Int",   &snap.LightningFlashIntensity,0.0f, 5.0f,     0.1f,     "%.2f",     def(&VolumetricCloudLayerSnapshot::LightningFlashIntensity)});
		params.push_back({"Light Bolt Col R",  &snap.LightningBoltColorR, 0.0f,    1.0f,     0.01f,    "%.3f",     def(&VolumetricCloudLayerSnapshot::LightningBoltColorR)});
		params.push_back({"Light Bolt Col G",  &snap.LightningBoltColorG, 0.0f,    1.0f,     0.01f,    "%.3f",     def(&VolumetricCloudLayerSnapshot::LightningBoltColorG)});
		params.push_back({"Light Bolt Col B",  &snap.LightningBoltColorB, 0.0f,    1.0f,     0.01f,    "%.3f",     def(&VolumetricCloudLayerSnapshot::LightningBoltColorB)});
		params.push_back({"Light Ambient",     &snap.LightningAmbientContrib,    0.0f, 1.0f, 0.01f, "%.3f", def(&VolumetricCloudLayerSnapshot::LightningAmbientContrib)});
		params.push_back({"Light Bolt Length", &snap.LightningBoltLengthScale,    0.1f, 5.0f, 0.05f, "%.2f", def(&VolumetricCloudLayerSnapshot::LightningBoltLengthScale)});
		params.push_back({"Light Bolt Thick",  &snap.LightningBoltThicknessScale, 0.001f, 5.0f, 0.05f, "%.3f", def(&VolumetricCloudLayerSnapshot::LightningBoltThicknessScale)});

		return params;
	}

	// ====================================================================
	// Draw a single slider with mouse-wheel support.
	//
	// - Dragging the slider works as normal.
	// - Hovering the slider and scrolling the mouse wheel adjusts the
	//   value by `param.Step` per wheel notch.
	// - The current value is shown next to the label.
	// - If the value differs from the preset default, it's highlighted.
	// ====================================================================

	static bool DrawParamSlider(CloudDebugParam& param, const char* idPrefix)
	{
		bool changed = false;

		// Highlight if value differs from default.
		bool differs = (std::abs(*param.ValuePtr - param.Default) > param.Step * 0.5f);
		if (differs)
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));

		// Build a unique ID.
		char id[128];
		snprintf(id, sizeof(id), "##%s_%s", idPrefix, param.Label);

		// Slider.
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 180.0f);
		if (ImGui::SliderFloat(id, param.ValuePtr, param.MinValue, param.MaxValue, param.Format))
			changed = true;

		// Mouse wheel while hovered.
		if (ImGui::IsItemHovered())
		{
			float wheel = ImGui::GetIO().MouseWheel;
			if (wheel != 0.0f)
			{
				*param.ValuePtr += wheel * param.Step;
				*param.ValuePtr = std::clamp(*param.ValuePtr, param.MinValue, param.MaxValue);
				changed = true;
			}
		}

		// Label + value on same line.
		ImGui::SameLine();
		ImGui::Text("%s", param.Label);

		// Revert button if value differs from default.
		if (differs)
		{
			ImGui::SameLine();
			char revertId[128];
			snprintf(revertId, sizeof(revertId), "R##%s_%s", idPrefix, param.Label);
			if (ImGui::SmallButton(revertId))
			{
				*param.ValuePtr = param.Default;
				changed = true;
			}
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Revert to preset default (%.4f)", param.Default);
		}

		if (differs)
			ImGui::PopStyleColor();

		return changed;
	}

	// ====================================================================
	// Draw a cloud category combo box.
	// ====================================================================

	static bool DrawCategoryCombo(const char* label, CloudCategory& category)
	{
		static const char* names[] = {
			"None", "CirrusHigh", "AltocumulusMid", "StratocumulusLow", "CumulonimbusVertical", "CumulonimbusVerticalBuildUp", "Aurora"
		};
		int current = static_cast<int>(category);
		bool changed = false;
		if (ImGui::Combo(label, &current, names, IM_ARRAYSIZE(names)))
		{
			category = static_cast<CloudCategory>(current);
			changed = true;
		}
		return changed;
	}

	// ====================================================================
	// Apply tuned cloud-pattern values for current shader behavior.
	// ====================================================================

	static void ApplyPatternPresetForCategory(VolumetricCloudLayerSnapshot& snap)
	{
		switch (snap.Category)
		{
		case CloudCategory::AltocumulusMid:
			// Schaefchenwolken: reference-shader-inspired soft cotton-ball puffs.
			// Uses only Alto-specific parameters for density/lighting.
			snap.AltoBillowStrength = 0.85f;
			snap.AltoCovSoftWidth   = 0.035f;
			snap.AltoAbsorption     = 1.0f;
			snap.AltoCloudSize      = 1.0f;
			snap.AltoCloudAmount    = 0.6875f;
			snap.AltoCloudBrightness = 1.0f;
			snap.AltoCloudColorR    = 1.0f;
			snap.AltoCloudColorG    = 1.0f;
			snap.AltoCloudColorB    = 1.0f;
			snap.AltoCloudColorDarkR = 0.55f;
			snap.AltoCloudColorDarkG = 0.55f;
			snap.AltoCloudColorDarkB = 0.65f;
			snap.AltoFbmLacunarity  = 2.6434f;
			snap.AltoFbmGain        = 0.5f;
			snap.AltoThickness      = 1800.0f;
			snap.AltoBottomSoftness = 0.35f;
			break;

		case CloudCategory::CirrusHigh:
			// Federwolken: thin, elongated wisps.
			snap.ShapeScale = 0.00009f;
			snap.DetailScale = 0.00055f;
			snap.DetailStrength = 0.14f;
			snap.EvolutionSpeed = 0.05f;
			snap.Coverage = 0.38f;
			snap.Absorption = 0.45f;
			break;

		default:
			break;
		}
	}

	// ====================================================================
	// Aurora Borealis layer controls — shown inside Cloud Layer A/B when
	// Category == CloudCategory::Aurora (replaces volumetric cloud sliders).
	// ====================================================================

	static void DrawAuroraControls()
	{
		using namespace TEN::Renderer;
		using namespace TEN::Renderer::Aurora;
		auto& aurora = g_Renderer.GetAuroraSettings();

		// Night-time visibility readout.
		{
			auto* levelPtr = dynamic_cast<Level*>(g_GameFlow->GetLevel(CurrentLevel));
			if (levelPtr && levelPtr->GetLensFlareEnabled())
			{
				constexpr float SHORT_TO_RAD = (3.14159265f * 2.0f / 65536.0f);
				float sunPitchRad = (float)levelPtr->GetLensFlarePitch() * SHORT_TO_RAD;
				float sunElev = std::sin(sunPitchRad);
				float auroraFade = std::clamp((-sunElev + aurora.NightFadeThreshold) * aurora.SunSuppressionStr, 0.0f, 1.0f);
				auroraFade = auroraFade * auroraFade * (3.0f - 2.0f * auroraFade);
				ImGui::Text("Sun Elev: %.3f  |  Aurora Visibility: %.3f", sunElev, auroraFade);
				if (auroraFade < 0.01f)
					ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "(Sun too high - aurora not visible)");
				else if (auroraFade < 0.5f)
					ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.5f, 1.0f), "(Twilight - aurora faint)");
				else
					ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "(Night - aurora visible)");
			}
			else
			{
				ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Lens flare not enabled.");
			}
		}

		ImGui::Separator();

		if (ImGui::CollapsingHeader("Core##aurora", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Indent(8.0f);
			ImGui::SliderFloat("Intensity##aurora",    &aurora.Intensity,  0.0f,  3.0f,  "%.3f");
			ImGui::SliderFloat("Brightness##aurora",   &aurora.Brightness, 0.0f,  5.0f,  "%.3f");
			ImGui::SliderFloat("Height##aurora",       &aurora.Height,     0.05f, 1.0f,  "%.3f");
			ImGui::TextDisabled("  Zenith coverage: 1.0 = full canopy, 0.1 = near-horizon band");
			ImGui::SliderFloat("Speed##aurora",        &aurora.Speed,      0.0f,  2.0f,  "%.3f");
			ImGui::Unindent(8.0f);
		}

		if (ImGui::CollapsingHeader("Color##aurora", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Indent(8.0f);
			static const char* auroraPresetNames[] = {
				"0: Green Classic", "1: Green + Purple", "2: Green + Red Tips",
				"3: Blue / Purple", "4: Strong Multicolor", "5: Turquoise / Blue / Purple"
			};
			ImGui::Combo("Color Preset##aurora", &aurora.ColorPreset, auroraPresetNames, IM_ARRAYSIZE(auroraPresetNames));
			ImGui::SliderFloat("Color Intensity##aurora", &aurora.ColorIntensity, 0.0f, 3.0f, "%.3f");
			ImGui::SliderFloat("Saturation##aurora",      &aurora.Saturation,     0.0f, 2.0f, "%.3f");
			ImGui::Unindent(8.0f);
		}

		if (ImGui::CollapsingHeader("Shape##aurora"))
		{
			ImGui::Indent(8.0f);
			ImGui::SliderFloat("Band Sharpness##aurora",   &aurora.BandSharpness,     0.5f, 8.0f,  "%.3f");
			ImGui::SliderFloat("Noise Scale##aurora",      &aurora.NoiseScale,         0.1f, 5.0f,  "%.3f");
			ImGui::SliderFloat("Vertical Stretch##aurora", &aurora.VerticalStretch,    0.5f, 10.0f, "%.3f");
			ImGui::SliderFloat("Spread##aurora",           &aurora.Spread,             0.1f, 1.0f,  "%.3f");
			ImGui::SliderFloat("Distortion##aurora",       &aurora.DistortionStrength, 0.0f, 1.0f,  "%.3f");
			ImGui::Unindent(8.0f);
		}

		if (ImGui::CollapsingHeader("Night Visibility##aurora"))
		{
			ImGui::Indent(8.0f);
			ImGui::SliderFloat("Night Fade Threshold##aurora", &aurora.NightFadeThreshold, 0.0f,  0.5f,  "%.3f");
			ImGui::SliderFloat("Sun Suppression##aurora",      &aurora.SunSuppressionStr,  1.0f,  20.0f, "%.2f");
			ImGui::Unindent(8.0f);
		}

		if (ImGui::CollapsingHeader("Advanced##aurora"))
		{
			ImGui::Indent(8.0f);
			ImGui::SliderInt("Layer Count##aurora",  &aurora.LayerCount, 1, 5);
			ImGui::SliderFloat("Softness##aurora",   &aurora.Softness,   0.0f, 1.0f, "%.3f");
			ImGui::Unindent(8.0f);
		}

		if (ImGui::CollapsingHeader("Preset Fade##aurora", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Indent(8.0f);
			ImGui::Text("Fade Progress: %.3f", g_Renderer.GetAuroraPresetFade());
			ImGui::SliderFloat("Fade Duration (s)##aurora", &g_Renderer.GetAuroraPresetFadeDuration(),
			                   0.5f, 120.0f, "%.1f s");
			ImGui::TextDisabled("  Duration for aurora to fade in/out when preset changes.");
			ImGui::Unindent(8.0f);
		}

		ImGui::Separator();
		if (ImGui::Button("Reset Aurora Defaults##layer"))
		{
			bool wasEnabled = aurora.Enabled;
			aurora = AuroraSettings{};
			aurora.Enabled = wasEnabled;
		}
	}

	// ====================================================================
	// Draw a cloud layer section with all parameter sliders.
	// ====================================================================

	static bool DrawLayerSection(
		const char* title,
		const char* idPrefix,
		VolumetricCloudLayerSnapshot& snap,
		const VolumetricCloudLayerSnapshot* defaults)
	{
		if (!ImGui::CollapsingHeader(title, ImGuiTreeNodeFlags_DefaultOpen))
			return false;

		bool changed = false;

		ImGui::Indent(8.0f);

		// Enabled toggle.
		if (ImGui::Checkbox("Enabled", &snap.Enabled))
			changed = true;

		// Category combo.
		if (DrawCategoryCombo("Category", snap.Category))
			changed = true;

		// For Aurora category, show aurora controls instead of volumetric cloud controls.
		if (snap.Category == CloudCategory::Aurora)
		{
			using namespace TEN::Renderer::Aurora;
			g_Renderer.GetAuroraSettings().Enabled = snap.Enabled;
			ImGui::Separator();
			DrawAuroraControls();
			ImGui::Unindent(8.0f);
			return changed;
		}

		// One-click pattern presets for requested cloud types.
		if (snap.Category == CloudCategory::AltocumulusMid ||
			snap.Category == CloudCategory::CirrusHigh ||
			snap.Category == CloudCategory::CumulonimbusVertical ||
			snap.Category == CloudCategory::CumulonimbusVerticalBuildUp)
		{
			if (ImGui::Button("Apply Tuned Pattern"))
			{
				ApplyPatternPresetForCategory(snap);
				changed = true;
			}
			ImGui::SameLine();
			if (snap.Category == CloudCategory::AltocumulusMid)
				ImGui::TextDisabled("Altocumulus grouping (Schaefchen)");
			else if (snap.Category == CloudCategory::CirrusHigh)
				ImGui::TextDisabled("Elongated Cirrus wisps (Federwolken)");
			else
				ImGui::TextDisabled("Storm tower defaults (Cumulonimbus)");
		}

		ImGui::Separator();

		// Build and draw all parameter sliders.
		auto params = BuildParamList(snap, defaults);
		for (auto& p : params)
			changed |= DrawParamSlider(p, idPrefix);

		// Altocumulus color pickers (visual RGB widgets for bright top + dark base).
		if (snap.Category == CloudCategory::AltocumulusMid)
		{
			ImGui::Separator();
			ImGui::TextDisabled("Bright (Top) Color");
			float altoColor[3] = { snap.AltoCloudColorR, snap.AltoCloudColorG, snap.AltoCloudColorB };
			char colorId[128];
			snprintf(colorId, sizeof(colorId), "Alto Color##%s", idPrefix);
			if (ImGui::ColorEdit3(colorId, altoColor))
			{
				snap.AltoCloudColorR = altoColor[0];
				snap.AltoCloudColorG = altoColor[1];
				snap.AltoCloudColorB = altoColor[2];
				changed = true;
			}
			ImGui::TextDisabled("Dark (Base / Shadow) Color");
			float darkColor[3] = { snap.AltoCloudColorDarkR, snap.AltoCloudColorDarkG, snap.AltoCloudColorDarkB };
			char darkColorId[128];
			snprintf(darkColorId, sizeof(darkColorId), "Alto Dark Color##%s", idPrefix);
			if (ImGui::ColorEdit3(darkColorId, darkColor))
			{
				snap.AltoCloudColorDarkR = darkColor[0];
				snap.AltoCloudColorDarkG = darkColor[1];
				snap.AltoCloudColorDarkB = darkColor[2];
				changed = true;
			}
			ImGui::TextDisabled("Lightning Bolt Color");
			float boltColor[3] = { snap.LightningBoltColorR, snap.LightningBoltColorG, snap.LightningBoltColorB };
			char boltColorId[128];
			snprintf(boltColorId, sizeof(boltColorId), "Bolt Color##%s", idPrefix);
			if (ImGui::ColorEdit3(boltColorId, boltColor))
			{
				snap.LightningBoltColorR = boltColor[0];
				snap.LightningBoltColorG = boltColor[1];
				snap.LightningBoltColorB = boltColor[2];
				changed = true;
			}
			if (ImGui::Checkbox("Lightning Enabled", &snap.LightningEnabled))
				changed = true;
		}

		// Reset all button.
		if (defaults)
		{
			ImGui::Separator();
			char resetId[64];
			snprintf(resetId, sizeof(resetId), "Reset All##%s", idPrefix);
			if (ImGui::Button(resetId))
			{
				snap = *defaults;
				changed = true;
			}
			ImGui::SameLine();
			ImGui::TextDisabled("(revert to preset defaults)");
		}

		ImGui::Unindent(8.0f);
		return changed;
	}

	// ====================================================================
	// Main overlay
	// ====================================================================

	static void DrawCloudTabContent()
	{
		auto info = g_SkyCloudSystem.GetDebugInfo();
		auto& state = g_SkyCloudSystem.GetMutableCurrentState();

		// ----------------------------------------------------------------
		// Weather State section
		// ----------------------------------------------------------------
		if (ImGui::CollapsingHeader("Weather State", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Indent(8.0f);

			ImGui::Text("Current Preset: %s", SkyCloudSystem::PresetTypeToString(info.CurrentPreset));
			ImGui::Text("Target Preset:  %s", SkyCloudSystem::PresetTypeToString(info.TargetPreset));

			if (g_SkyCloudSystem.IsTransitioning())
			{
				ImGui::ProgressBar(info.TransitionProgress, ImVec2(-1, 0),
					("Transition " + std::to_string(static_cast<int>(info.TransitionProgress * 100)) + "%").c_str());
			}
			else
			{
				ImGui::TextDisabled("No active transition");
			}

			if (!info.NextPreset.empty())
				ImGui::Text("Auto-chain -> %s", info.NextPreset.c_str());

			ImGui::Separator();

			ImGui::Text("Random Weather: %s", info.RandomModeActive ? "ACTIVE" : "OFF");
			if (info.RandomModeActive)
				ImGui::Text("  Dwell Remaining: %.1f s", info.RandomDwellRemaining);

			ImGui::Separator();

			// Layer status summary.
			ImGui::Text("Legacy Layer 1: %s", info.Layer1Enabled ? "ON" : "OFF");
			ImGui::Text("Legacy Layer 2: %s", info.Layer2Enabled ? "ON" : "OFF");
			ImGui::Text("Cloud A: %s  [%s]", info.CloudAEnabled ? "ON" : "OFF",
				CloudCategoryToString(info.CloudACategory));
			ImGui::Text("Cloud B: %s  [%s]", info.CloudBEnabled ? "ON" : "OFF",
				CloudCategoryToString(info.CloudBCategory));

			ImGui::Separator();
			ImGui::Text("Transmittance A: %.3f  B: %.3f  Combined: %.3f",
				info.CloudATransmittance, info.CloudBTransmittance, info.CombinedTransmittance);

			ImGui::Unindent(8.0f);
		}

		// ----------------------------------------------------------------
		// Preset Switcher section
		// ----------------------------------------------------------------
		if (ImGui::CollapsingHeader("Preset Switcher", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Indent(8.0f);

			auto presetTypes = g_SkyCloudSystem.GetAllPresetTypes();

			// Build display names list.
			static int selectedPresetIdx = 0;
			std::vector<const char*> presetNames;
			presetNames.reserve(presetTypes.size());
			for (auto t : presetTypes)
				presetNames.push_back(SkyCloudSystem::PresetTypeToString(t));

			if (!presetNames.empty())
			{
				if (selectedPresetIdx >= static_cast<int>(presetNames.size()))
					selectedPresetIdx = 0;

				ImGui::Combo("Target Preset", &selectedPresetIdx,
					presetNames.data(), static_cast<int>(presetNames.size()));

				WeatherPresetType selectedType = presetTypes[selectedPresetIdx];

				if (ImGui::Button("Apply Immediately"))
					g_SkyCloudSystem.SetPresetImmediate(selectedType);

				ImGui::SameLine();

				static float transDurA = 30.0f;
				static float transDurB = 30.0f;
				static int   lastPresetForDur = -1;

				// Auto-fill from preset definition when selection changes.
				if (selectedPresetIdx != lastPresetForDur)
				{
					lastPresetForDur = selectedPresetIdx;
					const auto* selDef = g_SkyCloudSystem.GetPresetDefinition(selectedType);
					if (selDef)
					{
						transDurA = (selDef->TransitionDurationA >= 0.0f) ? selDef->TransitionDurationA : selDef->DefaultTransitionDuration;
						transDurB = (selDef->TransitionDurationB >= 0.0f) ? selDef->TransitionDurationB : selDef->DefaultTransitionDuration;
					}
				}

				ImGui::Text("Transition:");
				ImGui::SameLine();
				ImGui::SetNextItemWidth(80.0f);
				ImGui::DragFloat("A##transDurA", &transDurA, 1.0f, 1.0f, 300.0f, "%.0f s");
				ImGui::SameLine();
				ImGui::SetNextItemWidth(80.0f);
				ImGui::DragFloat("B##transDurB", &transDurB, 1.0f, 1.0f, 300.0f, "%.0f s");
				ImGui::SameLine();
				if (ImGui::Button("Transition"))
					g_SkyCloudSystem.TransitionToPreset(selectedType, transDurA, transDurB);

				if (g_SkyCloudSystem.IsTransitioning())
				{
					ImGui::SameLine();
					if (ImGui::Button("Interrupt"))
						g_SkyCloudSystem.InterruptTransition();
				}
			}

			ImGui::Separator();

			// Random weather controls.
			static float rwDwell = 120.0f;
			static float rwTransition = 60.0f;
			ImGui::Text("Random Weather");
			ImGui::SetNextItemWidth(100.0f);
			ImGui::DragFloat("Dwell (s)##rw", &rwDwell, 1.0f, 5.0f, 600.0f, "%.0f");
			ImGui::SameLine();
			ImGui::SetNextItemWidth(100.0f);
			ImGui::DragFloat("Trans (s)##rw", &rwTransition, 1.0f, 5.0f, 300.0f, "%.0f");
			ImGui::SameLine();
			if (!g_SkyCloudSystem.IsRandomWeatherActive())
			{
				if (ImGui::Button("Start Random"))
					g_SkyCloudSystem.StartRandomWeather(rwDwell, rwTransition);
			}
			else
			{
				if (ImGui::Button("Stop Random"))
					g_SkyCloudSystem.StopRandomWeather();
			}

			ImGui::Unindent(8.0f);
		}

		// ----------------------------------------------------------------
		// Live Cloud Layer A Editor
		// ----------------------------------------------------------------
		{
			// Get preset defaults for diff highlighting.
			const auto* presetDef = g_SkyCloudSystem.GetPresetDefinition(info.CurrentPreset);
			const VolumetricCloudLayerSnapshot* defaultsA =
				presetDef ? &presetDef->TargetState.CloudA : nullptr;
			const VolumetricCloudLayerSnapshot* defaultsB =
				presetDef ? &presetDef->TargetState.CloudB : nullptr;

			if (DrawLayerSection("Cloud Layer A (Live)", "liveA", state.CloudA, defaultsA))
				g_SkyCloudSystem.SetVolumetricLayerA(state.CloudA);
			if (DrawLayerSection("Cloud Layer B (Live)", "liveB", state.CloudB, defaultsB))
				g_SkyCloudSystem.SetVolumetricLayerB(state.CloudB);
		}

		// ----------------------------------------------------------------
		// Preset Definition Editor (edits the stored preset, not just live)
		// ----------------------------------------------------------------
		if (ImGui::CollapsingHeader("Preset Definition Editor"))
		{
			ImGui::Indent(8.0f);

			auto presetTypes = g_SkyCloudSystem.GetAllPresetTypes();
			static int editPresetIdx = 0;

			std::vector<const char*> presetNames;
			for (auto t : presetTypes)
				presetNames.push_back(SkyCloudSystem::PresetTypeToString(t));

			if (!presetNames.empty())
			{
				if (editPresetIdx >= static_cast<int>(presetNames.size()))
					editPresetIdx = 0;

				ImGui::Combo("Edit Preset##def", &editPresetIdx,
					presetNames.data(), static_cast<int>(presetNames.size()));

				WeatherPresetType editType = presetTypes[editPresetIdx];
				auto* def = g_SkyCloudSystem.GetMutablePresetDefinition(editType);

				if (def)
				{
					ImGui::Text("Name: %s", def->Name.c_str());
					ImGui::DragFloat("Default Transition",  &def->DefaultTransitionDuration, 1.0f, 1.0f, 300.0f, "%.0f s");
					// Per-layer durations: -1 means "inherit DefaultTransition at runtime".
					ImGui::DragFloat("Transition A (Layer A)", &def->TransitionDurationA, 1.0f, -1.0f, 300.0f,
						def->TransitionDurationA < 0.0f ? "(inherit default)" : "%.0f s");
					ImGui::DragFloat("Transition B (Layer B)", &def->TransitionDurationB, 1.0f, -1.0f, 300.0f,
						def->TransitionDurationB < 0.0f ? "(inherit default)" : "%.0f s");
					ImGui::DragFloat("Random Weight", &def->RandomWeight, 0.1f, 0.0f, 10.0f, "%.1f");
					ImGui::Checkbox("Allow in Random", &def->AllowInRandom);
					ImGui::DragFloat("High Layer Lead (legacy)", &def->HighLayerLeadFraction, 0.01f, 0.0f, 1.0f, "%.2f");

					ImGui::Separator();
					ImGui::Text("Auto-Chain (NextPreset)");
					// Editable next-preset name.
					static char nextPresetBuf[64] = {};
					if (ImGui::IsWindowAppearing())
						strncpy_s(nextPresetBuf, def->NextPreset.c_str(), sizeof(nextPresetBuf) - 1);
					if (ImGui::InputText("Next Preset##chain", nextPresetBuf, sizeof(nextPresetBuf)))
						def->NextPreset = nextPresetBuf;
					ImGui::DragFloat("Next Trans Default##chain",  &def->NextPresetTransitionDuration,  1.0f, 0.1f, 300.0f, "%.0f s");
					ImGui::DragFloat("Next Trans A##chain", &def->NextPresetTransitionDurationA, 1.0f, -1.0f, 300.0f,
						def->NextPresetTransitionDurationA < 0.0f ? "(inherit default)" : "%.0f s");
					ImGui::DragFloat("Next Trans B##chain", &def->NextPresetTransitionDurationB, 1.0f, -1.0f, 300.0f,
						def->NextPresetTransitionDurationB < 0.0f ? "(inherit default)" : "%.0f s");
					if (ImGui::Button("Clear Auto-Chain"))
					{
						def->NextPreset = "";
						nextPresetBuf[0] = '\0';
					}

					ImGui::Separator();
					DrawLayerSection("Preset Cloud A", "defA", def->TargetState.CloudA, nullptr);
					DrawLayerSection("Preset Cloud B", "defB", def->TargetState.CloudB, nullptr);

					ImGui::Separator();
					if (ImGui::Button("Copy Live State -> This Preset"))
					{
						def->TargetState = g_SkyCloudSystem.GetCurrentState();
					}
					ImGui::SameLine();
					ImGui::TextDisabled("(saves current runtime values into preset definition)");
				}
			}

			ImGui::Unindent(8.0f);
		}

	}

	// ====================================================================
	// Moon Position Widget — circular control like the sun widget
	// ====================================================================

	static constexpr float MOON_WIDGET_SIZE   = 220.0f;
	static constexpr float MOON_POINT_RADIUS  = 7.0f;
	static constexpr float MOON_HIT_RADIUS    = 12.0f;
	static constexpr float MOON_PITCH_MIN     = -10.0f;
	static constexpr float MOON_PITCH_MAX     = 90.0f;
	static constexpr float MOON_MAX_RADIUS    = 1.0f + (-MOON_PITCH_MIN / MOON_PITCH_MAX);

	static ImVec2 MoonPitchYawToCirclePoint(float pitch, float yaw)
	{
		float r = std::clamp(1.0f - pitch / MOON_PITCH_MAX, 0.0f, MOON_MAX_RADIUS);
		float yawRad = yaw * (3.14159265f / 180.0f);
		return ImVec2(r * std::sin(yawRad), -r * std::cos(yawRad));
	}

	static void MoonCirclePointToPitchYaw(const ImVec2& point, float& outPitch, float& outYaw)
	{
		float r = std::sqrt(point.x * point.x + point.y * point.y);
		r = std::clamp(r, 0.0f, MOON_MAX_RADIUS);
		outPitch = std::clamp(MOON_PITCH_MAX * (1.0f - r), MOON_PITCH_MIN, MOON_PITCH_MAX);
		float yawRad = std::atan2(point.x, -point.y);
		outYaw = std::fmod(yawRad * (180.0f / 3.14159265f) + 360.0f, 360.0f);
	}

	static ImVec2 ClampToMoonCircle(const ImVec2& p)
	{
		float len = std::sqrt(p.x * p.x + p.y * p.y);
		if (len <= MOON_MAX_RADIUS) return p;
		return ImVec2(p.x / len * MOON_MAX_RADIUS, p.y / len * MOON_MAX_RADIUS);
	}

	static bool DrawMoonPositionWidget(float& pitch, float& yaw)
	{
		bool changed = false;
		float halfSize = MOON_WIDGET_SIZE * 0.5f;
		float radius = (halfSize - 2.0f) / MOON_MAX_RADIUS;

		ImVec2 canvasPos = ImGui::GetCursorScreenPos();
		ImVec2 center = ImVec2(canvasPos.x + halfSize, canvasPos.y + halfSize);

		ImGui::InvisibleButton("##moonWidget", ImVec2(MOON_WIDGET_SIZE, MOON_WIDGET_SIZE));
		ImDrawList* drawList = ImGui::GetWindowDrawList();

		// Background.
		drawList->AddRectFilled(canvasPos,
			ImVec2(canvasPos.x + MOON_WIDGET_SIZE, canvasPos.y + MOON_WIDGET_SIZE),
			IM_COL32(15, 15, 30, 255));
		drawList->AddRect(canvasPos,
			ImVec2(canvasPos.x + MOON_WIDGET_SIZE, canvasPos.y + MOON_WIDGET_SIZE),
			IM_COL32(60, 60, 100, 255));

		// Horizon circle.
		drawList->AddCircle(center, radius, IM_COL32(50, 60, 100, 255), 64, 1.5f);
		// Below-horizon ring.
		drawList->AddCircle(center, radius * MOON_MAX_RADIUS, IM_COL32(80, 40, 40, 100), 64, 1.0f);
		// Crosshairs.
		drawList->AddLine(ImVec2(center.x - radius, center.y), ImVec2(center.x + radius, center.y), IM_COL32(40, 40, 60, 128));
		drawList->AddLine(ImVec2(center.x, center.y - radius), ImVec2(center.x, center.y + radius), IM_COL32(40, 40, 60, 128));
		// Labels.
		drawList->AddCircle(center, 3.0f, IM_COL32(80, 100, 140, 180), 16, 1.0f);
		drawList->AddText(ImVec2(center.x + 6, center.y - 8), IM_COL32(100, 100, 120, 200), "Zenith");

		// Convert current pitch/yaw to screen position.
		ImVec2 normalizedPoint = MoonPitchYawToCirclePoint(pitch, yaw);
		ImVec2 screenPoint = ImVec2(
			center.x + normalizedPoint.x * radius,
			center.y + normalizedPoint.y * radius);

		// Interaction.
		static bool moonDragging = false;
		ImVec2 mousePos = ImGui::GetIO().MousePos;
		bool mouseDown = ImGui::GetIO().MouseDown[0];

		float distToPoint = std::sqrt(
			(mousePos.x - screenPoint.x) * (mousePos.x - screenPoint.x) +
			(mousePos.y - screenPoint.y) * (mousePos.y - screenPoint.y));
		float distToCenter = std::sqrt(
			(mousePos.x - center.x) * (mousePos.x - center.x) +
			(mousePos.y - center.y) * (mousePos.y - center.y));

		if (ImGui::IsItemActive() && mouseDown)
		{
			if (!moonDragging && (distToPoint < MOON_HIT_RADIUS || distToCenter <= radius * MOON_MAX_RADIUS))
				moonDragging = true;
		}
		else
		{
			moonDragging = false;
		}

		if (moonDragging)
		{
			ImVec2 newNormalized = ImVec2(
				(mousePos.x - center.x) / radius,
				(mousePos.y - center.y) / radius);
			newNormalized = ClampToMoonCircle(newNormalized);
			MoonCirclePointToPitchYaw(newNormalized, pitch, yaw);
			changed = true;
			screenPoint = ImVec2(
				center.x + newNormalized.x * radius,
				center.y + newNormalized.y * radius);
		}

		// Draw moon point (bluish-white glow).
		drawList->AddCircleFilled(screenPoint, MOON_POINT_RADIUS + 3.0f, IM_COL32(180, 200, 255, 60));
		drawList->AddCircleFilled(screenPoint, MOON_POINT_RADIUS, IM_COL32(200, 210, 240, 255));
		drawList->AddCircle(screenPoint, MOON_POINT_RADIUS, IM_COL32(220, 230, 255, 255), 0, 1.5f);
		if (moonDragging)
			drawList->AddCircle(screenPoint, MOON_POINT_RADIUS + 5.0f, IM_COL32(180, 200, 255, 180), 0, 2.0f);

		return changed;
	}

	// ====================================================================
	// Moon debug content
	// ====================================================================

	static void DrawMoonTabContent()
	{
		using namespace TEN::Renderer;
		using namespace TEN::Renderer::Moon;
		auto& moon = g_Renderer.GetMoonSettings();

		ImGui::Separator();
		ImGui::Spacing();

		// --- Moon Enable ---
		if (ImGui::CollapsingHeader("Moon System", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Indent(8.0f);

			ImGui::Checkbox("Moon Enabled", &moon.Enabled);

			if (!moon.Enabled)
			{
				ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Moon system is disabled.");
				ImGui::Unindent(8.0f);
				return;
			}

			// --- Moon State Info ---
			ImGui::Text("Pitch: %.1f deg", moon.Pitch);
			ImGui::Text("Yaw:   %.1f deg", moon.Yaw);

			// Compute phase info for readout.
			auto* levelPtr = dynamic_cast<Level*>(g_GameFlow->GetLevel(CurrentLevel));
			if (levelPtr && levelPtr->GetLensFlareEnabled())
			{
				float sunPitchRad = (float)levelPtr->GetLensFlarePitch() * (3.14159265f / 180.0f);
				float sunYawRad   = (float)levelPtr->GetLensFlareYaw() * (3.14159265f / 180.0f);
				// Build sun direction for phase computation.
				float sunElev = std::sin(sunPitchRad);
				float moonVis = g_Renderer.ComputeMoonVisibility(sunElev);

				DirectX::SimpleMath::Vector3 sunDir(
					std::cos(sunPitchRad) * std::sin(sunYawRad),
					-std::sin(sunPitchRad),
					std::cos(sunPitchRad) * std::cos(sunYawRad));
				sunDir.Normalize();

				float moonPitchRad = moon.Pitch * (3.14159265f / 180.0f);
				float moonYawRad   = moon.Yaw * (3.14159265f / 180.0f);
				DirectX::SimpleMath::Vector3 moonDir(
					std::cos(moonPitchRad) * std::sin(moonYawRad),
					-std::sin(moonPitchRad),
					std::cos(moonPitchRad) * std::cos(moonYawRad));
				moonDir.Normalize();

				float phase = g_Renderer.ComputeMoonPhase(sunDir, moonDir);
				float brightness = phase * phase * (3.0f - 2.0f * phase);

				// Describe phase.
				const char* phaseName = "Quarter";
				if (phase > 0.9f)       phaseName = "Full Moon";
				else if (phase > 0.7f)  phaseName = "Waxing Gibbous";
				else if (phase > 0.45f) phaseName = "Half Moon";
				else if (phase > 0.2f)  phaseName = "Crescent";
				else                    phaseName = "New Moon";

				ImGui::Text("Phase: %.2f (%s)", phase, phaseName);
				ImGui::Text("Phase Brightness: %.2f", brightness);
				ImGui::Text("Moon Visibility: %.2f", moonVis);
			}

			ImGui::Unindent(8.0f);
		}

		// --- Moon Position Control ---
		if (ImGui::CollapsingHeader("Moon Position Control", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Indent(8.0f);

			ImGui::Text("Drag the moon point inside the circle:");
			if (DrawMoonPositionWidget(moon.Pitch, moon.Yaw))
			{
				// Position updated via widget.
			}

			ImGui::Spacing();
			ImGui::SliderFloat("Moon Pitch##manual", &moon.Pitch, MOON_PITCH_MIN, MOON_PITCH_MAX, "%.1f deg");
			ImGui::SliderFloat("Moon Yaw##manual", &moon.Yaw, 0.0f, 360.0f, "%.1f deg");

			ImGui::Unindent(8.0f);
		}

		// --- Moon Appearance ---
		if (ImGui::CollapsingHeader("Moon Appearance", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Indent(8.0f);

			ImGui::SliderFloat("Disk Size",      &moon.DiskSize,      0.1f, 10.0f, "%.2f deg");
			ImGui::SliderFloat("Disk Intensity", &moon.DiskIntensity, 1.0f, 100.0f, "%.1f");
			ImGui::SliderFloat("Base Color R",   &moon.BaseColorR,    0.0f, 1.0f, "%.3f");
			ImGui::SliderFloat("Base Color G",   &moon.BaseColorG,    0.0f, 1.0f, "%.3f");
			ImGui::SliderFloat("Base Color B",   &moon.BaseColorB,    0.0f, 1.0f, "%.3f");

			ImGui::Separator();
			ImGui::TextDisabled("Moon Glow (Sky Halo)");
			ImGui::SliderFloat("Glow Intensity", &moon.GlowIntensity, 0.0f, 2.0f, "%.3f");
			ImGui::SliderFloat("Glow Falloff",   &moon.GlowFalloff,   1.0f, 100.0f, "%.2f");

			ImGui::Unindent(8.0f);
		}

		// --- Night Cloud Lighting ---
		if (ImGui::CollapsingHeader("Night Cloud Lighting", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Indent(8.0f);

			ImGui::SliderFloat("Moon Cloud Light Intensity", &moon.CloudLightIntensity, 0.0f, 2.0f, "%.3f");
			ImGui::TextDisabled("  Direct moonlight intensity on clouds at night.");
			ImGui::SliderFloat("Moon Cloud Ambient Boost",   &moon.CloudAmbientBoost,   0.0f, 0.5f, "%.3f");
			ImGui::TextDisabled("  Additional ambient for moonlit clouds.");

			ImGui::Unindent(8.0f);
		}

		// --- Moon God Rays ---
		if (ImGui::CollapsingHeader("Moon God Rays", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Indent(8.0f);

			ImGui::Checkbox("Moon God Rays Enabled", &moon.GodRays.Enabled);

			if (moon.GodRays.Enabled)
			{
				ImGui::SliderFloat("Length##moonray",      &moon.GodRays.Length,      0.05f, 1.5f, "%.3f");
				ImGui::SliderFloat("Intensity##moonray",   &moon.GodRays.Intensity,   0.0f,  1.0f, "%.3f");
				ImGui::SliderFloat("Decay##moonray",       &moon.GodRays.Decay,       0.90f, 1.0f, "%.4f");
				ImGui::SliderFloat("Softness##moonray",    &moon.GodRays.Softness,    0.1f,  3.0f, "%.3f");
				ImGui::SliderInt("Samples##moonray",       &moon.GodRays.SampleCount, 16,    128);
				ImGui::SliderFloat("Auto Strength##moonray", &moon.GodRays.AutoStrength, 0.0f, 1.0f, "%.3f");
			}
			else
			{
				ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Moon god rays are disabled.");
			}

			ImGui::Unindent(8.0f);
		}

		// --- Reset Moon ---
		ImGui::Separator();
		if (ImGui::Button("Reset Moon Defaults"))
		{
			bool wasEnabled = moon.Enabled;
			moon = MoonSettings{};
			moon.Enabled = wasEnabled;
		}
	}

	// ====================================================================
	// Starfield debug section
	// ====================================================================

	static void DrawStarfieldSection(Level* level)
	{
		if (!level)
			return;

		if (!ImGui::CollapsingHeader("Starfield", ImGuiTreeNodeFlags_DefaultOpen))
			return;

		ImGui::Indent(8.0f);

		// Persistent saved star count — used to restore when toggling back on.
		static int savedStarCount = 1000;

		int starCount = level->Starfield.GetStarCount();
		bool enabled = (starCount > 0);

		// Enable/disable toggle.
		if (ImGui::Checkbox("Starfield Enabled", &enabled))
		{
			if (enabled)
			{
				level->Starfield.SetStarCount(std::max(savedStarCount, 1));
			}
			else
			{
				savedStarCount = std::max(starCount, 1);
				level->Starfield.SetStarCount(0);
			}
			starCount = level->Starfield.GetStarCount();
		}

		ImGui::Spacing();

		// Sliders always visible — edits are applied live when enabled,
		// or staged in savedStarCount when disabled so settings persist.
		int editStarCount = enabled ? starCount : savedStarCount;
		if (ImGui::SliderInt("Star Count", &editStarCount, 1, 6000))
		{
			savedStarCount = editStarCount;
			if (enabled)
				level->Starfield.SetStarCount(editStarCount);
		}

		int meteorCount = level->Starfield.GetMeteorCount();
		if (ImGui::SliderInt("Meteor Count", &meteorCount, 0, 100))
			level->Starfield.SetMeteorCount(meteorCount);

		int meteorDensity = level->Starfield.GetMeteorSpawnDensity();
		if (ImGui::SliderInt("Meteor Spawn Density", &meteorDensity, 0, 100))
			level->Starfield.SetMeteorSpawnDensity(meteorDensity);

		float meteorVel = level->Starfield.GetMeteorVelocity();
		if (ImGui::SliderFloat("Meteor Velocity", &meteorVel, 0.0f, 100.0f, "%.1f"))
			level->Starfield.SetMeteorVelocity(meteorVel);

		// Starfield visibility info (from atmospheric sky).
		ImGui::Separator();
		ImGui::TextDisabled("Visibility (from Atmospheric Sky)");

		auto& atmoSettings = g_Renderer.GetAtmosphericSkySettings();
		if (atmoSettings.Enabled && level->GetLensFlareEnabled())
		{
			constexpr float SHORT_TO_RAD = (3.14159265f * 2.0f / 65536.0f);
			float sunPitchRad = (float)level->GetLensFlarePitch() * SHORT_TO_RAD;
			float sunElev = std::sin(sunPitchRad);
			float starVis = g_Renderer.ComputeStarfieldVisibility(sunElev);
			float dayNight = g_Renderer.ComputeDayNightBlend(sunElev);

			ImGui::Text("  Star Visibility:   %.3f", starVis);
			ImGui::Text("  Day/Night Blend:   %.3f", dayNight);
			ImGui::Text("  Night Sky Brightness: %.3f", atmoSettings.NightSkyBrightness);
		}
		else
		{
			ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "  Atmospheric sky disabled or no lens flare.");
		}

		ImGui::Unindent(8.0f);
	}

	// ====================================================================
	// Horizon mesh section (Sun/Moon/Horizon/Stars tab)
	// ====================================================================

	static void DrawHorizonSection(Level* level)
	{
		if (!level)
			return;

		if (!ImGui::CollapsingHeader("Horizont-Mesh", ImGuiTreeNodeFlags_DefaultOpen))
			return;

		ImGui::Indent(8.0f);

		ImGui::TextDisabled("Horizont 1");
		{
			bool enabled = level->Horizon1.GetEnabled();
			if (ImGui::Checkbox("Aktiv##h1", &enabled))
				level->Horizon1.SetEnabled(enabled);

			float alpha = level->Horizon1.GetTransparency();
			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 80.0f);
			if (ImGui::SliderFloat("Alpha##h1", &alpha, 0.0f, 1.0f, "%.2f"))
				level->Horizon1.SetTransparency(alpha);
		}

		ImGui::Separator();

		ImGui::TextDisabled("Horizont 2");
		{
			bool enabled = level->Horizon2.GetEnabled();
			if (ImGui::Checkbox("Aktiv##h2", &enabled))
				level->Horizon2.SetEnabled(enabled);

			float alpha = level->Horizon2.GetTransparency();
			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 80.0f);
			if (ImGui::SliderFloat("Alpha##h2", &alpha, 0.0f, 1.0f, "%.2f"))
				level->Horizon2.SetTransparency(alpha);
		}

		ImGui::Unindent(8.0f);
	}

	// ====================================================================
	// Unified Sky Debug Window
	// ====================================================================
	// Atmospheric Sky Dome debug tab
	// ====================================================================

	static void DrawAtmosphericSkyTabContent()
	{
		using namespace TEN::Renderer;
		auto& settings = g_Renderer.GetAtmosphericSkySettings();

		ImGui::Checkbox("Enabled", &settings.Enabled);
		ImGui::Separator();

		if (!settings.Enabled)
		{
			ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Atmospheric sky dome is disabled.");
			return;
		}

		// --- Sun info (read-only) ---
		ImGui::Text("Sun Info (from Lens Flare):");
		auto* levelPtr = dynamic_cast<Level*>(g_GameFlow->GetLevel(CurrentLevel));
		if (levelPtr && levelPtr->GetLensFlareEnabled())
		{
			float pitch = (float)levelPtr->GetLensFlarePitch();
			float yaw   = (float)levelPtr->GetLensFlareYaw();
			float elev  = std::sin(DirectX::XMConvertToRadians(pitch));
			ImGui::Text("  Pitch: %.1f  Yaw: %.1f  Elevation: %.3f", pitch, yaw, elev);

			auto col = levelPtr->GetLensFlareEvaluatedColor();
			ImGui::Text("  Sun Color: (%.2f, %.2f, %.2f)", col.x, col.y, col.z);

			// Compute and display day/night state.
			float dayNight = g_Renderer.ComputeDayNightBlend(elev);
			float starVis  = g_Renderer.ComputeStarfieldVisibility(elev);
			ImGui::Text("  Day/Night Blend: %.3f  Starfield: %.3f", dayNight, starVis);

			// Compute and display sun horizon fade (matching sprite fade in renderer).
			float hFade = std::clamp(elev * 4.0f + 1.0f, 0.0f, 1.0f);
			hFade = std::pow(hFade, settings.HorizonDarkeningStr);
			ImGui::Text("  Sun Horizon Fade: %.3f", hFade);
		}
		else
		{
			ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "  Lens flare not enabled.");
		}

		ImGui::Separator();

		// --- Scattering parameters ---
		if (ImGui::CollapsingHeader("Scattering", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::SliderFloat("Sky Color R",           &settings.SkyColorR,            0.0f, 1.0f, "%.3f");
			ImGui::SliderFloat("Sky Color G",           &settings.SkyColorG,            0.0f, 1.0f, "%.3f");
			ImGui::SliderFloat("Sky Color B",           &settings.SkyColorB,            0.0f, 1.0f, "%.3f");
			ImGui::SliderFloat("Density",               &settings.Density,              0.1f, 3.0f, "%.3f");
			ImGui::SliderFloat("Zenith Offset",         &settings.ZenithOffset,         0.0f, 0.5f, "%.3f");
			ImGui::SliderFloat("Multi Scatter Phase",   &settings.MultiScatterPhase,    0.0f, 1.0f, "%.3f");
			ImGui::SliderFloat("Anisotropic Intensity", &settings.AnisotropicIntensity, 0.0f, 2.0f, "%.3f");
		}

		// --- Sun disk ---
		if (ImGui::CollapsingHeader("Sun Disk", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::SliderFloat("Sun Disk Size",      &settings.SunDiskSize,      0.1f, 10.0f, "%.2f deg");
			ImGui::TextDisabled("  Apparent half-angle of the sun disk in degrees.");
			ImGui::SliderFloat("Sun Disk Intensity", &settings.SunDiskIntensity, 1.0f, 200.0f, "%.1f");
			ImGui::TextDisabled("  Brightness before tone mapping. High = solid white disk.");
		}

		// --- Glow and brightness ---
		if (ImGui::CollapsingHeader("Glow & Brightness", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::SliderFloat("Mie Intensity",          &settings.MieIntensity,          0.0f, 5.0f, "%.3f");
			ImGui::SliderFloat("Rayleigh Intensity",     &settings.RayleighIntensity,     0.0f, 5.0f, "%.3f");
			ImGui::SliderFloat("Sun Glow Intensity",     &settings.SunGlowIntensity,      0.0f, 10.0f, "%.3f");
			ImGui::SliderFloat("Horizon Darkening Str",  &settings.HorizonDarkeningStr,   0.1f, 5.0f, "%.3f");
			ImGui::SliderFloat("Exposure Multiplier",    &settings.ExposureMultiplier,     0.1f, 5.0f, "%.3f");
		}

		// --- Night sky ---
		if (ImGui::CollapsingHeader("Night Sky", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::SliderFloat("Night Sky Brightness",   &settings.NightSkyBrightness,    0.0f, 5.0f, "%.3f");
			ImGui::SliderFloat("Twilight Offset",        &settings.TwilightOffset,        0.0f, 0.3f, "%.4f");
			ImGui::SliderFloat("Night Blend Speed",      &settings.NightBlendSpeed,       1.0f, 20.0f, "%.2f");
		}

		// --- Cloud lighting integration ---
		if (ImGui::CollapsingHeader("Cloud Sun Lighting", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::SliderFloat("Cloud Sun Light Intensity",      &settings.CloudSunLightIntensity,      0.0f, 5.0f,  "%.3f");
			ImGui::SliderFloat("Cloud Ambient Intensity",        &settings.CloudAmbientIntensity,       0.0f, 2.0f,  "%.3f");
			ImGui::SliderFloat("Cloud Silverlining Strength",    &settings.CloudSilverliningStrength,   0.0f, 3.0f,  "%.3f");
			ImGui::SliderFloat("Cloud Forward Scatter Strength", &settings.CloudForwardScatterStrength, 0.0f, 3.0f,  "%.3f");
			ImGui::SliderFloat("Cloud Light Absorption",         &settings.CloudLightAbsorption,        0.1f, 5.0f,  "%.3f");
			ImGui::SliderFloat("Cloud Sun Warmth Influence",     &settings.CloudSunWarmthInfluence,     0.0f, 1.0f,  "%.3f");
			ImGui::SliderFloat("Cloud Twilight Ambient",         &settings.CloudTwilightAmbient,        0.0f, 1.0f,  "%.3f");
			ImGui::SliderFloat("Cloud Night Ambient",            &settings.CloudNightAmbient,           0.0f, 0.5f,  "%.3f");
		}

		// --- Reset button ---
		ImGui::Separator();
		if (ImGui::Button("Reset Defaults"))
		{
			settings = TEN::Renderer::AtmosphericSkySettings{};
			settings.Enabled = true; // Keep it enabled after reset.
		}
	}

	// ====================================================================
	// God Ray Tab
	// ====================================================================

	static void DrawGodRayTabContent()
	{
		using namespace TEN::Renderer;
		auto& settings = g_Renderer.GetGodRaySettings();

		ImGui::Checkbox("Enabled", &settings.Enabled);
		ImGui::Separator();

		if (!settings.Enabled)
		{
			ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "God rays are disabled.");
			return;
		}

		// --- Auto-strength info (read-only) ---
		ImGui::Text("Auto Strength Info:");
		{
			auto* levelPtr = dynamic_cast<Level*>(g_GameFlow->GetLevel(CurrentLevel));

			// Sun elevation (from lens flare pitch in TEN short angles).
			float elev = 1.0f;
			if (levelPtr && levelPtr->GetLensFlareEnabled())
			{
				constexpr float SHORT_TO_RAD = (DirectX::XM_2PI / 65536.0f);
				float pitch = (float)levelPtr->GetLensFlarePitch() * SHORT_TO_RAD;
				elev = std::sin(pitch);
			}

			// Cloud coverage: use render settings Coverage (scene-wide), not sun-point transmittance.
			float coverage = 0.0f;
			if (g_SkyCloudSystem.IsCloudAActive() || g_SkyCloudSystem.IsCloudBActive())
			{
				float covA = g_SkyCloudSystem.IsCloudAActive()
					? g_SkyCloudSystem.GetCloudARenderSettings().Coverage : 0.0f;
				float covB = g_SkyCloudSystem.IsCloudBActive()
					? g_SkyCloudSystem.GetCloudBRenderSettings().Coverage : 0.0f;
				coverage = std::max(covA, covB);
			}
			else
			{
				if (levelPtr && levelPtr->HasVolumetricCloudLayer(0))
				{
					auto* vlayer = levelPtr->GetVolumetricCloudLayer(0);
					if (vlayer && vlayer->Settings.Enabled)
						coverage = vlayer->Settings.Coverage;
				}
				else if (levelPtr && levelPtr->HasVolumetricCloudLayer(1))
				{
					auto* vlayer = levelPtr->GetVolumetricCloudLayer(1);
					if (vlayer && vlayer->Settings.Enabled)
						coverage = vlayer->Settings.Coverage;
				}
			}

			float elevFactor     = std::max(1.0f - elev * 0.6f, 0.3f);
			float coverageFactor = std::min(coverage * 3.0f, 1.0f);
			float autoStr        = elevFactor * coverageFactor;
			float finalAuto      = 1.0f + (autoStr - 1.0f) * settings.AutoStrengthMix;

			ImGui::Text("  Sun Elevation:    %.3f", elev);
			ImGui::Text("  Cloud Coverage:   %.3f", coverage);
			ImGui::Text("  Elev Factor:      %.3f", elevFactor);
			ImGui::Text("  Coverage Factor:  %.3f", coverageFactor);
			ImGui::Text("  Auto Strength:    %.3f", autoStr);
			ImGui::Text("  Final Auto Str:   %.3f (mix=%.2f)", finalAuto, settings.AutoStrengthMix);
		}

		// --- Actual GPU constant buffer values (what the shader actually receives) ---
		ImGui::Separator();
		ImGui::Text("GPU CB (actual values sent to shader):");
		{
			const auto& cb = g_Renderer.GetGodRayBuffer();
			bool sunOnScreen = (cb.SunScreenPos.x > -0.5f);
			ImGui::TextColored(
				sunOnScreen ? ImVec4(0.4f, 1.0f, 0.4f, 1.0f) : ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
				"  SunScreenPos:    (%.3f, %.3f) %s",
				cb.SunScreenPos.x, cb.SunScreenPos.y,
				sunOnScreen ? "[ON-SCREEN]" : "[OFF-SCREEN - rays BLOCKED]");
			ImGui::Text("  CB AutoStrength:  %.3f", cb.AutoStrength);
			ImGui::Text("  CB Intensity:     %.3f", cb.Intensity);
			ImGui::Text("  CB Decay:         %.4f", cb.Decay);
			ImGui::Text("  CB Softness:      %.3f", cb.Softness);
			ImGui::Text("  CB SampleCount:   %d",   cb.SampleCount);
			ImGui::Text("  CB SunColor:      (%.2f, %.2f, %.2f)", cb.SunColor.x, cb.SunColor.y, cb.SunColor.z);
			ImGui::Text("  CB SunElevation:  %.3f", cb.SunElevation);
		}

		ImGui::Separator();

		// --- Main controls ---
		if (ImGui::CollapsingHeader("Ray Parameters", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::SliderFloat("Length",          &settings.Length,          0.05f, 1.5f, "%.3f");
			ImGui::SliderFloat("Intensity",       &settings.Intensity,      0.0f,  3.0f, "%.3f");
			ImGui::SliderFloat("Decay",           &settings.Decay,          0.90f, 1.0f, "%.4f");
			ImGui::SliderFloat("Softness",        &settings.Softness,       0.1f,  3.0f, "%.3f");
			ImGui::SliderInt("Sample Count",      &settings.SampleCount,    16,    128);
			ImGui::SliderFloat("Auto Strength Mix", &settings.AutoStrengthMix, 0.0f, 1.0f, "%.3f");
		}

		// --- Reset button ---
		ImGui::Separator();
		if (ImGui::Button("Reset Defaults"))
		{
			settings = TEN::Renderer::GodRay::GodRaySettings{};
			settings.Enabled = true;
		}
	}

	// ====================================================================

	void DrawSkyDebugWindow()
	{
		ImGui::SetNextWindowSize(ImVec2(520, 760), ImGuiCond_FirstUseEver);
		if (!ImGui::Begin("Sky Debug", nullptr, ImGuiWindowFlags_NoCollapse))
		{
			ImGui::End();
			return;
		}

		if (ImGui::BeginTabBar("##SkyDebugTabs"))
		{
			if (ImGui::BeginTabItem("Wolken"))
			{
				DrawCloudTabContent();
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Sun/Moon/Horizon/Stars"))
			{
				TEN::Effects::DrawLensFlareTabContent();
				DrawMoonTabContent();
				auto* levelPtr = dynamic_cast<Level*>(
					g_GameFlow->GetLevel(CurrentLevel));
				DrawHorizonSection(levelPtr);
				DrawStarfieldSection(levelPtr);
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Atmospheric Sky"))
			{
				DrawAtmosphericSkyTabContent();
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("God Rays"))
			{
				DrawGodRayTabContent();
				ImGui::EndTabItem();
			}

			ImGui::EndTabBar();
		}

		ImGui::End();
	}

	// ====================================================================
	// Plain-text summary (legacy debug page compatibility)
	// ====================================================================

	std::string GetSkyCloudDebugText()
	{
		auto info = g_SkyCloudSystem.GetDebugInfo();
		std::ostringstream ss;
		ss << std::fixed << std::setprecision(2);

		ss << "=== Sky/Cloud System ===\n";
		ss << "Preset:     " << SkyCloudSystem::PresetTypeToString(info.CurrentPreset) << "\n";
		ss << "Target:     " << SkyCloudSystem::PresetTypeToString(info.TargetPreset) << "\n";
		ss << "Transition: " << (info.TransitionProgress * 100.0f) << "%\n";
		ss << "Random:     " << (info.RandomModeActive ? "ON" : "OFF");
		if (info.RandomModeActive)
			ss << " (dwell remaining: " << info.RandomDwellRemaining << "s)";
		ss << "\n\n";

		ss << "--- Layers ---\n";
		ss << "Legacy 1:   " << (info.Layer1Enabled ? "ON" : "OFF") << "\n";
		ss << "Legacy 2:   " << (info.Layer2Enabled ? "ON" : "OFF") << "\n";
		ss << "Cloud A:    " << (info.CloudAEnabled ? "ON" : "OFF")
			<< " [" << CloudCategoryToString(info.CloudACategory) << "]\n";
		ss << "Cloud B:    " << (info.CloudBEnabled ? "ON" : "OFF")
			<< " [" << CloudCategoryToString(info.CloudBCategory) << "]\n\n";

		ss << "--- Lens Flare Occlusion ---\n";
		ss << "Cloud A T:  " << info.CloudATransmittance << "\n";
		ss << "Cloud B T:  " << info.CloudBTransmittance << "\n";
		ss << "Combined:   " << info.CombinedTransmittance << "\n";

		return ss.str();
	}
}
