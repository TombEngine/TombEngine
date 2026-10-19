#include "framework.h"
#include "Specific/EngineMain.h"

#include "Game/control/control.h"
#include "Game/savegame.h"
#include "Renderer/Renderer.h"
#include "Renderer/UI/ImGuiIntegration.h"
#include "Renderer/Backend/SceneRenderer.h"
#include "Renderer/RHI/Test/RHITest.h"
#include "resource.h"
#include "Sound/sound.h"
#include "Specific/Benchmark.h"
#include "Specific/configuration.h"
#include "Specific/Input/Input.h"
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
bool GamePaused = false;

// Global variables
bool ResetClock;
std::unique_ptr<ISubsystem> g_Platform;
std::string GameDirectory;

// Pending window resize in windowed mode, produced by the SDL event loop and consumed by the game thread before rendering.
SDL_Mutex* NextResolutionMutex = nullptr;
static std::optional<Vector2i> NextResolution = std::nullopt;

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

TEN::Renderer::RHI::GraphicsApi ParseRHIBackend(const std::string& name)
{
	if (name == "dx11")
		return TEN::Renderer::RHI::GraphicsApi::DirectX11;

	if (name == "d3d12")
		return TEN::Renderer::RHI::GraphicsApi::Direct3D12;

	if (name == "opengl")
		return TEN::Renderer::RHI::GraphicsApi::OpenGL;

	return TEN::Renderer::RHI::GraphicsApi::Vulkan;
}

// Tries the requested scene renderer backend, then the others in order. Returns false when none starts.
bool StartSceneRenderer(SDL_Window* window, const std::string& requestedBackend, bool preferLowPowerGpu)
{
	const std::string backends[] = { requestedBackend, "vulkan", "d3d12", "dx11", "opengl" };
	for (int i = 0; i < (int)std::size(backends); i++)
	{
		if (std::find(backends, backends + i, backends[i]) != backends + i)
			continue;

		if (TEN::Renderer::Backend::g_SceneRenderer.Initialize(window, ParseRHIBackend(backends[i]), GameDirectory, preferLowPowerGpu))
			return true;

		TENLog("Scene renderer could not start on the " + backends[i] + " backend.", LogLevel::Warning);
	}

	return false;
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

	return (int)(mode->refresh_rate + 0.5f);
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

	// Benchmarks keep running in the background, so a focus change does not invalidate the measurement.
	if ((!DebugMode || isMinimized) && ThreadSuspendCount == 0 && !TEN::Benchmark::IsBenchmarkActive())
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
		ResetClock = true;

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
	float benchmarkDuration = 0.0f;
	auto rhiTestBackend = std::string();
	bool isOnLowPowerGpu = false;
	bool isVsyncDisabled = false;
	bool isGpuTimingDisabled = false;
	int renderScaleOverride = -1;
	int ambientOcclusionOverride = -1;
	int antialiasingOverride = -1;
	int widthOverride = 0;
	int heightOverride = 0;
	auto rendererBackend = std::string();

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
		else if (ArgEquals(arg.c_str(), "benchmark") && i + 1 < argc)
		{
			benchmarkDuration = std::stof(argv[++i]);
		}
		else if (ArgEquals(arg.c_str(), "rhitest") && i + 1 < argc)
		{
			rhiTestBackend = argv[++i];
		}
		else if (ArgEquals(arg.c_str(), "next") && i + 1 < argc)
		{
			// Renderer backend, overriding the configuration.
			rendererBackend = argv[++i];
		}
		else if (ArgEquals(arg.c_str(), "nogputiming"))
		{
			// Benchmarks without GPU pass timings, which split the command buffers on SDL GPU.
			isGpuTimingDisabled = true;
		}
		else if (ArgEquals(arg.c_str(), "novsync"))
		{
			// Presents without waiting for vertical sync, for benchmarks.
			isVsyncDisabled = true;
		}
		else if (ArgEquals(arg.c_str(), "renderscale") && i + 1 < argc)
		{
			// Render scale mode for this run (0 native to 4 FSR 1 performance), overriding the configuration, for benchmarks.
			renderScaleOverride = std::stoi(argv[++i]);
		}
		else if (ArgEquals(arg.c_str(), "ssao") && i + 1 < argc)
		{
			// Ambient occlusion on or off for this run, overriding the configuration, for benchmarks.
			ambientOcclusionOverride = std::stoi(argv[++i]);
		}
		else if (ArgEquals(arg.c_str(), "antialiasing") && i + 1 < argc)
		{
			// Antialiasing mode for this run (0 none to 3 high), overriding the configuration, for benchmarks.
			antialiasingOverride = std::stoi(argv[++i]);
		}
		else if (ArgEquals(arg.c_str(), "resolution") && i + 2 < argc)
		{
			// Windowed screen resolution for this run, overriding the configuration, for benchmarks.
			widthOverride = std::stoi(argv[++i]);
			heightOverride = std::stoi(argv[++i]);
		}
		else if (ArgEquals(arg.c_str(), "lowpower"))
		{
			// Runs the renderer on the integrated GPU of hybrid systems.
			isOnLowPowerGpu = true;
		}
	}

	// Construct asset directory.
	GameDirectory = ConstructAssetDirectory(gameDir);

	// Initialize logging as early as possible so any later failure is captured.
	InitTENLog(GameDirectory);
	g_Platform->InstallCrashHandler();

	if (benchmarkDuration > 0.0f)
		TEN::Benchmark::InitializeBenchmark(benchmarkDuration, GameDirectory);

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

	g_Bindings.Initialize();

	// Load configuration and optionally show setup dialog.
	if (!LoadConfiguration())
	{
		InitDefaultConfiguration();
		SetAudioConfiguration(g_Configuration);
	}

	if (renderScaleOverride >= 0)
		g_Configuration.RenderScale = (RenderScaleMode)std::clamp(renderScaleOverride, 0, (int)RenderScaleMode::Count - 1);

	if (ambientOcclusionOverride >= 0)
		g_Configuration.EnableAmbientOcclusion = (ambientOcclusionOverride != 0);

	if (antialiasingOverride >= 0)
		g_Configuration.AntialiasingMode = (AntialiasingMode)std::clamp(antialiasingOverride, 0, (int)AntialiasingMode::High);

	if (widthOverride > 0 && heightOverride > 0)
	{
		g_Configuration.ScreenWidth = widthOverride;
		g_Configuration.ScreenHeight = heightOverride;
		g_Configuration.EnableWindowedMode = true;
	}

	// The configured backend applies when the command line does not choose one; the retired legacy option uses Vulkan.
	if (rendererBackend.empty())
		rendererBackend = GetGraphicsBackendName(g_Configuration.GraphicsBackend);

	// Initialize main window.
	int width = g_Configuration.ScreenWidth;
	int height = g_Configuration.ScreenHeight;

	// OpenGL needs its window flag at creation, so a fallback from it to the other backends keeps an OpenGL window.
	auto windowBackend = ParseRHIBackend(rhiTestBackend.empty() ? rendererBackend : rhiTestBackend);
	unsigned int windowFlags = SDL_WINDOW_RESIZABLE | TEN::Renderer::RHI::GetWindowFlags(windowBackend);
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

	// Backend validation run: render the RHI test scene, save a screenshot and quit.
	if (!rhiTestBackend.empty())
	{
		auto backend = ParseRHIBackend(rhiTestBackend);

		SDL_ShowWindow(sdlWindow);
		bool result = TEN::Renderer::RHI::RunRHITest(sdlWindow, backend, GameDirectory, 120);

		SDL_DestroyWindow(sdlWindow);
		ShutdownTENLog();
		exit(result ? EXIT_SUCCESS : EXIT_FAILURE);
	}

	try
	{
		// Initialize audio (should be called prior to initializing renderer, because video handler needs it).
		Sound_Init(GameDirectory);

		// Initialize renderer.
		// The scene renderer draws the main window from the frames the renderer frontend prepares.
		if (!StartSceneRenderer(sdlWindow, rendererBackend, isOnLowPowerGpu))
			throw std::runtime_error("No renderer backend could be started.");

		if (isVsyncDisabled)
			TEN::Renderer::Backend::g_SceneRenderer.SetVsyncEnabled(false);

		TEN::Renderer::Backend::g_SceneRenderer.SetBenchmarkGpuTimingEnabled(!isGpuTimingDisabled);

		g_Renderer.Initialize(GameDirectory, g_Configuration.ScreenWidth, g_Configuration.ScreenHeight, g_Configuration.EnableWindowedMode);

		// Initialize ImGui (debug overlay), drawn by the scene renderer.
		TEN::Renderer::ImGuiInit(sdlWindow);

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
	NextResolutionMutex = SDL_CreateMutex();

	if (!GamePauseMutex || !GamePauseCond || !NextResolutionMutex)
	{
		auto errorMessage = fmt::format("Failed to create SDL synchronization primitives: {}", SDL_GetError());
		TENLog(errorMessage, LogLevel::Error);
		g_Platform->ShowErrorMessage(errorMessage);
		EngineClose();
		exit(EXIT_FAILURE);
	}

	// The game thread renders from now on.
	TEN::Renderer::Backend::g_SceneRenderer.ReleaseThread();
	GameThread = SDL_CreateThread(GameMain, "GameMain", nullptr);
	if (!GameThread)
	{
		TENLog(fmt::format("Failed to create game thread: {}", SDL_GetError()), LogLevel::Error);
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
		TEN::Benchmark::UpdateBenchmark();
		TEN::Renderer::Backend::g_SceneRenderer.PresentOnWindowThread();

		auto event = SDL_Event{};
		while (SDL_PollEvent(&event))
		{
			// Let ImGui see every event first; it returns true when the overlay
			// has focus and the event must not propagate to gameplay (so e.g. a
			// click on a debug window doesn't also fire in the game world). The
			// F8 toggle is handled here too.
			if (TEN::Renderer::ImGuiProcessEvent(event))
				continue;

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

			case SDL_EVENT_WINDOW_RESIZED:
			case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
			{
				if (!g_Configuration.EnableWindowedMode)
					break;

				int w = 0, h = 0;
				SDL_GetWindowSizeInPixels(sdlWindow, &w, &h);

				SDL_LockMutex(NextResolutionMutex);
				NextResolution = Vector2i(w, h);
				SDL_UnlockMutex(NextResolutionMutex);
				break;
			}

			default:
				HandleSDLEvent(event);
				break;
			}
		}

		// Avoid looping at 100% where there are no events.
		SDL_Delay(1);
	}

	ThreadEnded = true;

	// The game thread may still render a last frame (EndGameLoop) after DoTheGame was cleared here,
	// so it must finish before renderers and windows are released.
	WaitForGameThread();

	TENLog("Cleaning up and exiting...", LogLevel::Info);

	TEN::Benchmark::ShutdownBenchmark();

	// Tear ImGui down before the window: its SDL backend holds the window.
	TEN::Renderer::ImGuiShutdown();

	TEN::Renderer::Backend::g_SceneRenderer.Shutdown();

	SDL_DestroyWindow(sdlWindow);
	EngineClose();

	exit(EXIT_SUCCESS);
}

void ApplyPendingWindowResize()
{
	SDL_LockMutex(NextResolutionMutex);
	if (!NextResolution.has_value())
	{
		SDL_UnlockMutex(NextResolutionMutex);
		return;
	}

	auto res = NextResolution.value();
	NextResolution = std::nullopt;
	SDL_UnlockMutex(NextResolutionMutex);

	if (!g_Configuration.EnableWindowedMode)
		return;

	if (res.x <= 0 || res.y <= 0)
		return;

	if (res.x == g_Configuration.ScreenWidth && res.y == g_Configuration.ScreenHeight)
		return;

	g_Renderer.ChangeScreenResolution(res.x, res.y, true, false);
	g_Renderer.SetGraphicsSettingsChanged();
	g_Configuration.ScreenWidth = res.x;
	g_Configuration.ScreenHeight = res.y;
	SaveConfiguration();
}

void WaitForGameThread()
{
	if (GameThread == nullptr)
		return;

	int status = 0;
	SDL_LockMutex(GamePauseMutex);
	GamePaused = false;
	SDL_BroadcastCondition(GamePauseCond);
	SDL_UnlockMutex(GamePauseMutex);

	SDL_WaitThread(GameThread, &status);
	GameThread = nullptr;
}

void EngineClose()
{
	WaitForGameThread();

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

	if (NextResolutionMutex)
	{
		SDL_DestroyMutex(NextResolutionMutex);
		NextResolutionMutex = nullptr;
	}

	g_Platform->Shutdown();
	SDL_Quit();
	ShutdownTENLog();
}
