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
		case CloudCategory::CumulonimbusVertical: return "CumulonimbusVertical";
		default:                                  return "Unknown";
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

		std::vector<CloudDebugParam> params;

		//                Label               Ptr                       Min      Max       Step       Fmt         Default
		params.push_back({"Coverage",         &snap.Coverage,           0.0f,    1.0f,     0.01f,    "%.3f",     def(&VolumetricCloudLayerSnapshot::Coverage)});
		params.push_back({"Density",          &snap.Density,            0.0f,    10.0f,    0.1f,     "%.2f",     def(&VolumetricCloudLayerSnapshot::Density)});
		params.push_back({"Bottom Height",    &snap.BottomHeight,       100.0f,  200000.0f, 100.0f,  "%.0f",     def(&VolumetricCloudLayerSnapshot::BottomHeight)});
		params.push_back({"Thickness",        &snap.Thickness,          100.0f,  200000.0f, 100.0f,  "%.0f",     def(&VolumetricCloudLayerSnapshot::Thickness)});
		params.push_back({"Wind Dir X",       &snap.WindDirectionX,    -1.0f,    1.0f,     0.01f,    "%.3f",     def(&VolumetricCloudLayerSnapshot::WindDirectionX)});
		params.push_back({"Wind Dir Y",       &snap.WindDirectionY,    -1.0f,    1.0f,     0.01f,    "%.3f",     def(&VolumetricCloudLayerSnapshot::WindDirectionY)});
		params.push_back({"Wind Speed",       &snap.WindSpeed,          0.0f,    8.0f,     0.001f,   "%.4f",     def(&VolumetricCloudLayerSnapshot::WindSpeed)});
		params.push_back({"Evolution Speed",  &snap.EvolutionSpeed,     0.0f,    5.0f,     0.01f,    "%.3f",     def(&VolumetricCloudLayerSnapshot::EvolutionSpeed)});
		params.push_back({"Shape Scale",      &snap.ShapeScale,         0.0f,    0.01f,    0.00001f, "%.6f",     def(&VolumetricCloudLayerSnapshot::ShapeScale)});
		params.push_back({"Detail Scale",     &snap.DetailScale,        0.0f,    0.01f,    0.0001f,  "%.5f",     def(&VolumetricCloudLayerSnapshot::DetailScale)});
		params.push_back({"Detail Strength",  &snap.DetailStrength,     0.0f,    1.0f,     0.01f,    "%.3f",     def(&VolumetricCloudLayerSnapshot::DetailStrength)});
		params.push_back({"Absorption",       &snap.Absorption,         0.0f,    25.0f,    0.1f,     "%.2f",     def(&VolumetricCloudLayerSnapshot::Absorption)});
		params.push_back({"Ambient Contrib",  &snap.AmbientContrib,     0.0f,    1.0f,     0.01f,    "%.3f",     def(&VolumetricCloudLayerSnapshot::AmbientContrib)});
		params.push_back({"Silverlining",     &snap.SilverliningStr,    0.0f,    1.0f,     0.01f,    "%.3f",     def(&VolumetricCloudLayerSnapshot::SilverliningStr)});
		params.push_back({"Horizon Fade",     &snap.HorizonFade,        0.0f,    1.0f,     0.01f,    "%.3f",     def(&VolumetricCloudLayerSnapshot::HorizonFade)});
		params.push_back({"Distance Fade",    &snap.DistanceFade,       0.0f,    1.0f,     0.01f,    "%.3f",     def(&VolumetricCloudLayerSnapshot::DistanceFade)});

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
			"None", "CirrusHigh", "AltocumulusMid", "StratocumulusLow", "CumulonimbusVertical"
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
	// Draw a cloud layer section with all parameter sliders.
	// ====================================================================

	static void DrawLayerSection(
		const char* title,
		const char* idPrefix,
		VolumetricCloudLayerSnapshot& snap,
		const VolumetricCloudLayerSnapshot* defaults)
	{
		if (!ImGui::CollapsingHeader(title, ImGuiTreeNodeFlags_DefaultOpen))
			return;

		ImGui::Indent(8.0f);

		// Enabled toggle.
		ImGui::Checkbox("Enabled", &snap.Enabled);

		// Category combo.
		DrawCategoryCombo("Category", snap.Category);

		ImGui::Separator();

		// Build and draw all parameter sliders.
		auto params = BuildParamList(snap, defaults);
		for (auto& p : params)
			DrawParamSlider(p, idPrefix);

		// Reset all button.
		if (defaults)
		{
			ImGui::Separator();
			char resetId[64];
			snprintf(resetId, sizeof(resetId), "Reset All##%s", idPrefix);
			if (ImGui::Button(resetId))
				snap = *defaults;
			ImGui::SameLine();
			ImGui::TextDisabled("(revert to preset defaults)");
		}

		ImGui::Unindent(8.0f);
	}

	// ====================================================================
	// Main overlay
	// ====================================================================

	void DrawSkyCloudDebugOverlay()
	{
		ImGui::SetNextWindowSize(ImVec2(520, 720), ImGuiCond_FirstUseEver);
		if (!ImGui::Begin("Sky / Cloud Debug", nullptr, ImGuiWindowFlags_NoCollapse))
		{
			ImGui::End();
			return;
		}

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

				static float transitionDuration = 30.0f;
				ImGui::SetNextItemWidth(100.0f);
				ImGui::DragFloat("##transDur", &transitionDuration, 1.0f, 1.0f, 300.0f, "%.0f s");
				ImGui::SameLine();
				if (ImGui::Button("Transition"))
					g_SkyCloudSystem.TransitionToPreset(selectedType, transitionDuration);

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

			DrawLayerSection("Cloud Layer A (Live)", "liveA", state.CloudA, defaultsA);
			DrawLayerSection("Cloud Layer B (Live)", "liveB", state.CloudB, defaultsB);
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
					ImGui::DragFloat("Transition Duration", &def->DefaultTransitionDuration, 1.0f, 1.0f, 300.0f, "%.0f s");
					ImGui::DragFloat("Random Weight", &def->RandomWeight, 0.1f, 0.0f, 10.0f, "%.1f");
					ImGui::Checkbox("Allow in Random", &def->AllowInRandom);
					ImGui::DragFloat("High Layer Lead", &def->HighLayerLeadFraction, 0.01f, 0.0f, 1.0f, "%.2f");

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
