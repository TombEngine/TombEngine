#include "framework.h"
#include "Specific/EngineMain.h"

#include "Game/control/control.h"
#include "Game/savegame.h"
#include "Renderer/Renderer.h"
#include "resource.h"
#include "Sound/sound.h"
#include "Specific/configuration.h"
#include "Specific/level.h"
#include "Specific/Parallel.h"
#include "Specific/trutils.h"
#include "Scripting/Include/ScriptInterfaceState.h"
#include "Scripting/Include/ScriptInterfaceLevel.h"
#include "Scripting/Internal/LanguageScript.h"
#include "Video/Video.h"

using namespace TEN::Renderer;
using namespace TEN::Input;
using namespace TEN::Utils;
using namespace TEN::Video;

// SDL threads
SDL_Thread* GameThread = nullptr;
SDL_Thread* ConsoleThread = nullptr;
unsigned int ThreadSuspendCount = 0;

// Cooperative pause, it emulates Windows APIs for pausing and resuming the game but it's cross platform
SDL_Mutex* GamePauseMutex = nullptr;
SDL_Condition* GamePauseCond = nullptr;
bool       GamePaused = false;

// Global variables
bool ResetClock;
std::unique_ptr<ISubsystem> g_Platform;
std::string GameDirectory;

bool ArgEquals(const char* incomingArg, const std::string& name)
{
	if (!incomingArg)
		return false;

	auto arg = std::string(incomingArg);

	arg = TEN::Utils::ToLower(arg);
	auto lowerName = TEN::Utils::ToLower(name);

	if (!arg.empty() && (arg[0] == '-' || arg[0] == '/'))
	{
		arg.erase(0, 1);
		if (!arg.empty() && arg[0] == '-') 
			arg.erase(0, 1);
	}

	return arg == lowerName;
}

Vector2i GetScreenResolution()
{
	auto screenRes = Vector2i::Zero;

	auto display = SDL_GetPrimaryDisplay();
	if (display == 0)
		return screenRes;

	auto mode = SDL_GetCurrentDisplayMode(display);
	if (mode == nullptr)
		return screenRes;

	screenRes.x = mode->w;
	screenRes.y = mode->h;

	return screenRes;
}

int GetCurrentScreenRefreshRate()
{
	auto display = SDL_GetPrimaryDisplay();
	if (display == 0)
		return 0;

	auto mode = SDL_GetCurrentDisplayMode(display);
	if (mode == nullptr)
		return 0;

	if (mode->refresh_rate <= 0.0f)
		return 0;

	return static_cast<int>(mode->refresh_rate + 0.5f);
}

std::vector<Vector2i> GetAllSupportedScreenResolutions()
{
	auto screenResolutions = std::vector<Vector2i>{};

	auto display = SDL_GetPrimaryDisplay();
	if (display == 0)
		return screenResolutions;

	// Helper to add a resolution if not already in the list.
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

	// SDL may return only the native resolution on some platforms/drivers
	// (e.g. Wayland, some X11 setups, OpenGL on Windows).
	// Always supplement with common resolutions that fit within the desktop size.
	auto* desktopMode = SDL_GetDesktopDisplayMode(display);
	if (desktopMode != nullptr)
	{
		int maxW = desktopMode->w;
		int maxH = desktopMode->h;

		static const Vector2i commonResolutions[] =
		{
			{ 1024,  768 },
			{ 1152,  864 },
			{ 1280,  720 },
			{ 1280,  800 },
			{ 1280, 1024 },
			{ 1360,  768 },
			{ 1366,  768 },
			{ 1440,  900 },
			{ 1600,  900 },
			{ 1600, 1200 },
			{ 1680, 1050 },
			{ 1920, 1080 },
			{ 1920, 1200 },
			{ 2560, 1440 },
			{ 2560, 1600 },
			{ 3440, 1440 },
			{ 3840, 2160 }
		};

		for (const auto& res : commonResolutions)
		{
			if (res.x <= maxW && res.y <= maxH)
				addUnique(res.x, res.y);
		}

		// Always include native desktop resolution.
		addUnique(maxW, maxH);
	}

	std::sort(
		screenResolutions.begin(), screenResolutions.end(),
		[](const Vector2i& screenRes0, const Vector2i& screenRes1)
		{
			return ((screenRes0.x == screenRes1.x) ? (screenRes0.y < screenRes1.y) : (screenRes0.x < screenRes1.x));
		});

	return screenResolutions;
}

int SDLCALL ConsoleInput(void*)
{
	auto input = std::string();
	while (!ThreadEnded)
	{
		if (!std::getline(std::cin, input))
			break;

		if (std::regex_match(input, std::regex("^\\s*$")))
			continue;

		if (g_GameScript == nullptr)
		{
			TENLog("Scripting engine not initialized.", LogLevel::Error);
			continue;
		}
		else
		{
			g_GameScript->AddConsoleInput(input);
		}
	}

	return 0;
}

static void HandleWindowFocusGained(SDL_Window* window)
{
	g_Input.Unlock();

	if (!g_Configuration.EnableWindowedMode)
	{
		g_Renderer.ToggleFullScreen(true);
	}

	if (ThreadSuspendCount > 0)
	{
		TENLog("Resuming game thread.", LogLevel::Info);

		if (!g_VideoPlayer.Resume())
			ResumeAllSounds(SoundPauseMode::Global);

		ResumeGameThread();
	}
}

static void HandleWindowFocusLost(SDL_Window* window)
{
	g_Input.Lock();

	if (!g_Configuration.EnableWindowedMode)
		SDL_MinimizeWindow(window);

	bool isMinimized =
		(SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) != 0;

	if ((!DebugMode || isMinimized) && ThreadSuspendCount == 0)
	{
		TENLog("Suspending game thread.", LogLevel::Info);

		if (!g_VideoPlayer.Pause())
			PauseAllSounds(SoundPauseMode::Global);

		PauseGameThread();
	}
}

void PauseGameThread()
{
	SDL_LockMutex(GamePauseMutex);

	ThreadSuspendCount++;
	GamePaused = true;

	SDL_UnlockMutex(GamePauseMutex);
}

void ResumeGameThread()
{
	SDL_LockMutex(GamePauseMutex);

	if (ThreadSuspendCount > 0)
		ThreadSuspendCount--;

	if (ThreadSuspendCount == 0)
	{
		GamePaused = false;
		SDL_BroadcastCondition(GamePauseCond);
	}

	SDL_UnlockMutex(GamePauseMutex);
}

void WaitIfGamePaused()
{
	SDL_LockMutex(GamePauseMutex);
	while (GamePaused && !ThreadEnded && DoTheGame)
	{
		SDL_WaitCondition(GamePauseCond, GamePauseMutex);
	}
	SDL_UnlockMutex(GamePauseMutex);
}

int main(int argc, char* argv[])
{
	g_Platform = CreatePlatformSubsystem();
	g_Platform->Initialize();
	g_Platform->CheckPrerequisites();

	// Initialize SDL3.
	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS))
	{
		// Handle error.
		return 1;
	}

	// Process command line arguments.
	auto levelFile = std::string();
	auto gameDir = std::string();
	auto cmdLineApi = GraphicsAPI::Auto;

	// Parse command line arguments.
	for (int i = 1; i < argc; ++i)
	{
		auto arg = std::string(argv[i]);

		if (ArgEquals(arg.c_str(), "debug"))
		{
			DebugMode = true;
		}
		else if (ArgEquals(arg.c_str(), "level") && i + 1 < argc)
		{
			levelFile = argv[++i];
		}
		else if (ArgEquals(arg.c_str(), "hash") && i + 1 < argc)
		{
			SystemNameHash = std::stoul(argv[++i]);
		}
		else if (ArgEquals(arg.c_str(), "gamedir") && i + 1 < argc)
		{
			gameDir = argv[++i];
		}
		else if (ArgEquals(arg.c_str(), "api") && i + 1 < argc)
		{
			auto val = TEN::Utils::ToLower(std::string(argv[++i]));
			if (val == "dx11" || val == "d3d11" || val == "directx11")
				cmdLineApi = GraphicsAPI::DirectX11;
			else if (val == "opengl" || val == "gl")
				cmdLineApi = GraphicsAPI::OpenGL;
		}
	}

	// Construct asset directory.
	GameDirectory = ConstructAssetDirectory(gameDir);

	// Hide console window if mode isn't debug.
#if !_DEBUG
	if (!DebugMode)
	{
		g_Platform->HideConsole();
	}
	else
#endif
	{
		ConsoleThread = SDL_CreateThread(ConsoleInput, "ConsoleInput", nullptr);
		if (ConsoleThread)
			SDL_DetachThread(ConsoleThread);

		g_Platform->ConfigureConsole();
	}

	// Initialize logging.
	InitTENLog(GameDirectory);
	g_Platform->InstallCrashHandler();

	auto windowName = std::string("Starting Tomb Engine");

	// Indicate version.
	auto ver = g_Platform->GetProductOrFileVersion(false);

	if (ver.size() == 4)
	{
		windowName = windowName + " version " +
					 std::to_string(ver[0]) + "." +
					 std::to_string(ver[1]) + "." +
					 std::to_string(ver[2]) + "." +
					 std::to_string(ver[3]);
	}

#ifdef PLATFORM_64BIT
		windowName = windowName + " (64-bit)";
#else
		windowName = windowName + " (32-bit)";
#endif

	TENLog(windowName, LogLevel::Info);

	// Initialize savegame and scripting systems.
	SaveGame::Init(GameDirectory);
	ScriptInterfaceState::Init(GameDirectory);

	// Initialize scripting.
	try 
	{
		g_GameFlow = ScriptInterfaceState::CreateFlow();
		g_GameScriptEntities = ScriptInterfaceState::CreateObjectsHandler();
		g_GameStringsHandler = ScriptInterfaceState::CreateStringsHandler();

		// This must be loaded last as it adds metafunctions to the global
		// table so that every global variable added henceforth gets put
		// into a special hidden table which we can clean up.
		// By doing this last, we ensure that all built-in usertypes
		// are added to a hierarchy in the REAL global table, not the fake
		// hidden one.
		g_GameScript = ScriptInterfaceState::CreateGame();

		// TODSO: Major hack. This should not be needed to leak outside of
		// LogicHandler internals. In a future version stuff from FlowHandler
		// should be moved to LogicHandler or vice versa to make this stuff
		// less fragile (squidshire, 16/09/22)
		g_GameScript->ShortenTENCalls();
		g_GameFlow->SetGameDir(GameDirectory);
		g_GameFlow->LoadFlowScript();
	}
	catch (TENScriptException const& ex)
	{
		auto errorMessage = std::string("A Lua error occurred while setting up scripts ") + __func__ + ": " + ex.what();
		TENLog(errorMessage, LogLevel::Error, LogConfig::All);
		g_Platform->ShowErrorMessage(errorMessage);
		EngineClose();
		exit(EXIT_FAILURE);
	}

	// Load configuration and optionally show setup dialog.
	if (!LoadConfiguration())
		InitDefaultConfiguration();

	// @inputme
	g_Bindings.Initialize(g_Configuration.KeyboardMouseBindings, g_Configuration.GamepadBindings);

	// Resolve GraphicsAPI (command line overrides config).
	auto resolvedApi = (cmdLineApi != GraphicsAPI::Auto) ? cmdLineApi : g_Configuration.RendererAPI;
	if (resolvedApi == GraphicsAPI::Auto)
	{
#ifdef HAS_DX11
		resolvedApi = GraphicsAPI::DirectX11;
#else
		resolvedApi = GraphicsAPI::OpenGL;
#endif
	}

	// Initialize main window.
	int width = g_Configuration.ScreenWidth;
	int height = g_Configuration.ScreenHeight;

	unsigned int windowFlags = SDL_WINDOW_RESIZABLE;
	if (resolvedApi == GraphicsAPI::OpenGL)
		windowFlags |= SDL_WINDOW_OPENGL;
	if (!g_Configuration.EnableWindowedMode)
		windowFlags |= SDL_WINDOW_FULLSCREEN;

	auto sdlWindow = SDL_CreateWindow(
		g_GameFlow->GetString(STRING_WINDOW_TITLE),
		width,
		height,
		windowFlags);

	if (!sdlWindow)
	{
		auto errorMessage = std::string("Failed to create SDL window: ") + SDL_GetError();
		TENLog(errorMessage, LogLevel::Error);
		g_Platform->ShowErrorMessage(errorMessage);
		EngineClose();
		exit(EXIT_FAILURE);
	}

	g_Platform->SetSDL3Window(sdlWindow);

	// Create renderer device (must happen after SDL window is created for OpenGL context).
	g_Renderer.Create(resolvedApi);

	// Update adapter name now that the renderer is available.
	if (g_Configuration.AdapterName.empty())
		g_Configuration.AdapterName = g_Renderer.GetDefaultAdapterName();

	try
	{
		// Initialize audio (should be called prior to initializing renderer, because video handler needs it).
		Sound_Init(GameDirectory);

		// Initialize renderer.
		g_Renderer.Initialize(GameDirectory, g_Configuration.ScreenWidth, g_Configuration.ScreenHeight, g_Configuration.EnableWindowedMode);

		// Initialize input.
		g_Input.Initialize();

		// Load level if specified in command line.
		CurrentLevel = g_GameFlow->GetLevelNumber(levelFile);

		SDL_ShowWindow(sdlWindow);
		SDL_RaiseWindow(sdlWindow);
	}
	catch (std::exception& ex)
	{
		auto errorMessage = "Error during game initialization: " + std::string(ex.what());
		TENLog(errorMessage, LogLevel::Error);
		g_Platform->ShowErrorMessage(errorMessage);
		EngineClose();
		exit(EXIT_FAILURE);
	}

	DoTheGame = true;

	g_Parallel.Initialize();
	ThreadEnded = false;
	ThreadSuspendCount = 0;

	GamePauseMutex = SDL_CreateMutex();
	GamePauseCond = SDL_CreateCondition();

	GameThread = SDL_CreateThread(GameMain, "GameMain", nullptr);
	if (!GameThread)
	{
		TENLog(std::string("Failed to create game thread: ") + SDL_GetError(), LogLevel::Error);
		DoTheGame = false;
	}

	// Since the game window likes to steal input anyway, put it at the foreground so the user at least expects it.
	auto* focusedWindow = SDL_GetKeyboardFocus();
	if (focusedWindow != sdlWindow)
	{
		SDL_RaiseWindow(sdlWindow);
	}

	bool running = true;
	while (running && !ThreadEnded && DoTheGame)
	{
		auto event = SDL_Event{};
		while (SDL_PollEvent(&event))
		{
			switch (event.type)
			{
			case SDL_EVENT_QUIT:
			case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
				DoTheGame = false;
				running = false;
				break;

			case SDL_EVENT_WINDOW_FOCUS_GAINED:
				HandleWindowFocusGained(sdlWindow);
				break;

			case SDL_EVENT_WINDOW_FOCUS_LOST:
				HandleWindowFocusLost(sdlWindow);
				break;

			default:
				break;
			}
		}

		// Avoid looping at 100% where there are no events.
		SDL_Delay(1);
	}

	ThreadEnded = true;

	while (DoTheGame)
		SDL_Delay(1);

	TENLog("Cleaning up and exiting...", LogLevel::Info);

	SDL_DestroyWindow(sdlWindow);
	EngineClose();

	exit(EXIT_SUCCESS);
}

void EngineClose()
{
	if (GameThread)
	{
		int status = 0;
		SDL_LockMutex(GamePauseMutex);
		GamePaused = false;
		SDL_BroadcastCondition(GamePauseCond);
		SDL_UnlockMutex(GamePauseMutex);

		SDL_WaitThread(GameThread, &status);
		GameThread = nullptr;
	}

	if (GamePauseCond)
	{
		SDL_DestroyCondition(GamePauseCond);
		GamePauseCond = nullptr;
	}
	if (GamePauseMutex)
	{
		SDL_DestroyMutex(GamePauseMutex);
		GamePauseMutex = nullptr;
	}

	g_Platform->Shutdown();

	SDL_Quit();

	ShutdownTENLog();
}
