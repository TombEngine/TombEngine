#pragma once

#include <SDL3/SDL.h>

struct ImDrawData;

// ImGui wrapper: imgui_impl_sdl3 handles input and the window, and the scene renderer draws ImGuiGetDrawData() through
// the RHI. ImGuiNewFrame() starts a frame before any ImGui call, and ImGuiRenderFrame() finishes it before the present.
// ImGuiProcessEvent() returns true when ImGui consumed an SDL event, which the engine then ignores.

namespace TEN::Renderer
{
	void ImGuiInit(SDL_Window* window);
	void ImGuiShutdown();
	void ImGuiNewFrame();
	void ImGuiRenderFrame();

	// Returns true if ImGui consumed the event (engine should ignore it).
	bool ImGuiProcessEvent(const SDL_Event& event);

	void ImGuiSetOverlayVisible(bool isVisible);
	bool ImGuiIsOverlayVisible();

	// Scale of the debug overlay for the display height, and the generation of the font atlas built for it.
	float ImGuiGetUiScale();
	int ImGuiGetFontGeneration();

	// Draw data of the last ImGuiRenderFrame, for renderers that draw the overlay themselves; null when hidden.
	const ImDrawData* ImGuiGetDrawData();
}
