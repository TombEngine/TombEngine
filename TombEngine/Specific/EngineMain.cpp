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

	int count = 0;
	auto modes = SDL_GetFullscreenDisplayModes(display, &count);
	if (modes == nullptr || count <= 0)
		return screenResolutions;

	screenResolutions.reserve(count);

	for (int i = 0; i < count; ++i)
	{
		const auto* mode = modes[i];
		if (mode == nullptr)
			continue;

		auto screenResolution = Vector2i(mode->w, mode->h);

		bool add = true;
		for (const auto& screenRes : screenResolutions)
		{
			if (screenRes.x == screenResolution.x &&
				screenRes.y == screenResolution.y)
			{
				add = false;
				break;
			}
		}

		if (add)
			screenResolutions.push_back(screenResolution);
	}

	SDL_free(modes);

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
	SetInputLockState(false);

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
	SetInputLockState(true);

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

	// Process command line arguments.
	auto levelFile = std::string();
	auto gameDir = std::string();

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
	}

	// Construct asset directory.
	GameDirectory = ConstructAssetDirectory(gameDir);

	// Initialize logging as early as possible so any later failure is captured.
	InitTENLog(GameDirectory);
	g_Platform->InstallCrashHandler();

	// Initialize SDL3.
	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS))
	{
		auto error = std::string("Failed to initialize SDL: ") + SDL_GetError();
		TENLog(error, LogLevel::Error);
		g_Platform->ShowErrorMessage(error);
		ShutdownTENLog();
		return 1;
	}

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

		// Load global variables from external file.
		SaveGame::LoadGlobalVars();
	}
	catch (TENScriptException const& ex)
	{
		auto errorMessage = std::string("A Lua error occurred while setting up scripts ") + __func__ + ": " + ex.what();
		TENLog(errorMessage, LogLevel::Error, LogConfig::All);
		g_Platform->ShowErrorMessage(errorMessage);
		EngineClose();
		exit(EXIT_FAILURE);
	}

	g_Renderer.Create();

	// Load configuration and optionally show setup dialog.
	if (!LoadConfiguration())
		InitDefaultConfiguration();

	g_Bindings.Initialize();

	// Initialize main window.
	int width = g_Configuration.ScreenWidth;
	int height = g_Configuration.ScreenHeight;

	unsigned int windowFlags = SDL_WINDOW_RESIZABLE;
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

	try
	{
		// Initialize audio (should be called prior to initializing renderer, because video handler needs it).
		Sound_Init(GameDirectory);

		// Initialize renderer.
		g_Renderer.Initialize(GameDirectory, g_Configuration.ScreenWidth, g_Configuration.ScreenHeight, g_Configuration.EnableWindowedMode);

		// Initialize input.
		InitializeInput();

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
