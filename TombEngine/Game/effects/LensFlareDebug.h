#pragma once

// ============================================================================
// LensFlareDebug.h — ImGui debug overlay for lens flare / sun control.
//
// Provides:
//   - Interactive circular sun position widget (pitch/yaw control)
//   - Color mode selector and color editing
//   - Real-time preview of evaluated sun color
//   - Direct mutation of the lens flare runtime state
// ============================================================================

namespace TEN::Effects
{
	/// Draw the lens flare / sun debug panel as a standalone window.
	/// Call once per frame when the debug overlay is visible.
	void DrawLensFlareDebugOverlay();

	/// Draw the sun/lens flare panel content without window management.
	/// For use inside a parent ImGui tab.
	void DrawLensFlareTabContent();
}
