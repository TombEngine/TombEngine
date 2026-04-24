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
		// No macOS-specific initialization required.
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

	std::wstring MacOSSubsystem::GetBinaryPath(bool includeExeName)
	{
		uint32_t bufSize = 0;
		_NSGetExecutablePath(nullptr, &bufSize);

		auto buffer = std::vector<char>(bufSize);
		if (_NSGetExecutablePath(buffer.data(), &bufSize) != 0)
		{
			TENLog("Can't get current assembly path", LogLevel::Error);
			return std::wstring();
		}

		// Resolve symlinks via realpath.
		char resolved[PATH_MAX] = {};
		if (realpath(buffer.data(), resolved) == nullptr)
		{
			TENLog("Can't resolve assembly path", LogLevel::Error);
			return std::wstring();
		}

		// Convert to wstring.
		auto result = std::wstring(resolved, resolved + strlen(resolved));
		std::replace(result.begin(), result.end(), L'\\', L'/');

		if (includeExeName)
			return result;

		size_t pos = result.find_last_of(L"/");
		return (pos != std::wstring::npos) ? result.substr(0, pos + 1) : std::wstring();
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
		auto dummyPath = std::filesystem::path(exePath.begin(), exePath.end()) / "dummy.ten";

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

	std::unique_ptr<ISubsystem> CreatePlatformSubsystem()
	{
		return std::make_unique<MacOSSubsystem>();
	}
}

#endif
