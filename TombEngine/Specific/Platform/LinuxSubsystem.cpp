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
		// Inherit UTF-8 locale from the environment (standard on modern Linux).
		setlocale(LC_ALL, "");

		// Disable HiDPI scaling, the engine manages its own resolution.
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

	float LinuxSubsystem::DetectDisplayScale()
	{
		// Only needed when SDL chose the X11 backend (pure X11 or XWayland).
		// On native Wayland the engine disables SDL's own scaling in Initialize()
		// and manages resolution internally, so no window shrink is required.
		const char* videoDriver = SDL_GetCurrentVideoDriver();
		if (!videoDriver || strcmp(videoDriver, "x11") != 0)
			return 1.0f;

		float scale = 1.0f;
		std::string source;

		// 1. User override (highest priority).
		const char* envOverride = SDL_getenv("TEN_HIDPI_SCALE");
		if (envOverride && *envOverride)
		{
			float val = std::strtof(envOverride, nullptr);
			if (val > 1.0f)
			{
				scale = val;
				source = "TEN_HIDPI_SCALE";
			}
		}

		// 2. GNOME sets GDK_SCALE to an integer factor (e.g. "2").
		if (scale <= 1.0f)
		{
			const char* gdkScale = SDL_getenv("GDK_SCALE");
			if (gdkScale && *gdkScale)
			{
				float val = std::strtof(gdkScale, nullptr);
				if (val > 1.0f)
				{
					scale = val;
					source = "GDK_SCALE";
				}
			}
		}

		// 3. KDE/Qt sets QT_SCALE_FACTOR (can be fractional, e.g. "1.5").
		if (scale <= 1.0f)
		{
			const char* qtScale = SDL_getenv("QT_SCALE_FACTOR");
			if (qtScale && *qtScale)
			{
				float val = std::strtof(qtScale, nullptr);
				if (val > 1.0f)
				{
					scale = val;
					source = "QT_SCALE_FACTOR";
				}
			}
		}

		// 4. GNOME monitors.xml — works on native GNOME X11 and XWayland.
		if (scale <= 1.0f)
		{
			std::string monitorsPath;
			const char* configHome = SDL_getenv("XDG_CONFIG_HOME");
			if (configHome && *configHome)
				monitorsPath = std::string(configHome) + "/monitors.xml";
			else
			{
				const char* home = SDL_getenv("HOME");
				if (home && *home)
					monitorsPath = std::string(home) + "/.config/monitors.xml";
			}

			if (!monitorsPath.empty())
			{
				std::ifstream file(monitorsPath);
				if (file.is_open())
				{
					std::string line;
					while (std::getline(file, line))
					{
						auto pos = line.find("<scale>");
						if (pos != std::string::npos)
						{
							auto end = line.find("</scale>", pos);
							if (end != std::string::npos)
							{
								float val = std::strtof(line.c_str() + pos + 7, nullptr);
								if (val > 1.0f)
								{
									scale = val;
									source = "monitors.xml";
								}
							}
							break;
						}
					}
				}
			}
		}

		// 5. WSLg: read Windows DPI from registry via reg.exe.
		if (scale <= 1.0f)
		{
			std::ifstream procVersion("/proc/version");
			if (procVersion.is_open())
			{
				std::string ver;
				std::getline(procVersion, ver);
				if (ver.find("icrosoft") != std::string::npos)
				{
					FILE* pipe = popen("reg.exe query 'HKCU\\Control Panel\\Desktop\\WindowMetrics' /v AppliedDPI 2>/dev/null", "r");
					if (pipe)
					{
						char buf[256] = {};
						while (fgets(buf, sizeof(buf), pipe))
						{
							char* hexPos = strstr(buf, "0x");
							if (hexPos)
							{
								unsigned int dpi = (unsigned int)strtoul(hexPos, nullptr, 16);
								if (dpi > 96)
								{
									scale = (float)dpi / 96.0f;
									source = "WSL/DPI=" + std::to_string(dpi);
								}
								break;
							}
						}
						pclose(pipe);
					}
				}
			}
		}

		// 6. Xft.dpi from X resource database (pure X11 with DPI scaling).
		if (scale <= 1.0f)
		{
			FILE* pipe = popen("xrdb -query 2>/dev/null", "r");
			if (pipe)
			{
				char buf[256] = {};
				while (fgets(buf, sizeof(buf), pipe))
				{
					int dpi = 0;
					if (sscanf(buf, "Xft.dpi: %d", &dpi) == 1 || sscanf(buf, "Xft.dpi:\t%d", &dpi) == 1)
					{
						if (dpi > 96)
						{
							scale = (float)dpi / 96.0f;
							source = "Xft.dpi=" + std::to_string(dpi);
						}
						break;
					}
				}
				pclose(pipe);
			}
		}

		// 7. Fallback to SDL content scale.
		if (scale <= 1.0f)
		{
			SDL_DisplayID primaryDisplay = SDL_GetPrimaryDisplay();
			scale = SDL_GetDisplayContentScale(primaryDisplay);
		}

		if (scale > 1.0f)
			TENLog("X11 HiDPI detected (" + source + "=" + std::to_string(scale) + ")", LogLevel::Info);

		return scale;
	}

	Vector2i LinuxSubsystem::GetScreenResolution()
	{
		auto display = SDL_GetPrimaryDisplay();
		if (display == 0)
			return Vector2i::Zero;

		auto* mode = SDL_GetCurrentDisplayMode(display);
		if (mode == nullptr)
			return Vector2i::Zero;

		// On X11 with HiDPI (e.g. WSLg), the desktop mode reports the logical
		// (scaled-down) resolution. Multiply by the detected scale to recover
		// the actual physical resolution.
		float scale = DetectDisplayScale();
		return Vector2i((int)(mode->w * scale), (int)(mode->h * scale));
	}

	std::vector<Vector2i> LinuxSubsystem::GetAllSupportedScreenResolutions()
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
			// On X11 with HiDPI, desktop mode is already scaled down.
			// Recover the physical resolution for the cap.
			float scale = DetectDisplayScale();
			int maxW = (int)(desktopMode->w * scale);
			int maxH = (int)(desktopMode->h * scale);

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
		return std::make_unique<LinuxSubsystem>();
	}
}

#endif
