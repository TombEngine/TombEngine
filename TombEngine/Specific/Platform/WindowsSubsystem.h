#pragma once

#ifdef SDL_PLATFORM_WIN32

#pragma comment(linker,"/manifestdependency:\"" \
	"type='win32' " \
	"name='Microsoft.Windows.Common-Controls' " \
	"version='6.0.0.0' " \
	"processorArchitecture='*' "  \
	"publicKeyToken='6595b64144ccf1df' " \
	"language='*'\"")

#include <Windows.h>

#include "resource.h"
#include "Specific/Platform/ISubsystem.h"
#include "Specific/trutils.h"

using namespace TEN::Utils;

namespace TEN::Platform
{
	// Windows-specific implementation of the platform subsystem.
	// Encapsulates all remaining Win32 calls (registry access, crash handler,
	// message boxes, etc.) behind a clean interface.
	class WindowsSubsystem final : public ISubsystem
	{
	private:
		// Fields

		SDL_Window* _window        = nullptr;
		HINSTANCE   _hInstance     = nullptr;
		HMODULE     _adpcmLibrary = nullptr;

	public:
		// Constructors

		WindowsSubsystem();
		~WindowsSubsystem() override;

		// Getters

		SDL_Window*                 GetSDL3Window();
		std::wstring                GetBinaryPath(bool includeExeName) override;
		std::vector<unsigned short> GetProductOrFileVersion(bool productVersion) override;

		// Setters

		void SetSDL3Window(SDL_Window* window);

		// Utilities

		void Initialize() override;
		void Tick() override;
		void Shutdown() override;

		void InstallCrashHandler() override;
		bool CreateDummyTitleLevel(const std::string& levelPath) override;
		void CheckPrerequisites() override;
		void ConfigureConsole() override;
		void HideConsole() override;
		void ShowErrorMessage(const std::string& msg) override;

		void InitialiseAudioCodecs() override;
		void ReleaseAudioCodecs() override;

	private:
		// Helpers

		// Checks for the Microsoft Visual C++ Redistributable in the registry and prompts the user to install it if needed.
		void CheckVcRedist();

		// Installs the unhandled exception filter.
		void InstallExceptionFilter();
	};
}

#endif
