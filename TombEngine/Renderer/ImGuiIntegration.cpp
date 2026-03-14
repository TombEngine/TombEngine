// ============================================================================
// ImGuiIntegration.cpp — ImGui D3D11 / Win32 bootstrap for TombEngine.
// ============================================================================

#include "framework.h"
#include "Renderer/ImGuiIntegration.h"

#include <imgui.h>
#include <backends/imgui_impl_win32.h>
#include <backends/imgui_impl_dx11.h>

#include "Renderer/Renderer.h"
#include "Specific/winmain.h"
#include "Game/control/control.h"      // DebugMode

namespace TEN::Renderer
{
	static bool s_Initialized = false;
	static bool s_OverlayVisible = false;

	void ImGuiInit()
	{
		if (s_Initialized)
			return;

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();

		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		io.IniFilename = nullptr; // No imgui.ini persistence.

		ImGui::StyleColorsDark();
		ImGuiStyle& style = ImGui::GetStyle();
		style.WindowRounding  = 4.0f;
		style.FrameRounding   = 2.0f;
		style.GrabRounding    = 2.0f;
		style.ScrollbarSize   = 14.0f;
		style.Alpha           = 0.95f;

		ImGui_ImplWin32_Init(WindowsHandle);
		ImGui_ImplDX11_Init(
			g_Renderer.GetDevice(),
			g_Renderer.GetDeviceContext());

		s_Initialized = true;
	}

	void ImGuiShutdown()
	{
		if (!s_Initialized)
			return;

		ImGui_ImplDX11_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
		s_Initialized = false;
	}

	void ImGuiNewFrame()
	{
		if (!s_Initialized || !s_OverlayVisible)
			return;

		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
	}

	void ImGuiRenderFrame()
	{
		if (!s_Initialized || !s_OverlayVisible)
			return;

		ImGui::Render();
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
	}

	void ImGuiToggleOverlay()
	{
		s_OverlayVisible = !s_OverlayVisible;
	}

	bool ImGuiIsOverlayVisible()
	{
		return s_OverlayVisible;
	}
}
