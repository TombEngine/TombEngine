#include "framework.h"
#include "Specific/Platform/MacOSSubsystem.h"

#ifdef SDL_PLATFORM_MACOS

#include <csignal>
#include <filesystem>
#include <fstream>
#include <mach-o/dyld.h>

namespace TEN::Platform
{
	void MacOSSubsystem::Initialize()
	{
		// Inherit UTF-8 locale from the environment (standard on macOS).
		setlocale(LC_ALL, "");
	}

	void MacOSSubsystem::Tick()
	{
		// No periodic macOS-specific work required.
	}

	void MacOSSubsystem::Shutdown()
	{
		// No macOS-specific shutdown required.
	}

	SDL_Window* MacOSSubsystem::GetSDL3Window()
	{
		return _window;
	}

	void MacOSSubsystem::SetSDL3Window(SDL_Window* window)
	{
		_window = window;
	}

	std::string MacOSSubsystem::GetBinaryPath(bool includeExeName)
	{
		unsigned int bufSize = 0;
		_NSGetExecutablePath(nullptr, &bufSize);

		auto buffer = std::vector<char>(bufSize);
		if (_NSGetExecutablePath(buffer.data(), &bufSize) != 0)
		{
			TENLog("Can't get current assembly path", LogLevel::Error);
			return std::string();
		}

		// Resolve symlinks via realpath.
		char resolved[PATH_MAX] = {};
		if (realpath(buffer.data(), resolved) == nullptr)
		{
			TENLog("Can't resolve assembly path", LogLevel::Error);
			return std::string();
		}

		auto result = std::string(resolved);
		std::replace(result.begin(), result.end(), '\\', '/');

		if (includeExeName)
			return result;

		size_t pos = result.find_last_of("/");
		return (pos != std::string::npos) ? result.substr(0, pos + 1) : std::string();
	}

	std::vector<unsigned short> MacOSSubsystem::GetProductOrFileVersion(bool productVersion)
	{
		// Version info is not embedded in Mach-O binaries in the same way as PE files.
		return {};
	}

	void MacOSSubsystem::InstallCrashHandler()
	{
		static const auto handler = [](int sig)
		{
			const char* sigName = "Unknown signal";

			switch (sig)
			{
			case SIGSEGV: sigName = "Segmentation fault (SIGSEGV)"; break;
			case SIGABRT: sigName = "Abort (SIGABRT)"; break;
			case SIGFPE:  sigName = "Floating-point exception (SIGFPE)"; break;
			case SIGBUS:  sigName = "Bus error (SIGBUS)"; break;
			case SIGILL:  sigName = "Illegal instruction (SIGILL)"; break;
			}

			auto errorMessage = "Unhandled signal: " + std::string(sigName) + ".";
			TENLog(errorMessage, LogLevel::Error);

			// Re-raise with default handler to produce a core dump.
			signal(sig, SIG_DFL);
			raise(sig);
		};

		struct sigaction sa = {};
		sa.sa_handler = handler;
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = SA_RESETHAND;

		sigaction(SIGSEGV, &sa, nullptr);
		sigaction(SIGABRT, &sa, nullptr);
		sigaction(SIGFPE,  &sa, nullptr);
		sigaction(SIGBUS,  &sa, nullptr);
		sigaction(SIGILL,  &sa, nullptr);
	}

	void MacOSSubsystem::CheckPrerequisites()
	{
		// No macOS-specific prerequisites to check.
	}

	void MacOSSubsystem::ConfigureConsole()
	{
		// No macOS-specific console things to configure.
	}

	void MacOSSubsystem::HideConsole()
	{
		// No-op on macOS; console hiding is not applicable.
	}

	void MacOSSubsystem::ShowErrorMessage(const std::string& msg)
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Tomb Engine", msg.c_str(), _window);
	}

	void MacOSSubsystem::InitialiseAudioCodecs()
	{
		// No-op: BASS handles MSADPCM internally on macOS.
	}

	void MacOSSubsystem::ReleaseAudioCodecs()
	{
		// No-op: BASS handles MSADPCM internally on macOS.
	}

	bool MacOSSubsystem::CreateDummyTitleLevel(const std::string& levelPath)
	{
		// Look for dummy.ten next to the executable.
		auto exePath = GetBinaryPath(false);
		auto dummyPath = std::filesystem::path(exePath) / "dummy.ten";

		if (!std::filesystem::is_regular_file(dummyPath))
		{
			TENLog("Embedded title level file not found.", LogLevel::Error);
			return false;
		}

		try
		{
			auto dir = std::filesystem::path(levelPath).parent_path();
			if (!dir.empty())
				std::filesystem::create_directories(dir);

			std::filesystem::copy_file(dummyPath, levelPath, std::filesystem::copy_options::overwrite_existing);
		}
		catch (const std::exception& ex)
		{
			TENLog("Error while generating title level file: " + std::string(ex.what()), LogLevel::Error);
			return false;
		}

		return true;
	}

	float MacOSSubsystem::DetectDisplayScale()
	{
		// SDL handles HiDPI correctly on macOS.
		return 1.0f;
	}

	Vector2i MacOSSubsystem::GetScreenResolution()
	{
		auto display = SDL_GetPrimaryDisplay();
		if (display == 0)
			return Vector2i::Zero;

		auto* mode = SDL_GetCurrentDisplayMode(display);
		if (mode == nullptr)
			return Vector2i::Zero;

		return Vector2i(mode->w, mode->h);
	}

	std::vector<Vector2i> MacOSSubsystem::GetAllSupportedScreenResolutions()
	{
		auto screenResolutions = std::vector<Vector2i>{};

		auto display = SDL_GetPrimaryDisplay();
		if (display == 0)
			return screenResolutions;

		auto addUnique = [&screenResolutions](int w, int h)
		{
			for (const auto& res : screenResolutions)
			{
				if (res.x == w && res.y == h)
					return;
			}
			screenResolutions.push_back(Vector2i(w, h));
		};

		int count = 0;
		auto modes = SDL_GetFullscreenDisplayModes(display, &count);
		if (modes != nullptr && count > 0)
		{
			screenResolutions.reserve(count);
			for (int i = 0; i < count; ++i)
			{
				const auto* mode = modes[i];
				if (mode != nullptr)
					addUnique(mode->w, mode->h);
			}
			SDL_free(modes);
		}

		auto* desktopMode = SDL_GetDesktopDisplayMode(display);
		if (desktopMode != nullptr)
		{
			int maxW = desktopMode->w;
			int maxH = desktopMode->h;

			static const Vector2i commonResolutions[] =
			{
				{ 1024,  768 }, { 1152,  864 }, { 1280,  720 }, { 1280,  800 },
				{ 1280, 1024 }, { 1360,  768 }, { 1366,  768 }, { 1440,  900 },
				{ 1600,  900 }, { 1600, 1200 }, { 1680, 1050 }, { 1920, 1080 },
				{ 1920, 1200 }, { 2560, 1440 }, { 2560, 1600 }, { 3440, 1440 },
				{ 3840, 2160 }
			};

			for (const auto& res : commonResolutions)
			{
				if (res.x <= maxW && res.y <= maxH)
					addUnique(res.x, res.y);
			}

			addUnique(maxW, maxH);
		}

		std::sort(screenResolutions.begin(), screenResolutions.end(),
			[](const Vector2i& a, const Vector2i& b)
			{
				return (a.x == b.x) ? (a.y < b.y) : (a.x < b.x);
			});

		return screenResolutions;
	}

	std::unique_ptr<ISubsystem> CreatePlatformSubsystem()
	{
		return std::make_unique<MacOSSubsystem>();
	}
}

#endif
