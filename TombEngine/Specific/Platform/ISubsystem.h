#pragma once

namespace TEN::Platform
{
	enum class MessageBoxIcon
	{
		Info,
		Warning,
		Error
	};

	class ISubsystem
	{
	public:
		// Constructors

		virtual ~ISubsystem() = default;

		// Getters

		virtual SDL_Window*                 GetSDL3Window() = 0;
		virtual std::wstring                GetBinaryPath(bool includeExeName) = 0;
		virtual std::vector<unsigned short> GetProductOrFileVersion(bool productVersion) = 0;

		// Setters

		virtual void SetSDL3Window(SDL_Window* window) = 0;

		// Utilities

		virtual void Initialize() = 0;
		virtual void Tick() = 0;
		virtual void Shutdown() = 0;

		virtual bool CreateDummyTitleLevel(const std::string& levelPath) = 0;
		virtual void InstallCrashHandler() = 0;
		virtual void CheckPrerequisites() = 0;
		virtual void ConfigureConsole() = 0;
		virtual void HideConsole() = 0;
		virtual void ShowErrorMessage(const std::string& text) = 0;

		// Platform-specific audio codec workarounds.
		virtual void InitialiseAudioCodecs() = 0;
		virtual void ReleaseAudioCodecs() = 0;

		// Returns the OS display scale factor (e.g. 2.0 for 200% HiDPI).
		// Used to shrink the window so the compositor's upscaling produces
		// the intended size on screen.  Returns 1.0 when no scaling is needed.
		virtual float DetectDisplayScale() = 0;

		// Returns the native desktop resolution (physical pixels).
		virtual Vector2i GetScreenResolution() = 0;

		// Returns all supported screen resolutions for the primary display.
		virtual std::vector<Vector2i> GetAllSupportedScreenResolutions() = 0;
	};

	std::unique_ptr<ISubsystem> CreatePlatformSubsystem();
}
