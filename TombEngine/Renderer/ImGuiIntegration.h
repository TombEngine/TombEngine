#pragma once

#include <SDL3/SDL.h>

// ============================================================================
// ImGuiIntegration.h — Cross-platform ImGui wrapper for TombEngine.
//
// Backends: imgui_impl_sdl3 (input/window) + imgui_impl_dx11 (rendering).
// SDL3 covers Windows / Linux / macOS uniformly, so this header has no
// OS-specific entry points.
//
// Frame contract:
//   1. ImGuiNewFrame()      — top of the frame, before any ImGui::* calls
//   2. ... ImGui draw calls (DrawSkyCloudDebugOverlay, etc.) ...
//   3. ImGuiRenderFrame()   — just before Present()
//
// Event contract (call from the SDL_PollEvent loop, before dispatching the
// event to the engine):
//   ImGuiProcessEvent(event) returns true if ImGui consumed the event and
//   the engine should drop it (e.g. mouse over an open ImGui window).
//
// Lifetime:
//   ImGuiInit(window) — once, after the SDL window + D3D11 device exist
//   ImGuiShutdown()   — once, before destroying the device
// ============================================================================

namespace TEN::Renderer
{
	void ImGuiInit(SDL_Window* window);
	void ImGuiShutdown();
	void ImGuiNewFrame();
	void ImGuiRenderFrame();

	// Returns true if ImGui consumed the event (engine should ignore it).
	bool ImGuiProcessEvent(const SDL_Event& event);

	// F8 toggle handled internally by ImGuiProcessEvent.
	void ImGuiToggleOverlay();
	bool ImGuiIsOverlayVisible();
}
