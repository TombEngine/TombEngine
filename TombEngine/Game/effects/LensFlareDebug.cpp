// ============================================================================
// LensFlareDebug.cpp — ImGui debug overlay for lens flare / sun control.
//
// Architecture:
//   1. A square widget with an inscribed circle represents the sky dome.
//   2. The draggable point inside the circle maps to pitch (vertical)
//      and yaw (horizontal) of the existing lens flare sun.
//   3. Color mode and colors are editable in real time.
//   4. All changes mutate the live lens flare data directly.
// ============================================================================

#include "framework.h"
#include "Game/Effects/LensFlareDebug.h"
#include "Renderer/ImGuiIntegration.h"

#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <filesystem>

#include "Game/control/control.h"
#include "Renderer/Renderer.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Scripting/Internal/TEN/Flow/Level/FlowLevel.h"
#include "Scripting/Internal/TEN/Flow/LensFlare/LensFlare.h"
#include "Specific/level.h"

using namespace TEN::Scripting;

namespace TEN::Effects
{
	// ====================================================================
	// Constants
	// ====================================================================

	static constexpr float WIDGET_SIZE       = 220.0f; // Square widget side length (pixels).
	static constexpr float POINT_RADIUS      = 7.0f;   // Draggable sun point radius.
	static constexpr float HIT_RADIUS        = 12.0f;  // Larger hit-test radius for easy grabbing.

	// Pitch mapping: center of circle = 90 degrees (zenith), edge = 0 (horizon).
	// Below-horizon pitch allowed so the sun can dip below the horizon darkening line.
	static constexpr float PITCH_MIN = -10.0f;
	static constexpr float PITCH_MAX = 90.0f;

	// Maximum widget radius: r=1 = horizon (pitch 0), slightly beyond = below horizon.
	static constexpr float MAX_WIDGET_RADIUS = 1.0f + (-PITCH_MIN / PITCH_MAX);

	// Yaw mapping: left = 0, right = 360.
	static constexpr float YAW_MIN = 0.0f;
	static constexpr float YAW_MAX = 360.0f;

	// ====================================================================
	// Helpers
	// ====================================================================

	static const char* ColorModeToString(LensFlareColorMode mode)
	{
		switch (mode)
		{
		case LensFlareColorMode::AutoRealistic:    return "Auto Realistic";
		case LensFlareColorMode::SingleColor:      return "Single Color";
		case LensFlareColorMode::GradientTwoColor: return "Two-Color Gradient";
		default:                                   return "Unknown";
		}
	}

	// Convert pitch/yaw to a 2D point in normalized circle space [-1, 1].
	// Polar sky-dome projection: center = zenith (pitch=90), edge = horizon (pitch=0).
	// Yaw=0 points up (north), clockwise.
	static ImVec2 PitchYawToCirclePoint(float pitch, float yaw)
	{
		// Radius from center: 0 at zenith (pitch=90), 1 at horizon (pitch=0),
		// >1 for below-horizon (negative pitch).
		float r = std::clamp(1.0f - pitch / PITCH_MAX, 0.0f, MAX_WIDGET_RADIUS);

		// Angle: yaw=0 points up (-Y), clockwise.
		float yawRad = yaw * (3.14159265f / 180.0f);
		float cx =  r * std::sin(yawRad);
		float cy = -r * std::cos(yawRad);

		return ImVec2(cx, cy);
	}

	// Convert a normalized circle point [-1, 1] back to pitch/yaw.
	// Inverse polar sky-dome projection: distance from center -> pitch, angle -> yaw.
	static void CirclePointToPitchYaw(const ImVec2& point, float& outPitch, float& outYaw)
	{
		// Distance from center: 0 = zenith (pitch=90), 1 = horizon (pitch=0),
		// >1 = below horizon (negative pitch).
		float r = std::sqrt(point.x * point.x + point.y * point.y);
		r = std::clamp(r, 0.0f, MAX_WIDGET_RADIUS);
		outPitch = std::clamp(PITCH_MAX * (1.0f - r), PITCH_MIN, PITCH_MAX);

		// Angle: atan2(x, -y) gives yaw=0 pointing up, clockwise.
		float yawRad = std::atan2(point.x, -point.y);
		outYaw = yawRad * (180.0f / 3.14159265f);
		outYaw = std::fmod(outYaw + 360.0f, 360.0f);
	}

	// Clamp a 2D point to the maximum widget radius (allows slightly beyond
	// the unit circle for below-horizon sun positions).
	static ImVec2 ClampToWidgetCircle(const ImVec2& p)
	{
		float len = std::sqrt(p.x * p.x + p.y * p.y);
		if (len <= MAX_WIDGET_RADIUS)
			return p;
		return ImVec2(p.x / len * MAX_WIDGET_RADIUS, p.y / len * MAX_WIDGET_RADIUS);
	}

	// ====================================================================
	// Sun position widget
	// ====================================================================

	// Returns true if the pitch/yaw were changed.
	static bool DrawSunPositionWidget(float& pitch, float& yaw)
	{
		bool changed = false;

		float halfSize = WIDGET_SIZE * 0.5f;
		// Horizon circle radius; shrunk so the below-horizon ring still fits inside the widget.
		float radius = (halfSize - 2.0f) / MAX_WIDGET_RADIUS;

		ImVec2 canvasPos = ImGui::GetCursorScreenPos();
		ImVec2 center = ImVec2(canvasPos.x + halfSize, canvasPos.y + halfSize);

		// Reserve space for the widget.
		ImGui::InvisibleButton("##sunWidget", ImVec2(WIDGET_SIZE, WIDGET_SIZE));

		ImDrawList* drawList = ImGui::GetWindowDrawList();

		// Draw square background.
		drawList->AddRectFilled(canvasPos,
			ImVec2(canvasPos.x + WIDGET_SIZE, canvasPos.y + WIDGET_SIZE),
			IM_COL32(30, 30, 40, 255));
		drawList->AddRect(canvasPos,
			ImVec2(canvasPos.x + WIDGET_SIZE, canvasPos.y + WIDGET_SIZE),
			IM_COL32(80, 80, 100, 255));

		// Draw inscribed circle (horizon line).
		drawList->AddCircle(center, radius, IM_COL32(70, 90, 120, 255), 64, 1.5f);

		// Draw faint outer ring showing below-horizon limit.
		drawList->AddCircle(center, radius * MAX_WIDGET_RADIUS, IM_COL32(120, 50, 50, 100), 64, 1.0f);

		// Draw crosshair lines.
		drawList->AddLine(ImVec2(center.x - radius, center.y),
			ImVec2(center.x + radius, center.y),
			IM_COL32(50, 50, 70, 128));
		drawList->AddLine(ImVec2(center.x, center.y - radius),
			ImVec2(center.x, center.y + radius),
			IM_COL32(50, 50, 70, 128));

		// Draw labels: center = zenith, edge = horizon, beyond = below horizon.
		drawList->AddText(ImVec2(canvasPos.x + 4, canvasPos.y + WIDGET_SIZE - 16),
			IM_COL32(120, 120, 140, 200), "Horizon");
		// Draw a small ring at the center to mark the zenith.
		drawList->AddCircle(center, 3.0f, IM_COL32(100, 130, 170, 180), 16, 1.0f);
		drawList->AddText(ImVec2(center.x + 6, center.y - 8),
			IM_COL32(120, 120, 140, 200), "Zenith");

		// Convert current pitch/yaw to circle point.
		ImVec2 normalizedPoint = PitchYawToCirclePoint(pitch, yaw);
		ImVec2 screenPoint = ImVec2(
			center.x + normalizedPoint.x * radius,
			center.y + normalizedPoint.y * radius);

		// ---- Interaction: drag the sun point ----
		static bool isDragging = false;
		ImVec2 mousePos = ImGui::GetIO().MousePos;
		bool mouseDown = ImGui::GetIO().MouseDown[0];

		// Hit test: check if mouse is near the sun point or inside the circle.
		float distToPoint = std::sqrt(
			(mousePos.x - screenPoint.x) * (mousePos.x - screenPoint.x) +
			(mousePos.y - screenPoint.y) * (mousePos.y - screenPoint.y));

		float distToCenter = std::sqrt(
			(mousePos.x - center.x) * (mousePos.x - center.x) +
			(mousePos.y - center.y) * (mousePos.y - center.y));

		if (ImGui::IsItemActive() && mouseDown)
		{
			// Start dragging if near the point, or allow click-to-place inside widget area
			// (including the below-horizon ring).
			if (!isDragging && (distToPoint < HIT_RADIUS || distToCenter <= radius * MAX_WIDGET_RADIUS))
				isDragging = true;
		}
		else
		{
			isDragging = false;
		}

		if (isDragging)
		{
			// Convert mouse position to normalized circle space.
			ImVec2 newNormalized = ImVec2(
				(mousePos.x - center.x) / radius,
				(mousePos.y - center.y) / radius);

			// Clamp to widget circle (allows below-horizon positions).
			newNormalized = ClampToWidgetCircle(newNormalized);

			// Convert back to pitch/yaw.
			CirclePointToPitchYaw(newNormalized, pitch, yaw);
			changed = true;

			// Update screen point for immediate visual feedback.
			screenPoint = ImVec2(
				center.x + newNormalized.x * radius,
				center.y + newNormalized.y * radius);
		}

		// Draw sun point with glow.
		drawList->AddCircleFilled(screenPoint, POINT_RADIUS + 3.0f, IM_COL32(255, 200, 50, 60));
		drawList->AddCircleFilled(screenPoint, POINT_RADIUS, IM_COL32(255, 220, 80, 255));
		drawList->AddCircle(screenPoint, POINT_RADIUS, IM_COL32(255, 255, 200, 255), 0, 1.5f);

		// Draw a small indicator when dragging.
		if (isDragging)
			drawList->AddCircle(screenPoint, POINT_RADIUS + 5.0f, IM_COL32(255, 255, 100, 180), 0, 2.0f);

		return changed;
	}

	// ====================================================================
	// Color swatch preview
	// ====================================================================

	static void DrawColorSwatch(const char* label, const Color& color, float width = 60.0f)
	{
		ImVec2 pos = ImGui::GetCursorScreenPos();
		ImDrawList* drawList = ImGui::GetWindowDrawList();

		ImU32 col = IM_COL32(
			static_cast<int>(std::clamp(color.x, 0.0f, 1.0f) * 255.0f),
			static_cast<int>(std::clamp(color.y, 0.0f, 1.0f) * 255.0f),
			static_cast<int>(std::clamp(color.z, 0.0f, 1.0f) * 255.0f),
			255);

		float height = ImGui::GetTextLineHeight() + 2.0f;
		drawList->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height), col);
		drawList->AddRect(pos, ImVec2(pos.x + width, pos.y + height), IM_COL32(128, 128, 128, 200));

		ImGui::Dummy(ImVec2(width, height));
		ImGui::SameLine();
		ImGui::Text("%s (%.0f, %.0f, %.0f)", label,
			color.x * 255.0f, color.y * 255.0f, color.z * 255.0f);
	}

	// ====================================================================
	// Main overlay
	// ====================================================================

	static void DrawSunPanelContent()
	{
		auto* level = dynamic_cast<Level*>(g_GameFlow->GetLevel(CurrentLevel));
		if (!level)
			return;

		auto& lensFlare = level->GetMutableLensFlare();

		// ----------------------------------------------------------------
		// Sun State section
		// ----------------------------------------------------------------
		if (ImGui::CollapsingHeader("Sun State", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Indent(8.0f);

			bool enabled = lensFlare.GetEnabled();
			if (ImGui::Checkbox("Enabled", &enabled))
				lensFlare.SetEnabled(enabled);

			float pitch = lensFlare.GetPitch();
			float yaw   = lensFlare.GetYaw();

			ImGui::Text("Pitch: %.1f deg", pitch);
			ImGui::Text("Yaw:   %.1f deg", yaw);
			ImGui::Text("Elevation: %.2f", lensFlare.GetNormalizedElevation());

			// Direction vector preview.
			float pitchRad = pitch * 3.14159265f / 180.0f;
			float yawRad   = yaw * 3.14159265f / 180.0f;
			float dx = std::cos(pitchRad) * std::sin(yawRad);
			float dy = std::sin(pitchRad);
			float dz = std::cos(pitchRad) * std::cos(yawRad);
			ImGui::Text("Direction: (%.3f, %.3f, %.3f)", dx, dy, dz);

			ImGui::Text("Color Mode: %s", ColorModeToString(lensFlare.GetColorMode()));

			// Show evaluated color.
			Color evalColor = lensFlare.EvaluateColor();
			DrawColorSwatch("Effective Color", evalColor);

			ImGui::Unindent(8.0f);
		}

		// ----------------------------------------------------------------
		// Interactive Sun Position Widget
		// ----------------------------------------------------------------
		if (ImGui::CollapsingHeader("Sun Position Control", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Indent(8.0f);

			float pitch = lensFlare.GetPitch();
			float yaw   = lensFlare.GetYaw();

			ImGui::Text("Drag the sun point inside the circle:");
			if (DrawSunPositionWidget(pitch, yaw))
			{
				lensFlare.SetPitch(pitch);
				lensFlare.SetYaw(yaw);
			}

			if (ImGui::Button("Copy Lua to clipboard##sun"))
			{
				auto* level = dynamic_cast<Level*>(g_GameFlow->GetLevel(CurrentLevel));
				std::string levelName = "level";
				if (level && !level->FileName.empty())
				{
					auto stem = std::filesystem::path(level->FileName).stem().string();
					if (!stem.empty())
						levelName = stem;
				}

				LensFlareColorMode mode = lensFlare.GetColorMode();
				char buf[256];
				if (mode == LensFlareColorMode::AutoRealistic)
				{
					snprintf(buf, sizeof(buf),
						"%s.lensFlare = Flow.LensFlare(%.2f, %.2f)",
						levelName.c_str(), pitch, yaw);
				}
				else if (mode == LensFlareColorMode::SingleColor)
				{
					ScriptColor c = lensFlare.GetColor();
					snprintf(buf, sizeof(buf),
						"%s.lensFlare = Flow.LensFlare(%.2f, %.2f, Color(%d, %d, %d))",
						levelName.c_str(), pitch, yaw, (int)c.GetR(), (int)c.GetG(), (int)c.GetB());
				}
				else
				{
					ScriptColor a = lensFlare.GetColor();
					ScriptColor b = lensFlare.GetColorB();
					snprintf(buf, sizeof(buf),
						"%s.lensFlare = Flow.LensFlare(%.2f, %.2f, Color(%d, %d, %d), Color(%d, %d, %d))",
						levelName.c_str(), pitch, yaw,
						(int)a.GetR(), (int)a.GetG(), (int)a.GetB(),
						(int)b.GetR(), (int)b.GetG(), (int)b.GetB());
				}
				ImGui::SetClipboardText(buf);
			}

			// Also provide manual numeric input.
			ImGui::Spacing();
			bool manualChanged = false;
			if (ImGui::SliderFloat("Pitch##manual", &pitch, PITCH_MIN, PITCH_MAX, "%.1f deg"))
			{
				lensFlare.SetPitch(pitch);
				manualChanged = true;
			}
			if (ImGui::SliderFloat("Yaw##manual", &yaw, YAW_MIN, YAW_MAX, "%.1f deg"))
			{
				lensFlare.SetYaw(yaw);
				manualChanged = true;
			}

			ImGui::Unindent(8.0f);
		}

		// ----------------------------------------------------------------
		// Color Mode & Color Editing
		// ----------------------------------------------------------------
		if (ImGui::CollapsingHeader("Sun Color", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Indent(8.0f);

			auto& atmoSettings = g_Renderer.GetAtmosphericSkySettings();

			// Color mode combo.
			LensFlareColorMode mode = lensFlare.GetColorMode();
			int modeInt = static_cast<int>(mode);
			const char* modeNames[] = { "Auto Realistic", "Single Color", "Two-Color Gradient" };
			if (ImGui::Combo("Color Mode", &modeInt, modeNames, 3))
			{
				lensFlare.SetColorMode(static_cast<LensFlareColorMode>(modeInt));
				mode = static_cast<LensFlareColorMode>(modeInt);
			}

			ImGui::Separator();

			if (mode == LensFlareColorMode::AutoRealistic)
			{
				ImGui::TextWrapped("Sun color is computed automatically from elevation.");
				ImGui::Spacing();

				// Show a gradient preview strip.
				ImGui::Text("Elevation Color Ramp:");
				ImVec2 rampPos = ImGui::GetCursorScreenPos();
				float rampWidth = ImGui::GetContentRegionAvail().x;
				float rampHeight = 20.0f;
				ImDrawList* drawList = ImGui::GetWindowDrawList();

				constexpr int RAMP_STEPS = 64;
				float stepWidth = rampWidth / RAMP_STEPS;
				for (int i = 0; i < RAMP_STEPS; i++)
				{
					float t = static_cast<float>(i) / (RAMP_STEPS - 1);
					Color rawC = ComputeRealisticSunColor(t);
					float sunInfl = std::max(0.0f, 1.0f - t * atmoSettings.SunElevationRampSpeed);
					float blend   = sunInfl * atmoSettings.SunWarmInfluence;
					Color c;
					c.x = std::clamp(1.0f + (rawC.x - 1.0f) * blend, 0.0f, 1.0f);
					c.y = std::clamp(1.0f + (rawC.y - 1.0f) * blend, 0.0f, 1.0f);
					c.z = std::clamp(1.0f + (rawC.z - 1.0f) * blend, 0.0f, 1.0f);
					ImU32 col = IM_COL32(
						static_cast<int>(c.x * 255.0f),
						static_cast<int>(c.y * 255.0f),
						static_cast<int>(c.z * 255.0f), 255);

					drawList->AddRectFilled(
						ImVec2(rampPos.x + i * stepWidth, rampPos.y),
						ImVec2(rampPos.x + (i + 1) * stepWidth, rampPos.y + rampHeight),
						col);
				}
				drawList->AddRect(rampPos, ImVec2(rampPos.x + rampWidth, rampPos.y + rampHeight),
					IM_COL32(128, 128, 128, 200));
				ImGui::Dummy(ImVec2(rampWidth, rampHeight));

				// Show indicator of current position on the ramp.
				float currentElev = lensFlare.GetNormalizedElevation();
				float indicatorX = rampPos.x + currentElev * rampWidth;
				drawList->AddTriangleFilled(
					ImVec2(indicatorX, rampPos.y + rampHeight + 2),
					ImVec2(indicatorX - 4, rampPos.y + rampHeight + 8),
					ImVec2(indicatorX + 4, rampPos.y + rampHeight + 8),
					IM_COL32(255, 255, 255, 220));
				ImGui::Dummy(ImVec2(rampWidth, 10.0f));

				// Show computed color (with sky gradient applied at current elevation).
				Color evalColor = lensFlare.EvaluateColor();
				float skyBlend = std::max(0.0f, 1.0f - currentElev * atmoSettings.SunElevationRampSpeed) * atmoSettings.SunWarmInfluence;
				Color skyColor;
				skyColor.x = std::clamp(1.0f + (evalColor.x - 1.0f) * skyBlend, 0.0f, 1.0f);
				skyColor.y = std::clamp(1.0f + (evalColor.y - 1.0f) * skyBlend, 0.0f, 1.0f);
				skyColor.z = std::clamp(1.0f + (evalColor.z - 1.0f) * skyBlend, 0.0f, 1.0f);
				DrawColorSwatch("Current", skyColor);
			}
			else if (mode == LensFlareColorMode::SingleColor)
			{
				ImGui::Text("Single fixed color:");

				ScriptColor sc = lensFlare.GetColor();
				float col[3] = {
					sc.GetR() / 255.0f,
					sc.GetG() / 255.0f,
					sc.GetB() / 255.0f
				};
				if (ImGui::ColorEdit3("Color A##single", col))
				{
					lensFlare.SetColor(ScriptColor(
						static_cast<byte>(col[0] * 255.0f),
						static_cast<byte>(col[1] * 255.0f),
						static_cast<byte>(col[2] * 255.0f)));
				}

				Color evalColor = lensFlare.EvaluateColor();
				float curElev = lensFlare.GetNormalizedElevation();
				float skyBlend = std::max(0.0f, 1.0f - curElev * atmoSettings.SunElevationRampSpeed) * atmoSettings.SunWarmInfluence;
				Color skyColor;
				skyColor.x = std::clamp(1.0f + (evalColor.x - 1.0f) * skyBlend, 0.0f, 1.0f);
				skyColor.y = std::clamp(1.0f + (evalColor.y - 1.0f) * skyBlend, 0.0f, 1.0f);
				skyColor.z = std::clamp(1.0f + (evalColor.z - 1.0f) * skyBlend, 0.0f, 1.0f);
				DrawColorSwatch("Effective", skyColor);
			}
			else if (mode == LensFlareColorMode::GradientTwoColor)
			{
				ImGui::Text("Two-color gradient (horizon to zenith):");

				// Color A (horizon).
				ScriptColor scA = lensFlare.GetColor();
				float colA[3] = {
					scA.GetR() / 255.0f,
					scA.GetG() / 255.0f,
					scA.GetB() / 255.0f
				};
				if (ImGui::ColorEdit3("Color A (Horizon)", colA))
				{
					lensFlare.SetColor(ScriptColor(
						static_cast<byte>(colA[0] * 255.0f),
						static_cast<byte>(colA[1] * 255.0f),
						static_cast<byte>(colA[2] * 255.0f)));
				}

				// Color B (zenith).
				ScriptColor scB = lensFlare.GetColorB();
				float colB[3] = {
					scB.GetR() / 255.0f,
					scB.GetG() / 255.0f,
					scB.GetB() / 255.0f
				};
				if (ImGui::ColorEdit3("Color B (Zenith)", colB))
				{
					lensFlare.SetColorB(ScriptColor(
						static_cast<byte>(colB[0] * 255.0f),
						static_cast<byte>(colB[1] * 255.0f),
						static_cast<byte>(colB[2] * 255.0f)));
				}

				// Show gradient preview strip.
				ImGui::Text("Gradient Preview:");
				ImVec2 rampPos = ImGui::GetCursorScreenPos();
				float rampWidth = ImGui::GetContentRegionAvail().x;
				float rampHeight = 20.0f;
				ImDrawList* drawList = ImGui::GetWindowDrawList();

				constexpr int RAMP_STEPS = 64;
				float stepWidth = rampWidth / RAMP_STEPS;
				for (int i = 0; i < RAMP_STEPS; i++)
				{
					float t = static_cast<float>(i) / (RAMP_STEPS - 1);
					float st = t * t * (3.0f - 2.0f * t); // smoothstep matching EvaluateColor
					float rawR = colA[0] + (colB[0] - colA[0]) * st;
					float rawG = colA[1] + (colB[1] - colA[1]) * st;
					float rawB = colA[2] + (colB[2] - colA[2]) * st;
					float sunInfl = std::max(0.0f, 1.0f - t * atmoSettings.SunElevationRampSpeed);
					float blend   = sunInfl * atmoSettings.SunWarmInfluence;
					float r = std::clamp(1.0f + (rawR - 1.0f) * blend, 0.0f, 1.0f);
					float g = std::clamp(1.0f + (rawG - 1.0f) * blend, 0.0f, 1.0f);
					float b = std::clamp(1.0f + (rawB - 1.0f) * blend, 0.0f, 1.0f);

					ImU32 col = IM_COL32(
						static_cast<int>(r * 255.0f),
						static_cast<int>(g * 255.0f),
						static_cast<int>(b * 255.0f), 255);

					drawList->AddRectFilled(
						ImVec2(rampPos.x + i * stepWidth, rampPos.y),
						ImVec2(rampPos.x + (i + 1) * stepWidth, rampPos.y + rampHeight),
						col);
				}
				drawList->AddRect(rampPos, ImVec2(rampPos.x + rampWidth, rampPos.y + rampHeight),
					IM_COL32(128, 128, 128, 200));
				ImGui::Dummy(ImVec2(rampWidth, rampHeight));

				// Elevation indicator.
				float currentElev = lensFlare.GetNormalizedElevation();
				float indicatorX = rampPos.x + currentElev * rampWidth;
				drawList->AddTriangleFilled(
					ImVec2(indicatorX, rampPos.y + rampHeight + 2),
					ImVec2(indicatorX - 4, rampPos.y + rampHeight + 8),
					ImVec2(indicatorX + 4, rampPos.y + rampHeight + 8),
					IM_COL32(255, 255, 255, 220));
				ImGui::Dummy(ImVec2(rampWidth, 10.0f));

				Color evalColor = lensFlare.EvaluateColor();
				float skyBlend = std::max(0.0f, 1.0f - currentElev * atmoSettings.SunElevationRampSpeed) * atmoSettings.SunWarmInfluence;
				Color skyColor;
				skyColor.x = std::clamp(1.0f + (evalColor.x - 1.0f) * skyBlend, 0.0f, 1.0f);
				skyColor.y = std::clamp(1.0f + (evalColor.y - 1.0f) * skyBlend, 0.0f, 1.0f);
				skyColor.z = std::clamp(1.0f + (evalColor.z - 1.0f) * skyBlend, 0.0f, 1.0f);
				DrawColorSwatch("Effective", skyColor);
			}

			ImGui::Separator();
			ImGui::TextDisabled("Sky Gradient (Atmospheric Sky)");
			ImGui::SliderFloat("Elevation Ramp Speed", &atmoSettings.SunElevationRampSpeed, 0.1f, 5.0f, "%.3f");
			ImGui::TextDisabled("  Low = warm tint persists high up. High = warm only at horizon.");
			ImGui::SliderFloat("Warm Influence",       &atmoSettings.SunWarmInfluence,      0.0f, 1.0f, "%.3f");
			ImGui::TextDisabled("  0 = always white. 1 = full sun color at horizon.");

			ImGui::Spacing();
			if (ImGui::Button("Copy Lua to clipboard (Gameflow.lua)##skyGradient"))
			{
				auto* levelPtr = g_GameFlow->GetLevel(CurrentLevel);
				std::string levelName = "level";
				if (levelPtr && !levelPtr->FileName.empty())
				{
					auto stem = std::filesystem::path(levelPtr->FileName).stem().string();
					if (!stem.empty())
						levelName = stem;
				}

				char buf[256];
				snprintf(buf, sizeof(buf),
					"%s.dynamicSky.skyGradient = %.3f\n"
					"%s.dynamicSky.warmInfluence = %.3f",
					levelName.c_str(), atmoSettings.SunElevationRampSpeed,
					levelName.c_str(), atmoSettings.SunWarmInfluence);
				ImGui::SetClipboardText(buf);
			}

			ImGui::Unindent(8.0f);
		}

		// ----------------------------------------------------------------
		// Sprite ID
		// ----------------------------------------------------------------
		if (ImGui::CollapsingHeader("Sprite"))
		{
			ImGui::Indent(8.0f);

			int spriteID = lensFlare.GetSunSpriteID();
			if (ImGui::InputInt("Sun Sprite ID", &spriteID))
				lensFlare.SetSunSpriteID(spriteID);

			ImGui::Unindent(8.0f);
		}

	}

	void DrawLensFlareDebugOverlay()
	{
		ImGui::SetNextWindowSize(ImVec2(380, 560), ImGuiCond_FirstUseEver);
		if (!ImGui::Begin("Sun / Lens Flare Debug", nullptr, ImGuiWindowFlags_NoCollapse))
		{
			ImGui::End();
			return;
		}
		DrawSunPanelContent();
		ImGui::End();
	}

	void DrawLensFlareTabContent()
	{
		DrawSunPanelContent();
	}
}
