#pragma once

// ============================================================================
// ImGuiIntegration.h — Thin wrapper for ImGui in TEN's D3D11 renderer.
//
// Call order each frame:
//   1. ImGuiNewFrame()      — at the top of the frame (before debug UI)
//   2. ... ImGui draw calls (DrawSkyCloudDebugOverlay, etc.) ...
//   3. ImGuiRenderFrame()   — just before Present()
//
// Lifetime:
//   ImGuiInit()     — once, after D3D11 device/context and HWND are ready
//   ImGuiShutdown() — once, before destroying the device
// ============================================================================

namespace TEN::Renderer
{
	void ImGuiInit();
	void ImGuiShutdown();
	void ImGuiNewFrame();
	void ImGuiRenderFrame();

	// Toggle the ImGui debug overlay on/off (F8).
	void ImGuiToggleOverlay();
	bool ImGuiIsOverlayVisible();
}
