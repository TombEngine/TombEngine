#include "framework.h"
#include "Specific/Platform/LinuxSubsystem.h"

#ifdef SDL_PLATFORM_LINUX

#include <csignal>
#include <filesystem>
#include <fstream>
#include <unistd.h>

namespace TEN::Platform
{
	void LinuxSubsystem::Initialize()
	{
		// No Linux-specific initialization required.
	}

	void LinuxSubsystem::Tick()
	{
		// No periodic Linux-specific work required.
	}

	void LinuxSubsystem::Shutdown()
	{
		// No Linux-specific shutdown required.
	}

	SDL_Window* LinuxSubsystem::GetSDL3Window()
	{
		return _window;
	}

	void LinuxSubsystem::SetSDL3Window(SDL_Window* window)
	{
		_window = window;
	}

	std::wstring LinuxSubsystem::GetBinaryPath(bool includeExeName)
	{
		static const int MAX_PATH_LENGTH = 1024;
		char buffer[MAX_PATH_LENGTH] = {};

		ssize_t len = readlink("/proc/self/exe", buffer, MAX_PATH_LENGTH - 1);
		if (len <= 0)
		{
			TENLog("Can't get current assembly path", LogLevel::Error);
			return std::wstring();
		}

		buffer[len] = '\0';

		// Convert to wstring.
		auto result = std::wstring(buffer, buffer + len);
		std::replace(result.begin(), result.end(), L'\\', L'/');

		if (includeExeName)
			return result;

		size_t pos = result.find_last_of(L"/");
		return (pos != std::wstring::npos) ? result.substr(0, pos + 1) : std::wstring();
	}

	std::vector<unsigned short> LinuxSubsystem::GetProductOrFileVersion(bool productVersion)
	{
		// Version info is not embedded in ELF binaries in the same way as PE files.
		return {};
	}

	void LinuxSubsystem::InstallCrashHandler()
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

	void LinuxSubsystem::CheckPrerequisites()
	{
		// No Linux-specific prerequisites to check.
	}

	void LinuxSubsystem::ConfigureConsole()
	{
		// No Linux-specific console things to configure.
	}

	void LinuxSubsystem::HideConsole()
	{
		// No-op on Linux; console hiding is not applicable.
	}

	void LinuxSubsystem::ShowErrorMessage(const std::string& msg)
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Tomb Engine", msg.c_str(), _window);
	}

	void LinuxSubsystem::InitialiseAudioCodecs()
	{
		// No-op: BASS handles MSADPCM internally on Linux.
	}

	void LinuxSubsystem::ReleaseAudioCodecs()
	{
		// No-op: BASS handles MSADPCM internally on Linux.
	}

	bool LinuxSubsystem::CreateDummyTitleLevel(const std::string& levelPath)
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
		return std::make_unique<LinuxSubsystem>();
	}
}

#endif
