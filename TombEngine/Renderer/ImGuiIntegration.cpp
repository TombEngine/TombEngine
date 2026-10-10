// ImGui bridge: SDL3 handles input and the window, and the next renderer draws the draw data through the RHI, so
// ImGui has no rendering backend of its own.

#include "framework.h"
#include "Renderer/ImGuiIntegration.h"

#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>

namespace TEN::Renderer
{
	static bool s_Initialized    = false;
	static bool s_OverlayVisible = false;

	void ImGuiInit(SDL_Window* window)
	{
		if (s_Initialized)
			return;

		if (window == nullptr)
		{
			TENLog("ImGui init skipped: missing SDL window.", LogLevel::Warning);
			return;
		}

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();

		auto& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		io.IniFilename = nullptr; // No imgui.ini persistence.

		// The next renderer draws each command with its vertex offset and builds the font texture from the atlas.
		io.BackendRendererName = "TombEngine RHI";
		io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
		io.Fonts->Build();

		ImGui::StyleColorsDark();
		auto& style = ImGui::GetStyle();
		style.WindowRounding = 4.0f;
		style.FrameRounding  = 2.0f;
		style.GrabRounding   = 2.0f;
		style.ScrollbarSize  = 14.0f;
		style.Alpha          = 0.95f;

		if (!ImGui_ImplSDL3_InitForOther(window))
		{
			TENLog("ImGui_ImplSDL3_InitForOther failed.", LogLevel::Error);
			ImGui::DestroyContext();
			return;
		}

		s_Initialized = true;
	}

	void ImGuiShutdown()
	{
		if (!s_Initialized)
			return;

		ImGui_ImplSDL3_Shutdown();
		ImGui::GetIO().BackendRendererName = nullptr;
		ImGui::DestroyContext();
		s_Initialized = false;
	}

	void ImGuiNewFrame()
	{
		if (!s_Initialized || !s_OverlayVisible)
			return;

		ImGui_ImplSDL3_NewFrame();
		ImGui::NewFrame();
	}

	void ImGuiRenderFrame()
	{
		if (!s_Initialized || !s_OverlayVisible)
			return;

		ImGui::Render();
	}

	bool ImGuiProcessEvent(const SDL_Event& event)
	{
		if (!s_Initialized)
			return false;

		ImGui_ImplSDL3_ProcessEvent(&event);

		// Only consume the event when the overlay actually has focus, otherwise
		// gameplay keeps receiving input as before.
		if (!s_OverlayVisible)
			return false;

		const auto& io = ImGui::GetIO();
		switch (event.type)
		{
		case SDL_EVENT_MOUSE_MOTION:
		case SDL_EVENT_MOUSE_BUTTON_DOWN:
		case SDL_EVENT_MOUSE_BUTTON_UP:
		case SDL_EVENT_MOUSE_WHEEL:
			return io.WantCaptureMouse;

		case SDL_EVENT_KEY_DOWN:
		case SDL_EVENT_KEY_UP:
		case SDL_EVENT_TEXT_INPUT:
			return io.WantCaptureKeyboard;

		default:
			return false;
		}
	}

	void ImGuiToggleOverlay() { s_OverlayVisible = !s_OverlayVisible; }
	void ImGuiSetOverlayVisible(bool isVisible) { s_OverlayVisible = isVisible; }
	bool ImGuiIsOverlayVisible() { return s_OverlayVisible; }

	const ImDrawData* ImGuiGetDrawData()
	{
		if (!s_Initialized || !s_OverlayVisible)
			return nullptr;

		const auto* drawData = ImGui::GetDrawData();
		return (drawData != nullptr && drawData->Valid) ? drawData : nullptr;
	}
}
