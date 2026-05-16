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
#include <imgui_internal.h>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>

#include "Game/control/control.h"
#include "Game/Effects/LensFlareDebug.h"
#include "Game/effects/weather.h"
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
		case CloudCategory::AltocumulusMid:       return "AltocumulusMid";
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

	// Number of entries in WeatherPresetType (excluding Count).
	static constexpr int REAL_PRESET_COUNT = static_cast<int>(WeatherPresetType::Count);

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
		params.push_back({"Bottom Height",    &snap.BottomHeight,       100.0f,  200000.0f, 100.0f,  "%.0f",     def(&VolumetricCloudLayerSnapshot::BottomHeight)});
		params.push_back({"Horizon Width",    &snap.AltoHorizonWidth,   0.0f,    1.0f,     0.01f,    "%.3f",     def(&VolumetricCloudLayerSnapshot::AltoHorizonWidth)});
		params.push_back({"Evolution Speed",  &snap.EvolutionSpeed,     0.0f,    5.0f,     0.01f,    "%.3f",     def(&VolumetricCloudLayerSnapshot::EvolutionSpeed)});
		params.push_back({"Curl Warp Str",    &snap.CurlWarpStrength,   0.0f,    2.0f,     0.01f,    "%.3f",     def(&VolumetricCloudLayerSnapshot::CurlWarpStrength)});
		params.push_back({"Horizon Fade",     &snap.HorizonFade,        0.0f,    1.0f,     0.01f,    "%.3f",     def(&VolumetricCloudLayerSnapshot::HorizonFade)});
		params.push_back({"Distance Fade",    &snap.DistanceFade,       0.0f,    1.0f,     0.01f,    "%.3f",     def(&VolumetricCloudLayerSnapshot::DistanceFade)});
		params.push_back({"Horizon Mesh Bleed", &snap.HorizonMeshBleed,  0.0f,    1.0f,     0.01f,    "%.3f",     def(&VolumetricCloudLayerSnapshot::HorizonMeshBleed)});
		// --- Altocumulus-specific parameters (meaningful only when Category == AltocumulusMid) ---
		params.push_back({"Alto Billow Str",  &snap.AltoBillowStrength, 0.0f,    1.0f,     0.01f,    "%.3f",     def(&VolumetricCloudLayerSnapshot::AltoBillowStrength)});
		params.push_back({"Alto Cov Soft W",  &snap.AltoCovSoftWidth,   0.0f,    0.25f,    0.005f,   "%.4f",     def(&VolumetricCloudLayerSnapshot::AltoCovSoftWidth)});
		params.push_back({"Alto Absorption", &snap.AltoAbsorption,      0.0f,    5.0f,     0.05f,    "%.3f",     def(&VolumetricCloudLayerSnapshot::AltoAbsorption)});
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
		params.push_back({"Bleed Depth",        &snap.AltoBleedDepth,       0.0f, 100.0f,     0.1f,     "%.2f",     def(&VolumetricCloudLayerSnapshot::AltoBleedDepth)});
		params.push_back({"Horizon Gradient",   &snap.AltoHorizonGradientFade, 0.0f, 1.0f,    0.01f,    "%.3f",     def(&VolumetricCloudLayerSnapshot::AltoHorizonGradientFade)});
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
		// --- Edge quality tuning ---
		params.push_back({"FBM Scale",         &snap.AltoFbmScale,               0.5f,   4.0f, 0.02f, "%.3f", def(&VolumetricCloudLayerSnapshot::AltoFbmScale)});
		params.push_back({"Jitter Strength",   &snap.JitterStrength,              0.0f,   1.0f, 0.01f, "%.3f", def(&VolumetricCloudLayerSnapshot::JitterStrength)});
		params.push_back({"Jitter Abs Cap",    &snap.AltoJitterAbsCap,            0.1f,  10.0f, 0.05f, "%.2f", def(&VolumetricCloudLayerSnapshot::AltoJitterAbsCap)});
		params.push_back({"Upsamp Sigma2",     &snap.UpsampleSpatialSigma2,       0.5f,   8.0f, 0.05f, "%.2f", def(&VolumetricCloudLayerSnapshot::UpsampleSpatialSigma2)});
		params.push_back({"Temporal A Low",    &snap.TemporalAlphaLow,            0.0f,   0.3f, 0.005f,"%.3f", def(&VolumetricCloudLayerSnapshot::TemporalAlphaLow)});
		params.push_back({"Temporal A High",   &snap.TemporalAlphaHigh,           0.7f,   1.0f, 0.005f,"%.3f", def(&VolumetricCloudLayerSnapshot::TemporalAlphaHigh)});

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

	static bool DrawCategoryCombo(const char* label, CloudCategory& category, bool isLayerA)
	{
		// Layer A is reserved for Aurora (and reserved water surface in the future).
		// Layer B handles all volumetric cloud categories.
		static const char* namesA[] = { "None", "Aurora" };
		static const char* namesB[] = { "None", "AltocumulusMid", "Aurora" };

		bool changed = false;

		if (isLayerA)
		{
			// Map snap.Category (full enum) to the reduced 2-entry combo.
			int current = (category == CloudCategory::Aurora) ? 1 : 0;
			if (ImGui::Combo(label, &current, namesA, IM_ARRAYSIZE(namesA)))
			{
				category = (current == 1) ? CloudCategory::Aurora : CloudCategory::None;
				changed = true;
			}
		}
		else
		{
			int current = (int)category;
			if (ImGui::Combo(label, &current, namesB, IM_ARRAYSIZE(namesB)))
			{
				category = (CloudCategory)current;
				changed = true;
			}
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
	// Dual-zone blend threshold widget.
	//
	// Renders a white-to-black gradient bar with two downward-pointing
	// triangle markers that the user can drag:
	//   Left  marker = BlendThresholdHigh (bright cutoff, near white)
	//   Right marker = BlendThresholdLow  (dark  cutoff, near black)
	//
	// The region between the two markers is highlighted in blue —
	// that's the alpha-blend zone.  Outside the markers = screen blend.
	//
	// Coordinate mapping: bar-x = (1 - luminance) * barWidth
	//   So luma=1 (white) is at the LEFT, luma=0 (black) is at the RIGHT.
	// ====================================================================

	static bool DrawBlendThresholdWidget(float& threshHigh, float& transWidth, float& threshLow, const char* id)
	{
		bool changed = false;

		const float barH     = 18.0f;
		const float mkH      = 11.0f;        // arrow triangle height
		const float mkHW     = 8.0f;         // arrow hit half-width
		const float mkHitH   = mkH + 6.0f;   // arrow button height (extra tolerance)
		const float tierStep = 10.0f;        // vertical stagger between arrow tiers
		const float valH     = 28.0f;        // room for value text
		const float totalH   = barH + 1.0f + 2.0f * tierStep + mkHitH + valH;

		ImDrawList* dl     = ImGui::GetWindowDrawList();
		ImVec2      origin = ImGui::GetCursorScreenPos();
		float       barW   = ImGui::GetContentRegionAvail().x - 10.0f;
		if (barW < 60.0f) barW = 60.0f;

		ImVec2 barMin  = origin;
		ImVec2 barMax  = ImVec2(origin.x + barW, origin.y + barH);
		float  mkY     = origin.y + barH + 1.0f;
		float  mkYHigh = mkY;                     // tier 0: threshHigh (left red)
		float  mkYTrans= mkY + tierStep;          // tier 1: transWidth  (orange)
		float  mkYLow  = mkY + 2.0f * tierStep;  // tier 2: threshLow  (right red)

		// pixel <-> luma helpers
		auto xOf    = [&](float luma) -> float { return origin.x + (1.0f - luma) * barW; };
		auto lumaOf = [&](float px)   -> float { return 1.0f - std::clamp((px - origin.x) / barW, 0.0f, 1.0f); };

		float xHigh  = xOf(threshHigh);
		float xTrans = xOf(threshHigh - transWidth);  // orange arrow: lower edge of bright smoothstep
		float xLow   = xOf(threshLow);

		// ---- gradient bar (white left -> black right) ----
		dl->AddRectFilledMultiColor(barMin, barMax,
			IM_COL32(255, 255, 255, 255), IM_COL32(0, 0, 0, 255),
			IM_COL32(0, 0, 0, 255),       IM_COL32(255, 255, 255, 255));
		if (xHigh < xLow)
			dl->AddRectFilled(ImVec2(xHigh, origin.y), ImVec2(xLow, origin.y + barH),
				IM_COL32(80, 140, 255, 55));
		// Orange tint over the bright-side transition zone (threshHigh ± transWidth).
		{
			float xTL = std::max(origin.x, xOf(std::min(1.0f, threshHigh + transWidth)));
			float xTR = std::min(barMax.x, xTrans);
			if (xTL < xTR)
				dl->AddRectFilled(ImVec2(xTL, origin.y), ImVec2(xTR, origin.y + barH),
					IM_COL32(255, 160, 40, 45));
		}
		dl->AddRect(barMin, barMax, IM_COL32(160, 160, 160, 220));

		// ---- bar button: mouse-wheel + tooltip (bar area only) ----
		ImGui::SetCursorScreenPos(origin);
		char barId[128]; snprintf(barId, sizeof(barId), "##blendbar_%s", id);
		ImGui::InvisibleButton(barId, ImVec2(barW, barH));
		bool barHov = ImGui::IsItemHovered();
		if (barHov && ImGui::GetIO().MouseWheel != 0.0f)
		{
			float step = ImGui::GetIO().MouseWheel * 0.01f;
			float mx   = ImGui::GetIO().MousePos.x;
			if (std::abs(mx - xHigh) <= std::abs(mx - xLow))
				threshHigh = std::clamp(threshHigh + step, threshLow + 0.02f, 1.0f);
			else
				threshLow  = std::clamp(threshLow  + step, 0.0f, threshHigh - 0.02f);
			changed = true;
		}
		if (barHov)
			ImGui::SetTooltip(
				"Drag arrows or use mouse-wheel to adjust:\n"
				"  Left  red  (Hi=%.3f):   bright cutoff - screen blend above this\n"
				"  Orange     (W=%.4f):    transition half-width around Hi cutoff\n"
				"  Right red  (Lo=%.3f):   dark cutoff  - screen blend below this\n"
				"  Blue zone between red arrows = alpha blend zone",
				threshHigh, transWidth, threshLow);

		// ---- high (left) arrow button  [tier 0] ----
		ImGui::SetNextItemAllowOverlap();
		ImGui::SetCursorScreenPos(ImVec2(xHigh - mkHW, mkYHigh));
		char highId[128]; snprintf(highId, sizeof(highId), "##blendhigh_%s", id);
		ImGui::InvisibleButton(highId, ImVec2(mkHW * 2.0f, mkHitH));
		bool highActive  = ImGui::IsItemActive();
		bool highHovered = ImGui::IsItemHovered();
		if (highActive)
		{
			threshHigh = std::clamp(lumaOf(ImGui::GetIO().MousePos.x), threshLow + 0.02f, 1.0f);
			changed = true;
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
		}
		else if (highHovered)
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

		// ---- transition-width (orange) arrow button  [tier 1] ----
		ImGui::SetNextItemAllowOverlap();
		ImGui::SetCursorScreenPos(ImVec2(xTrans - mkHW, mkYTrans));
		char transId[128]; snprintf(transId, sizeof(transId), "##blendtrans_%s", id);
		ImGui::InvisibleButton(transId, ImVec2(mkHW * 2.0f, mkHitH));
		bool transActive  = ImGui::IsItemActive();
		bool transHovered = ImGui::IsItemHovered();
		if (transActive)
		{
			float mx   = ImGui::GetIO().MousePos.x;
			float maxW = std::clamp(threshHigh - threshLow - 0.02f, 0.005f, 0.4f);
			transWidth = std::clamp(threshHigh - lumaOf(mx), 0.005f, maxW);
			changed = true;
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
		}
		else if (transHovered)
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

		// ---- low (right) arrow button  [tier 2] ----
		ImGui::SetNextItemAllowOverlap();
		ImGui::SetCursorScreenPos(ImVec2(xLow - mkHW, mkYLow));
		char lowId[128]; snprintf(lowId, sizeof(lowId), "##blendlow_%s", id);
		ImGui::InvisibleButton(lowId, ImVec2(mkHW * 2.0f, mkHitH));
		bool lowActive  = ImGui::IsItemActive();
		bool lowHovered = ImGui::IsItemHovered();
		if (lowActive)
		{
			threshLow = std::clamp(lumaOf(ImGui::GetIO().MousePos.x), 0.0f, threshHigh - 0.02f);
			changed = true;
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
		}
		else if (lowHovered)
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

		// ---- recompute positions after possible drag ----
		xHigh  = xOf(threshHigh);
		xTrans = xOf(threshHigh - transWidth);
		xLow   = xOf(threshLow);

		// ---- marker lines: extend from bar top down to each arrow tip ----
		dl->AddLine(ImVec2(xHigh,  origin.y), ImVec2(xHigh,  mkYHigh  + mkH), IM_COL32(255,  80,  80, 170), 1.5f);
		dl->AddLine(ImVec2(xTrans, origin.y), ImVec2(xTrans, mkYTrans + mkH), IM_COL32(255, 160,  40, 150), 1.0f);
		dl->AddLine(ImVec2(xLow,   origin.y), ImVec2(xLow,   mkYLow   + mkH), IM_COL32(255,  80,  80, 170), 1.5f);

		// ---- arrow triangles (each at its own tier Y) ----
		auto DrawArrow = [&](float cx, float ay, bool active, bool hovered, bool isOrange) {
			ImU32 fill, edge;
			if (isOrange)
			{
				fill = active  ? IM_COL32(255, 235, 100, 255)
				     : hovered ? IM_COL32(255, 195,  60, 255)
				     :           IM_COL32(210, 130,  20, 255);
				edge = IM_COL32(140, 80, 0, 255);
			}
			else
			{
				fill = active  ? IM_COL32(255, 210,  60, 255)
				     : hovered ? IM_COL32(255, 150,  80, 255)
				     :           IM_COL32(220,  60,  60, 255);
				edge = IM_COL32(160, 30, 30, 255);
			}
			dl->AddTriangleFilled(
				ImVec2(cx - mkHW + 1.0f, ay),
				ImVec2(cx + mkHW - 1.0f, ay),
				ImVec2(cx, ay + mkH), fill);
			dl->AddTriangle(
				ImVec2(cx - mkHW + 1.0f, ay),
				ImVec2(cx + mkHW - 1.0f, ay),
				ImVec2(cx, ay + mkH), edge, 1.2f);
		};
		DrawArrow(xHigh,  mkYHigh,  highActive,  highHovered,  false);
		DrawArrow(xTrans, mkYTrans, transActive, transHovered, true);
		DrawArrow(xLow,   mkYLow,   lowActive,   lowHovered,   false);

		// ---- labels ----
		float lblY = mkY + mkH + 2.0f;
		const ImU32 scrU = ImGui::ColorConvertFloat4ToU32(ImVec4(0.55f, 0.90f, 0.55f, 1.0f));
		const ImU32 alpU = ImGui::ColorConvertFloat4ToU32(ImVec4(0.55f, 0.75f, 1.00f, 1.0f));
		dl->AddText(ImVec2(origin.x + 2.0f, lblY),         scrU, "screen");
		dl->AddText(ImVec2(origin.x + barW - 42.0f, lblY), scrU, "screen");
		if (xHigh < xLow)
		{
			float midX = (xHigh + xLow) * 0.5f - 14.0f;
			dl->AddText(ImVec2(midX, lblY), alpU, "alpha");
		}

		// ---- value display ----
		char valBuf[80];
		snprintf(valBuf, sizeof(valBuf), "Hi: %.3f  W: %.4f  Lo: %.3f", threshHigh, transWidth, threshLow);
		ImVec2 ts = ImGui::CalcTextSize(valBuf);
		dl->AddText(ImVec2(origin.x + barW * 0.5f - ts.x * 0.5f, lblY + 14.0f),
			IM_COL32(190, 190, 190, 255), valBuf);

		// ---- advance cursor past entire widget ----
		ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + totalH));
		ImGui::Dummy(ImVec2(barW, 0.0f));

		return changed;
	}

	// ====================================================================
	// Draw a cloud layer section with all parameter sliders.
	// ====================================================================

	// ====================================================================
	// Build a Lua snippet string from a snapshot, ready to copy.
	//
	// For "liveA"/"defA" idPrefixes it wraps in Flow.SetVolumetricCloudLayerA({})
	// or outputs as cloudA = {} for preset editor sections.
	// ====================================================================

	static std::string BuildCloudLayerLua(const VolumetricCloudLayerSnapshot& snap, const char* idPrefix)
	{
		bool isLayerA = (strchr(idPrefix, 'A') != nullptr);
		bool isPreset = (strncmp(idPrefix, "def", 3) == 0);

		std::ostringstream ss;

		if (isPreset)
			ss << (isLayerA ? "cloudA = {\n" : "cloudB = {\n");
		else
			ss << (isLayerA ? "Flow.SetVolumetricCloudLayerA({\n" : "Flow.SetVolumetricCloudLayerB({\n");

		// Helpers: field writes with aligned columns.
		char vbuf[64];
		auto fld = [&](const char* key, const char* fmt, float val)
		{
			snprintf(vbuf, sizeof(vbuf), fmt, val);
			ss << "    " << std::left << std::setw(32) << (std::string(key) + " ") << "= " << vbuf << ",\n";
		};
		auto ifld = [&](const char* key, float val)
		{
			snprintf(vbuf, sizeof(vbuf), "%.0f", val);
			ss << "    " << std::left << std::setw(32) << (std::string(key) + " ") << "= " << vbuf << ",\n";
		};
		auto bfld = [&](const char* key, bool val)
		{
			ss << "    " << std::left << std::setw(32) << (std::string(key) + " ") << "= " << (val ? "true" : "false") << ",\n";
		};
		auto sfld = [&](const char* key, const char* val)
		{
			ss << "    " << std::left << std::setw(32) << (std::string(key) + " ") << "= \"" << val << "\",\n";
		};

		bfld("enabled",                         snap.Enabled);
		sfld("category",                         CloudCategoryToString(snap.Category));
		fld( "coverage",              "%.4f",    snap.Coverage);
		ifld("bottomHeight",                     snap.BottomHeight);
		ifld("horizonWidth",                     snap.Thickness);
		fld( "altoHorizonWidth",      "%.4f",    snap.AltoHorizonWidth);
		fld( "evolutionSpeed",        "%.4f",    snap.EvolutionSpeed);
		fld( "curlWarpStrength",      "%.4f",    snap.CurlWarpStrength);
		fld( "horizonFade",           "%.4f",    snap.HorizonFade);
		fld( "distanceFade",          "%.4f",    snap.DistanceFade);
		fld( "horizonMeshBleed",      "%.4f",    snap.HorizonMeshBleed);
		ss << "    -- Altocumulus-specific\n";
		fld( "altoBillowStrength",    "%.4f",    snap.AltoBillowStrength);
		fld( "altoCovSoftWidth",      "%.4f",    snap.AltoCovSoftWidth);
		fld( "altoAbsorption",        "%.4f",    snap.AltoAbsorption);
		fld( "altoCloudSize",         "%.4f",    snap.AltoCloudSize);
		fld( "altoCloudAmount",       "%.4f",    snap.AltoCloudAmount);
		fld( "altoCloudBrightness",   "%.4f",    snap.AltoCloudBrightness);
		fld( "altoCloudColorR",       "%.4f",    snap.AltoCloudColorR);
		fld( "altoCloudColorG",       "%.4f",    snap.AltoCloudColorG);
		fld( "altoCloudColorB",       "%.4f",    snap.AltoCloudColorB);
		fld( "altoCloudColorDarkR",   "%.4f",    snap.AltoCloudColorDarkR);
		fld( "altoCloudColorDarkG",   "%.4f",    snap.AltoCloudColorDarkG);
		fld( "altoCloudColorDarkB",   "%.4f",    snap.AltoCloudColorDarkB);
		fld( "altoFbmLacunarity",     "%.4f",    snap.AltoFbmLacunarity);
		fld( "altoFbmGain",           "%.4f",    snap.AltoFbmGain);
		ifld("altoThickness",                    snap.AltoThickness);
		fld( "altoBottomSoftness",    "%.4f",    snap.AltoBottomSoftness);
		fld( "altoZenithBias",        "%.4f",    snap.AltoZenithBias);
		fld( "altoHeightBlendPower",  "%.4f",    snap.AltoHeightBlendPower);
		fld( "altoBleedDepth",        "%.4f",    snap.AltoBleedDepth);
		fld( "altoHorizonGradientFade","%.4f",   snap.AltoHorizonGradientFade);
		ss << "    -- Composite blend\n";
		fld( "blendThresholdHigh",    "%.4f",    snap.BlendThresholdHigh);
		fld( "blendThresholdHighWidth","%.4f",   snap.BlendThresholdHighWidth);
		fld( "blendThresholdLow",     "%.4f",    snap.BlendThresholdLow);
		ss << "    -- Lightning\n";
		bfld("lightningEnabled",                 snap.LightningEnabled);
		fld( "lightningStrikeFreq",   "%.4f",    snap.LightningStrikeFreq);
		fld( "lightningInternalFreq", "%.4f",    snap.LightningInternalFreq);
		fld( "lightningSpeed",        "%.4f",    snap.LightningSpeed);
		fld( "lightningInternalSpeed","%.4f",    snap.LightningInternalSpeed);
		fld( "lightningGlowIntensity","%.4f",    snap.LightningGlowIntensity);
		fld( "lightningFlashIntensity","%.4f",   snap.LightningFlashIntensity);
		fld( "lightningBoltColorR",   "%.4f",    snap.LightningBoltColorR);
		fld( "lightningBoltColorG",   "%.4f",    snap.LightningBoltColorG);
		fld( "lightningBoltColorB",   "%.4f",    snap.LightningBoltColorB);
		fld( "lightningAmbientContrib","%.4f",   snap.LightningAmbientContrib);
		fld( "lightningBoltLengthScale","%.4f",  snap.LightningBoltLengthScale);
		fld( "lightningBoltThicknessScale","%.4f",snap.LightningBoltThicknessScale);
		ss << "    -- Technical tuning\n";
		fld( "altoFbmScale",          "%.4f",    snap.AltoFbmScale);
		fld( "jitterStrength",        "%.4f",    snap.JitterStrength);
		fld( "altoJitterAbsCap",      "%.4f",    snap.AltoJitterAbsCap);
		fld( "upsampleSpatialSigma2", "%.4f",    snap.UpsampleSpatialSigma2);
		fld( "temporalAlphaLow",      "%.4f",    snap.TemporalAlphaLow);
		fld( "temporalAlphaHigh",     "%.4f",    snap.TemporalAlphaHigh);

		if (isPreset)
			ss << "}\n";
		else
			ss << "})\n";

		return ss.str();
	}

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

		bool isLayerA = (idPrefix != nullptr && strchr(idPrefix, 'A') != nullptr);

		// Layer A is reserved for Aurora; sanitize stale volumetric category data.
		if (isLayerA && snap.Category == CloudCategory::AltocumulusMid)
			snap.Category = CloudCategory::None;

		// Enabled toggle.
		if (ImGui::Checkbox("Enabled", &snap.Enabled))
			changed = true;

		// Category combo.
		if (DrawCategoryCombo("Category", snap.Category, isLayerA))
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

		// Layer A only supports Aurora — hide all volumetric cloud parameters.
		if (isLayerA)
		{
			ImGui::TextDisabled("(Layer A parameters are shown only when Aurora is active.)");
			ImGui::Unindent(8.0f);
			return changed;
		}

		// One-click pattern presets for requested cloud types.
		if (snap.Category == CloudCategory::AltocumulusMid)
		{
			if (ImGui::Button("Apply Tuned Pattern"))
			{
				ApplyPatternPresetForCategory(snap);
				changed = true;
			}
			ImGui::SameLine();
			ImGui::TextDisabled("Altocumulus grouping (Schaefchen)");
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

			// ----------------------------------------------------------------
			// Composite Blend — dual-zone gradient widget with draggable arrows
			// ----------------------------------------------------------------
			ImGui::Separator();
			ImGui::TextDisabled("Composite Blend");
			ImGui::TextDisabled("  Drag arrows to set blend zones (mouse-wheel also works):");
			ImGui::TextDisabled("  Screen blend: bright (left) + very dark (right)  |  Alpha blend: middle.");
			char wgtId[64];
			snprintf(wgtId, sizeof(wgtId), "blend_%s", idPrefix);
			changed |= DrawBlendThresholdWidget(snap.BlendThresholdHigh, snap.BlendThresholdHighWidth, snap.BlendThresholdLow, wgtId);
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

		// Copy Lua button — copies all current values as a ready-to-paste Lua snippet.
		{
			ImGui::Separator();
			char copyId[64];
			snprintf(copyId, sizeof(copyId), "Copy Lua to Clipboard##%s", idPrefix);
			if (ImGui::Button(copyId))
				ImGui::SetClipboardText(BuildCloudLayerLua(snap, idPrefix).c_str());
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip(
					"Copies all current values as a Lua table to the clipboard.\n"
					"Paste into Flow.SetVolumetricCloudLayerA/B or DefineWeatherPreset cloudA/cloudB.");
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
		// Performance Diagnostics — live shader CB values every frame.
		// Use this to identify what changes at the moment of an FPS spike.
		// ----------------------------------------------------------------
		if (ImGui::CollapsingHeader("Performance Diagnostics", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Indent(8.0f);

			const auto& io  = ImGui::GetIO();
			const auto& cb  = g_Renderer.GetVolumetricCloudCB();

			float fps = io.Framerate;
			float ms  = io.DeltaTime * 1000.0f;
			ImGui::TextColored(fps < 15.0f ? ImVec4(1,0.3f,0.3f,1) : ImVec4(0.3f,1,0.3f,1),
				"FPS: %.1f  (%.2f ms)", fps, ms);

			ImGui::Separator();

			// Render path
			bool dualPath = g_SkyCloudSystem.IsCloudAActive() || g_SkyCloudSystem.IsCloudBActive();
			ImGui::Text("Render Path:  %s", dualPath ? "DualVolumetricClouds" : "inactive");
			ImGui::Text("CloudA Active: %s   CloudB Active: %s",
				g_SkyCloudSystem.IsCloudAActive() ? "YES" : "no",
				g_SkyCloudSystem.IsCloudBActive() ? "YES" : "no");
			ImGui::Text("FrameCounter A: %d   B: %d",
				g_Renderer.GetCloudFrameCounterA(),
				g_Renderer.GetCloudFrameCounterB());

			ImGui::Separator();

			// Temporal
			const char* temporalStr =
				(cb.TemporalEnabled == 0) ? "OFF" :
				(cb.TemporalEnabled == 1) ? "WARMUP (all pixels)" :
				                            "ACTIVE (checkerboard)";
			ImGui::TextColored(cb.TemporalEnabled == 2 ? ImVec4(0.3f,1,0.3f,1) : ImVec4(1,0.8f,0.2f,1),
				"TemporalEnabled: %d (%s)", cb.TemporalEnabled, temporalStr);
			ImGui::Text("FrameIndex: %.0f   PrimarySteps: %d   ShadowSteps: %d",
				cb.FrameIndex, cb.PrimaryStepCount, cb.ShadowStepCount);

			ImGui::Separator();

			// Morph/transition state
			ImGui::TextColored(cb.MorphActive > 0.5f ? ImVec4(1,0.4f,0.4f,1) : ImVec4(0.7f,0.7f,0.7f,1),
				"MorphActive: %.2f   DissolvePhase: %.3f   FormationPhase: %.3f",
				cb.MorphActive, cb.DissolvePhase, cb.FormationPhase);

			ImGui::Separator();

			// Density parameters (most likely culprits for sudden GPU cost change)
			ImGui::Text("AltoCloudAmount: %.3f   EvolutionSpeed: %.3f",
				cb.AltoCloudAmount, cb.EvolutionSpeed);
			ImGui::Text("AltoHorizonWidth: %.4f   DriftOutProgress: %.3f",
				cb.AltoHorizonWidth, cb.DriftOutProgress);
			ImGui::Text("AltoCloudSize: %.3f   AltoAbsorption: %.3f",
				cb.AltoCloudSize, cb.AltoAbsorption);

			ImGui::Unindent(8.0f);
		}

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

			// Dwell timer.
			if (info.DwellTarget >= 0.0f)
			{
				float dwellPct = (info.DwellTarget > 0.0f) ? (info.DwellElapsed / info.DwellTarget) : 1.0f;
				char dwellLabel[64];
				snprintf(dwellLabel, sizeof(dwellLabel), "Dwell %.1fs / %.1fs", info.DwellElapsed, info.DwellTarget);
				ImGui::ProgressBar(dwellPct, ImVec2(-1, 0), dwellLabel);
			}
			else if (g_SkyCloudSystem.IsTransitioning())
			{
				ImGui::TextDisabled("Dwell: -- (transitioning, starts after)");
			}
			else
			{
				ImGui::TextDisabled("Dwell: none (staying)");
			}

			ImGui::Separator();

			// Layer status summary.
			ImGui::Text("Legacy Layer 1: %s", info.Layer1Enabled ? "ON" : "OFF");
			ImGui::Text("Legacy Layer 2: %s", info.Layer2Enabled ? "ON" : "OFF");

			// Cloud A — show active preset and transition target.
			if (info.LayerATransitioning)
				ImGui::Text("Cloud A: %s  -> %s  [%s]  (%.0f%%)",
					SkyCloudSystem::PresetTypeToString(info.LayerAPreset),
					SkyCloudSystem::PresetTypeToString(info.LayerATargetPreset),
					CloudCategoryToString(info.CloudACategory),
					info.LayerATransitionProgress * 100.0f);
			else
				ImGui::Text("Cloud A: %s  [%s]  %s",
					SkyCloudSystem::PresetTypeToString(info.LayerAPreset),
					CloudCategoryToString(info.CloudACategory),
					info.CloudAEnabled ? "ON" : "OFF");
			// Cloud B — show active preset and transition target.
			if (info.LayerBTransitioning)
				ImGui::Text("Cloud B: %s  -> %s  [%s]  (%.0f%%)",
					SkyCloudSystem::PresetTypeToString(info.LayerBPreset),
					SkyCloudSystem::PresetTypeToString(info.LayerBTargetPreset),
					CloudCategoryToString(info.CloudBCategory),
					info.LayerBTransitionProgress * 100.0f);
			else
				ImGui::Text("Cloud B: %s  [%s]  %s",
					SkyCloudSystem::PresetTypeToString(info.LayerBPreset),
					CloudCategoryToString(info.CloudBCategory),
					info.CloudBEnabled ? "ON" : "OFF");
			if (info.LayerBDwellTarget >= 0.0f)
				ImGui::TextDisabled("  B Dwell: %.1fs / %.1fs", info.LayerBDwellElapsed, info.LayerBDwellTarget);
			else if (info.LayerBTransitioning)
				ImGui::TextDisabled("  B Dwell: -- (starts after transition)");

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

		ImGui::Unindent(8.0f);
		}

		// ----------------------------------------------------------------
		// Per-Layer Preset Switcher (Layer A and Layer B independently)
		// ----------------------------------------------------------------
		if (ImGui::CollapsingHeader("Per-Layer Preset Switcher"))
		{
			ImGui::Indent(8.0f);
			ImGui::TextDisabled("Select different presets for Layer A and Layer B independently.");
			ImGui::Separator();

			auto presetTypesL = g_SkyCloudSystem.GetLayerAPresetTypes();
			std::vector<const char*> presetNamesL;
			presetNamesL.reserve(presetTypesL.size());
			for (auto t : presetTypesL)
				presetNamesL.push_back(SkyCloudSystem::PresetTypeToString(t));

			auto presetTypesB = g_SkyCloudSystem.GetLayerBPresetTypes();
			std::vector<const char*> presetNamesB;
			presetNamesB.reserve(presetTypesB.size());
			for (auto t : presetTypesB)
				presetNamesB.push_back(SkyCloudSystem::PresetTypeToString(t));

			if (!presetNamesL.empty() && !presetNamesB.empty())
			{
				// --- Layer A ---
				ImGui::TextUnformatted("Cloud Layer A");
				ImGui::Indent(8.0f);

				static int layerAPresetIdx = 0;
				if (layerAPresetIdx >= static_cast<int>(presetNamesL.size())) layerAPresetIdx = 0;
				ImGui::SetNextItemWidth(160.0f);
				ImGui::Combo("Preset##layerA", &layerAPresetIdx, presetNamesL.data(), static_cast<int>(presetNamesL.size()));

				static float layerADur = 30.0f;
				ImGui::SameLine();
				ImGui::SetNextItemWidth(80.0f);
				ImGui::DragFloat("s##layerADur", &layerADur, 1.0f, 1.0f, 300.0f, "%.0f s");

				WeatherPresetType typeA = presetTypesL[layerAPresetIdx];
				if (ImGui::Button("Apply Immediately##layerA"))
					g_SkyCloudSystem.SetLayerAPresetImmediate(typeA);
				ImGui::SameLine();
				if (ImGui::Button("Transition##layerA"))
					g_SkyCloudSystem.TransitionLayerAToPreset(typeA, layerADur);
				if (g_SkyCloudSystem.IsLayerATransitioning())
				{
					ImGui::SameLine();
					if (ImGui::Button("Interrupt##layerA"))
						g_SkyCloudSystem.InterruptLayerATransition();
					ImGui::SameLine();
					ImGui::ProgressBar(g_SkyCloudSystem.GetLayerATransitionProgress(), ImVec2(100.0f, 0.0f));
				}

				ImGui::Unindent(8.0f);
				ImGui::Separator();

				// --- Layer B ---
				ImGui::TextUnformatted("Cloud Layer B");
				ImGui::Indent(8.0f);

				static int layerBPresetIdx = 0;
				if (layerBPresetIdx >= static_cast<int>(presetNamesB.size())) layerBPresetIdx = 0;
				ImGui::SetNextItemWidth(160.0f);
				ImGui::Combo("Preset##layerB", &layerBPresetIdx, presetNamesB.data(), static_cast<int>(presetNamesB.size()));

				static float layerBDur = 30.0f;
				ImGui::SameLine();
				ImGui::SetNextItemWidth(80.0f);
				ImGui::DragFloat("s##layerBDur", &layerBDur, 1.0f, 1.0f, 300.0f, "%.0f s");

				WeatherPresetType typeB = presetTypesB[layerBPresetIdx];
				if (ImGui::Button("Apply Immediately##layerB"))
					g_SkyCloudSystem.SetLayerBPresetImmediate(typeB);
				ImGui::SameLine();
				if (ImGui::Button("Transition##layerB"))
					g_SkyCloudSystem.TransitionLayerBToPreset(typeB, layerBDur);
				ImGui::SameLine();
				{
					bool hasLayerBDwell = (info.LayerBDwellTarget >= 0.0f);
					ImGui::BeginDisabled(!hasLayerBDwell);
					if (ImGui::Button(info.LayerBDwellPaused ? "Resume Dwell##layerB" : "Pause Dwell##layerB"))
					{
						if (info.LayerBDwellPaused)
							g_SkyCloudSystem.ResumeLayerBDwell();
						else
							g_SkyCloudSystem.PauseLayerBDwell();
					}
					ImGui::EndDisabled();
				}
				if (g_SkyCloudSystem.IsLayerBTransitioning())
				{
					ImGui::SameLine();
					if (ImGui::Button("Interrupt##layerB"))
						g_SkyCloudSystem.InterruptLayerBTransition();
					ImGui::SameLine();
					ImGui::ProgressBar(g_SkyCloudSystem.GetLayerBTransitionProgress(), ImVec2(100.0f, 0.0f));
				}

				ImGui::Unindent(8.0f);
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
	// Horizon mesh section (Atmospheric Sky/Horizon tab)
	// ====================================================================

	static void DrawHorizonSection(Level* level)
	{
		if (!level)
			return;

		if (!ImGui::CollapsingHeader("Horizont-Mesh", ImGuiTreeNodeFlags_DefaultOpen))
			return;

		ImGui::Indent(8.0f);

		auto& atmoSettings = g_Renderer.GetAtmosphericSkySettings();

		ImGui::TextDisabled("Horizont 1");
		{
			bool enabled = level->Horizon1.GetEnabled();
			if (ImGui::Checkbox("Aktiv##h1", &enabled))
				level->Horizon1.SetEnabled(enabled);

			float alpha = level->Horizon1.GetTransparency();
			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 80.0f);
			if (ImGui::SliderFloat("Alpha##h1", &alpha, 0.0f, 1.0f, "%.2f"))
				level->Horizon1.SetTransparency(alpha);

			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 80.0f);
			ImGui::SliderFloat("Horizon Bottom Fade##h1", &atmoSettings.HorizonGradientRise[0], 0.0f, 1.0f, "%.3f");
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

			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 80.0f);
			ImGui::SliderFloat("Horizon Bottom Fade##h2", &atmoSettings.HorizonGradientRise[1], 0.0f, 1.0f, "%.3f");
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
		auto* levelPtr = dynamic_cast<Level*>(g_GameFlow->GetLevel(CurrentLevel));

		ImGui::Checkbox("Realistic Skydome (Atmospheric)", &settings.Enabled);
		ImGui::Separator();

		DrawHorizonSection(levelPtr);
		ImGui::Separator();

		if (!settings.Enabled)
		{
			ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Atmospheric sky dome is disabled.");
			return;
		}

		// --- Sun info (read-only) ---
		ImGui::Text("Sun Info (from Lens Flare):");
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

			// Cloud occlusion indicator — green = disc clear, red = disc behind screen-blend cloud
			float transmittance = g_SkyCloudSystem.GetCombinedCloudTransmittance();
			bool  discOccluded  = (transmittance < 0.5f);
			ImVec4 indicatorColor = discOccluded
				? ImVec4(1.0f, 0.15f, 0.15f, 1.0f)
				: ImVec4(0.15f, 1.0f, 0.15f, 1.0f);
			ImGui::TextColored(indicatorColor, discOccluded ? "  [DISC OCCLUDED]" : "  [DISC CLEAR]");
			ImGui::SameLine();
			ImGui::TextDisabled("cloud transmittance: %.2f", transmittance);
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

		// --- Sunset underside lighting ---
		if (ImGui::CollapsingHeader("Sunset Underside Lighting", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::SliderFloat("Sunset Underside Intensity",   &settings.SunsetUndersideIntensity,  0.0f, 3.0f,  "%.3f");
			ImGui::SliderFloat("Sunset Underside Spread",      &settings.SunsetUndersideSpread,     0.5f, 4.0f,  "%.2f");
			ImGui::SliderFloat("Sunset Underside Height Fade", &settings.SunsetUndersideHeightFade, 0.5f, 4.0f,  "%.2f");
		}

		// --- Horizon ground color ---
		if (ImGui::CollapsingHeader("Black Void Color", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::TextDisabled("Color of the lower horizon void (level.dynamicSky.blackvoidcolor).");
			float horizonCol[3] = { settings.HorizonColorR, settings.HorizonColorG, settings.HorizonColorB };
			if (ImGui::ColorEdit3("Black Void Color", horizonCol))
			{
				settings.HorizonColorR = horizonCol[0];
				settings.HorizonColorG = horizonCol[1];
				settings.HorizonColorB = horizonCol[2];
			}
			ImGui::SliderFloat("Black Void R", &settings.HorizonColorR, 0.0f, 1.0f, "%.3f");
			ImGui::SliderFloat("Black Void G", &settings.HorizonColorG, 0.0f, 1.0f, "%.3f");
			ImGui::SliderFloat("Black Void B", &settings.HorizonColorB, 0.0f, 1.0f, "%.3f");
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

			// Cloud coverage proxy: use Coverage (overall cloud layer transparency [0,1]).
			float coverage = 0.0f;
			if (g_SkyCloudSystem.IsCloudAActive() || g_SkyCloudSystem.IsCloudBActive())
			{
				float covA = g_SkyCloudSystem.IsCloudAActive()
					? g_SkyCloudSystem.GetCloudARenderSettings().Coverage : 0.0f;
				float covB = g_SkyCloudSystem.IsCloudBActive()
					? g_SkyCloudSystem.GetCloudBRenderSettings().Coverage : 0.0f;
				coverage = std::max(covA, covB);
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
	// Wind Direction Widget — circular control similar to the moon widget.
	// X axis = East/West (right = East, left = West).
	// Y axis = North/South (up = North, down = South).
	// Distance from center = wind strength [0..1].
	// ====================================================================

	static constexpr float WIND_WIDGET_SIZE  = 220.0f;
	static constexpr float WIND_POINT_RADIUS = 7.0f;
	static constexpr float WIND_HIT_RADIUS   = 12.0f;

	// Returns true while dragging or when the value changed this frame.
	static bool DrawWindWidget(float& outNormX, float& outNormZ)
	{
		bool changed = false;
		float halfSize = WIND_WIDGET_SIZE * 0.5f;
		float radius   = halfSize - 6.0f;

		ImVec2 canvasPos = ImGui::GetCursorScreenPos();
		ImVec2 center    = ImVec2(canvasPos.x + halfSize, canvasPos.y + halfSize);

		ImGui::InvisibleButton("##windWidget", ImVec2(WIND_WIDGET_SIZE, WIND_WIDGET_SIZE));
		ImDrawList* drawList = ImGui::GetWindowDrawList();

		// Background.
		drawList->AddRectFilled(canvasPos,
			ImVec2(canvasPos.x + WIND_WIDGET_SIZE, canvasPos.y + WIND_WIDGET_SIZE),
			IM_COL32(20, 25, 35, 255));
		drawList->AddRect(canvasPos,
			ImVec2(canvasPos.x + WIND_WIDGET_SIZE, canvasPos.y + WIND_WIDGET_SIZE),
			IM_COL32(70, 80, 110, 255));

		// Concentric strength rings (25 / 50 / 75 / 100 %).
		drawList->AddCircle(center, radius * 0.25f, IM_COL32(60, 70, 90, 180), 64, 1.0f);
		drawList->AddCircle(center, radius * 0.50f, IM_COL32(60, 70, 90, 180), 64, 1.0f);
		drawList->AddCircle(center, radius * 0.75f, IM_COL32(60, 70, 90, 180), 64, 1.0f);
		drawList->AddCircle(center, radius,         IM_COL32(80, 100, 140, 220), 64, 1.5f);

		// Crosshairs.
		drawList->AddLine(ImVec2(center.x - radius, center.y), ImVec2(center.x + radius, center.y), IM_COL32(50, 55, 75, 160));
		drawList->AddLine(ImVec2(center.x, center.y - radius), ImVec2(center.x, center.y + radius), IM_COL32(50, 55, 75, 160));

		// Compass labels.
		drawList->AddText(ImVec2(center.x - 5, canvasPos.y + 2),                                IM_COL32(180, 190, 220, 220), "N");
		drawList->AddText(ImVec2(center.x - 5, canvasPos.y + WIND_WIDGET_SIZE - 16),            IM_COL32(180, 190, 220, 220), "S");
		drawList->AddText(ImVec2(canvasPos.x + 4, center.y - 8),                                 IM_COL32(180, 190, 220, 220), "W");
		drawList->AddText(ImVec2(canvasPos.x + WIND_WIDGET_SIZE - 12, center.y - 8),             IM_COL32(180, 190, 220, 220), "E");
		drawList->AddCircle(center, 3.0f, IM_COL32(120, 130, 160, 200), 16, 1.0f);

		// Map current normalized vector (range [-1..1]) to screen.
		// Widget convention: up == -Z (North), right == +X (East).
		ImVec2 screenPoint = ImVec2(
			center.x + outNormX * radius,
			center.y - outNormZ * radius);

		// Interaction.
		static bool windDragging = false;
		ImVec2 mousePos = ImGui::GetIO().MousePos;
		bool   mouseDown = ImGui::GetIO().MouseDown[0];

		float distToPoint = std::sqrt(
			(mousePos.x - screenPoint.x) * (mousePos.x - screenPoint.x) +
			(mousePos.y - screenPoint.y) * (mousePos.y - screenPoint.y));
		float distToCenter = std::sqrt(
			(mousePos.x - center.x) * (mousePos.x - center.x) +
			(mousePos.y - center.y) * (mousePos.y - center.y));

		if (ImGui::IsItemActive() && mouseDown)
		{
			if (!windDragging && (distToPoint < WIND_HIT_RADIUS || distToCenter <= radius))
				windDragging = true;
		}
		else
		{
			windDragging = false;
		}

		if (windDragging)
		{
			float nx = (mousePos.x - center.x) / radius;
			float nz = -(mousePos.y - center.y) / radius;

			float len = std::sqrt(nx * nx + nz * nz);
			if (len > 1.0f)
			{
				nx /= len;
				nz /= len;
			}

			outNormX = nx;
			outNormZ = nz;
			changed = true;

			screenPoint = ImVec2(
				center.x + outNormX * radius,
				center.y - outNormZ * radius);
		}

		// Wind point glow.
		drawList->AddCircleFilled(screenPoint, WIND_POINT_RADIUS + 3.0f, IM_COL32(180, 220, 255, 60));
		drawList->AddCircleFilled(screenPoint, WIND_POINT_RADIUS,        IM_COL32(220, 235, 255, 255));
		drawList->AddCircle(screenPoint, WIND_POINT_RADIUS,              IM_COL32(255, 255, 255, 255), 0, 1.5f);
		if (windDragging)
			drawList->AddCircle(screenPoint, WIND_POINT_RADIUS + 5.0f, IM_COL32(180, 220, 255, 180), 0, 2.0f);

		return changed;
	}

	// ====================================================================
	// Wind debug content
	// ====================================================================

	static void DrawWindTabContent()
	{
		using namespace TEN::Effects::Environment;

		ImGui::Separator();
		ImGui::Spacing();

		ImGui::TextWrapped(
			"Drag the point inside the circle to set the steady wind direction "
			"and strength. Up = North, Right = East. The center is calm (no wind).");
		ImGui::Spacing();

		// Pull current base wind and convert to normalized [-1..1] coordinates.
		auto currentWind = Weather.BaseWind();
		const float MAX_WIND = EnvironmentController::MAX_BASE_WIND_STRENGTH;
		float normX = std::clamp(currentWind.x / MAX_WIND, -1.0f, 1.0f);
		float normZ = std::clamp(currentWind.z / MAX_WIND, -1.0f, 1.0f);

		// Layout: widget on the left, info panel on the right.
		ImGui::BeginGroup();
		bool changed = DrawWindWidget(normX, normZ);
		ImGui::EndGroup();

		ImGui::SameLine();
		ImGui::BeginGroup();

		float magnitude = std::sqrt(normX * normX + normZ * normZ);
		float strengthPct = magnitude * 100.0f;

		// Compass bearing (0 deg = North, 90 deg = East).
		float bearing = 0.0f;
		const char* compass = "Calm";
		if (magnitude > 0.001f)
		{
			bearing = std::atan2(normX, -normZ) * (180.0f / 3.14159265f);
			if (bearing < 0.0f)
				bearing += 360.0f;

			static const char* DIRS[] = {
				"N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE",
				"S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"
			};
			int idx = (int)((bearing + 11.25f) / 22.5f) & 0xF;
			compass = DIRS[idx];
		}

		ImGui::Text("Direction: %s", compass);
		ImGui::Text("Bearing:   %.1f deg", bearing);
		ImGui::Text("Strength:  %.1f %%", strengthPct);
		ImGui::Spacing();
		ImGui::Text("Vector X:  %+.3f", normX * MAX_WIND);
		ImGui::Text("Vector Z:  %+.3f", normZ * MAX_WIND);

		ImGui::Spacing();
		if (ImGui::Button("Calm (no wind)"))
		{
			normX   = 0.0f;
			normZ   = 0.0f;
			changed = true;
		}

		ImGui::EndGroup();

		if (changed)
			Weather.SetBaseWind(normX * MAX_WIND, normZ * MAX_WIND);

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::TextWrapped(
			"This wind drives Lara's ponytail, particles, and the AltocumulusMid "
			"cloud direction. Particles and hair add a randomised gust on top,"
			"but the clouds always move with the steady value from the LUA entry:"
			"level.dynamicSky.Clouds.windSpeed = 0.0 - 8.0");

		// ============================================================
		// Volumetric Dust Storm controls.
		// ============================================================
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		auto& dust = g_Renderer.GetDustStormSettings();

		if (ImGui::CollapsingHeader("Volumetric Dust Storm", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Checkbox("Enable Dust Storm", &dust.Enabled);

			ImGui::BeginDisabled(!dust.Enabled);
			ImGui::SliderFloat("Dust Density", &dust.Density, 0.0f, 2.0f, "%.2f");

			ImGui::SliderFloat("Min Height", &dust.MinHeight, 0.0f, 1.0f, "%.2f");
			ImGui::SliderFloat("Max Height", &dust.MaxHeight, 0.0f, 1.0f, "%.2f");
			if (dust.MaxHeight < dust.MinHeight)
				dust.MaxHeight = dust.MinHeight;

			float color[3] = { dust.ColorR, dust.ColorG, dust.ColorB };
			if (ImGui::ColorEdit3("Dust Color", color))
			{
				dust.ColorR = color[0];
				dust.ColorG = color[1];
				dust.ColorB = color[2];
			}

			ImGui::Spacing();
			ImGui::TextDisabled("Tuning");
			ImGui::SliderFloat("Wind Coupling", &dust.WindSpeedScale, 0.0f, 4.0f, "%.2f");
			ImGui::SliderFloat("Turbulence",    &dust.Turbulence,     0.0f, 2.0f, "%.2f");
			ImGui::SliderInt  ("Steps",         &dust.StepCount,      3,    12);
			ImGui::EndDisabled();

			ImGui::TextWrapped(
				"The dust storm is a volumetric raymarched effect rendered after "
				"the scene. It is only visible from outdoor rooms and follows the "
				"steady wind set above.");
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
			if (ImGui::BeginTabItem("Clouds"))
			{
				DrawCloudTabContent();
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Sun/Moon/Stars"))
			{
				TEN::Effects::DrawLensFlareTabContent();
				DrawMoonTabContent();
				auto* levelPtr = dynamic_cast<Level*>(
					g_GameFlow->GetLevel(CurrentLevel));
				DrawStarfieldSection(levelPtr);
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Atmospheric Sky/Horizon"))
			{
				DrawAtmosphericSkyTabContent();
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("God Rays"))
			{
				DrawGodRayTabContent();
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Wind"))
			{
				DrawWindTabContent();
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
		ss << "Transition: " << (info.TransitionProgress * 100.0f) << "%\n\n";

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
