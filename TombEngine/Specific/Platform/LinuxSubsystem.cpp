#include "framework.h"
#include "Specific/Platform/LinuxSubsystem.h"

#ifdef SDL_PLATFORM_LINUX

#include <csignal>
#include <cstring>
#include <execinfo.h>
#include <filesystem>
#include <fstream>
#include <unistd.h>

namespace TEN::Platform
{
	void LinuxSubsystem::Initialize()
	{
		// Inherit UTF-8 locale from the environment (standard on modern Linux).
		setlocale(LC_ALL, "");

		// The engine manages its own resolution, so SDL must not scale Wayland windows.
		SDL_SetHint(SDL_HINT_VIDEO_WAYLAND_SCALE_TO_DISPLAY, "0");
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

	std::string LinuxSubsystem::GetBinaryPath(bool includeExeName)
	{
		static const int MAX_PATH_LENGTH = 1024;
		char buffer[MAX_PATH_LENGTH] = {};

		ssize_t len = readlink("/proc/self/exe", buffer, MAX_PATH_LENGTH - 1);
		if (len <= 0)
		{
			TENLog("Can't get current assembly path", LogLevel::Error);
			return std::string();
		}

		buffer[len] = '\0';

		auto result = std::string(buffer, len);
		std::replace(result.begin(), result.end(), '\\', '/');

		if (includeExeName)
			return result;

		size_t pos = result.find_last_of("/");
		return (pos != std::string::npos) ? result.substr(0, pos + 1) : std::string();
	}

	std::vector<unsigned short> LinuxSubsystem::GetProductOrFileVersion(bool productVersion)
	{
		// Version info is not embedded in ELF binaries in the same way as PE files.
		return {};
	}

	// Scale written in an environment variable; zero when it is not set.
	static float GetEnvironmentScale(const char* name)
	{
		const char* value = SDL_getenv(name);
		if (value == nullptr || *value == '\0')
			return 0.0f;

		return std::strtof(value, nullptr);
	}

	// First line of a command output containing the pattern; empty when there is none.
	static std::string FindCommandOutputLine(const char* command, const char* pattern)
	{
		auto* pipe = popen(command, "r");
		if (pipe == nullptr)
			return {};

		auto result = std::string{};
		char buffer[256] = {};
		while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
		{
			if (strstr(buffer, pattern) != nullptr)
			{
				result = buffer;
				break;
			}
		}

		pclose(pipe);
		return result;
	}

	static bool IsWindowsSubsystemForLinux()
	{
		auto file = std::ifstream("/proc/version");
		auto version = std::string{};
		std::getline(file, version);

		return (version.find("icrosoft") != std::string::npos);
	}

	// X11 reports logical pixels when the desktop is scaled. The scale comes from the user override, the desktop
	// environment variables, the Windows display scale under WSLg, the X resources and finally SDL.
	static float DetectX11DisplayScale()
	{
		constexpr auto DEFAULT_DPI = 96.0f;

		for (const char* name : { "TEN_HIDPI_SCALE", "GDK_SCALE", "QT_SCALE_FACTOR" })
		{
			float scale = GetEnvironmentScale(name);
			if (scale > 1.0f)
				return scale;
		}

		if (IsWindowsSubsystemForLinux())
		{
			auto line = FindCommandOutputLine("reg.exe query 'HKCU\\Control Panel\\Desktop\\WindowMetrics' /v AppliedDPI 2>/dev/null", "0x");
			if (!line.empty())
				return (float)strtoul(line.c_str() + line.find("0x"), nullptr, 16) / DEFAULT_DPI;
		}

		auto line = FindCommandOutputLine("xrdb -query 2>/dev/null", "Xft.dpi:");
		if (!line.empty())
			return std::strtof(line.c_str() + line.find(':') + 1, nullptr) / DEFAULT_DPI;

		return SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
	}

	float LinuxSubsystem::GetDisplayScale()
	{
		// Wayland scaling is disabled in Initialize, so only X11 and XWayland report logical pixels.
		const char* driver = SDL_GetCurrentVideoDriver();
		if (driver == nullptr || strcmp(driver, "x11") != 0)
			return 1.0f;

		static float scale = std::max(DetectX11DisplayScale(), 1.0f);
		return scale;
	}

	VideoMemoryInfo LinuxSubsystem::GetVideoMemoryInfo(const std::string& adapterName)
	{
		// Not reported yet.
		return {};
	}

	static void WriteSignalText(const char* text)
	{
		if (write(STDERR_FILENO, text, strlen(text)) < 0)
			return;
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

			// Only async-signal-safe calls until the stack trace is written.
			constexpr auto MAX_FRAME_COUNT = 32;

			void* frames[MAX_FRAME_COUNT];
			int frameCount = backtrace(frames, MAX_FRAME_COUNT);
			WriteSignalText("\nUnhandled signal: ");
			WriteSignalText(sigName);
			WriteSignalText("\nStack trace:\n");
			backtrace_symbols_fd(frames, frameCount, STDERR_FILENO);

			// The heap may be corrupted, so logging can fail.
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

	std::unique_ptr<ISubsystem> CreatePlatformSubsystem()
	{
		return std::make_unique<LinuxSubsystem>();
	}
}

#endif
