// ============================================================================
// ImGuiIntegration.cpp — ImGui SDL3 (input/window) + DX11 (rendering) bridge.
// Lives entirely above the IGraphicsDevice abstraction except for one downcast
// to DX11GraphicsDevice that hands ImGui the raw ID3D11Device*/Context*. When a
// non-DX11 backend exists, swap the rendering half here without touching call
// sites.
// ============================================================================

#include "framework.h"
#include "Renderer/ImGuiIntegration.h"

#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_dx11.h>

#include "Renderer/Renderer.h"
#include "Renderer/Native/DirectX11/DX11GraphicsDevice.h"

namespace TEN::Renderer
{
	using namespace TEN::Renderer::Native::DirectX11;

	static bool s_Initialized   = false;
	static bool s_OverlayVisible = false;

	void ImGuiInit(SDL_Window* window)
	{
		if (s_Initialized)
			return;

		auto* dx11 = dynamic_cast<DX11GraphicsDevice*>(g_Renderer.GetGraphicsDevice());
		if (window == nullptr || dx11 == nullptr)
		{
			TENLog("ImGui init skipped: missing SDL window or DX11 backend.", LogLevel::Warning);
			return;
		}

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();

		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		io.IniFilename = nullptr; // No imgui.ini persistence.

		ImGui::StyleColorsDark();
		auto& style = ImGui::GetStyle();
		style.WindowRounding  = 4.0f;
		style.FrameRounding   = 2.0f;
		style.GrabRounding    = 2.0f;
		style.ScrollbarSize   = 14.0f;
		style.Alpha           = 0.95f;

		if (!ImGui_ImplSDL3_InitForD3D(window))
		{
			TENLog("ImGui_ImplSDL3_InitForD3D failed.", LogLevel::Error);
			ImGui::DestroyContext();
			return;
		}

		if (!ImGui_ImplDX11_Init(dx11->GetD3D11Device(), dx11->GetD3D11Context()))
		{
			TENLog("ImGui_ImplDX11_Init failed.", LogLevel::Error);
			ImGui_ImplSDL3_Shutdown();
			ImGui::DestroyContext();
			return;
		}

		s_Initialized = true;
	}

	void ImGuiShutdown()
	{
		if (!s_Initialized)
			return;

		ImGui_ImplDX11_Shutdown();
		ImGui_ImplSDL3_Shutdown();
		ImGui::DestroyContext();
		s_Initialized = false;
	}

	void ImGuiNewFrame()
	{
		if (!s_Initialized || !s_OverlayVisible)
			return;

		ImGui_ImplDX11_NewFrame();
		ImGui_ImplSDL3_NewFrame();
		ImGui::NewFrame();
	}

	void ImGuiRenderFrame()
	{
		if (!s_Initialized || !s_OverlayVisible)
			return;

		ImGui::Render();
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
	}

	bool ImGuiProcessEvent(const SDL_Event& event)
	{
		// F8 toggle is engine-owned and applies even when the overlay is hidden.
		if (event.type == SDL_EVENT_KEY_DOWN &&
			event.key.scancode == SDL_SCANCODE_F8 &&
			event.key.repeat == 0)
		{
			ImGuiToggleOverlay();
			return true;
		}

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
	bool ImGuiIsOverlayVisible() { return s_OverlayVisible; }
}
