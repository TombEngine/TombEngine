#pragma once

// ============================================================================
// SkyCloudDebug.h — ImGui-based debug overlay for the sky/cloud system.
//
// Contains:
//   - Parameter metadata system (CloudDebugParam)
//   - Full interactive editor with sliders, combo boxes, mouse-wheel editing
//   - Live runtime editing of current state and stored preset definitions
//   - Preset switching, transition triggering, random weather control
// ============================================================================

#include <string>
#include <vector>
#include "Game/Sky/SkyCloudSystem.h"

namespace TEN::Sky
{
	// ====================================================================
	// Parameter metadata — drives slider range, step, format.
	// ====================================================================

	struct CloudDebugParam
	{
		const char* Label    = "";
		float*      ValuePtr = nullptr;
		float       MinValue = 0.0f;
		float       MaxValue = 1.0f;
		float       Step     = 0.01f;      // Mouse-wheel increment.
		const char* Format   = "%.3f";     // Printf format for display.
		float       Default  = 0.0f;       // Preset default (for diff highlighting).
	};

	// ====================================================================
	// Public API
	// ====================================================================

	/// Main entry point: call once per frame when the debug overlay is visible.
	/// Renders the full ImGui cloud debug window with all sections.
	void DrawSkyCloudDebugOverlay();

	/// Plain-text summary (for the old text-based debug page, still useful).
	std::string GetSkyCloudDebugText();
}
