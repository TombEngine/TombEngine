#pragma once

#ifdef SDL_PLATFORM_MACOS

#include "Specific/Platform/ISubsystem.h"

namespace TEN::Platform
{
	// macOS-specific implementation of the platform subsystem.
	class MacOSSubsystem final : public ISubsystem
	{
	private:
		// Fields

		SDL_Window* _window = nullptr;

	public:
		// Getters

		SDL_Window*                 GetSDL3Window() override;
		std::wstring                GetBinaryPath(bool includeExeName) override;
		std::vector<unsigned short> GetProductOrFileVersion(bool productVersion) override;

		// Setters

		void SetSDL3Window(SDL_Window* window) override;

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
	};
}

#endif
