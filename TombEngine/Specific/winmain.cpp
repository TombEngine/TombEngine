#include "framework.h"
#include "Specific/winmain.h"

#include <CommCtrl.h>
#include <DbgHelp.h>
#include <process.h>
#include <iostream>
#include <codecvt>
#include <filesystem>

#include "Game/control/control.h"
#include "Game/savegame.h"
#include "Renderer/Renderer.h"
#include "resource.h"
#include "Sound/sound.h"
#include "Specific/level.h"
#include "Specific/configuration.h"
#include "Specific/Parallel.h"
#include "Specific/trutils.h"
#include "Scripting/Internal/LanguageScript.h"
#include "Scripting/Include/ScriptInterfaceState.h"
#include "Scripting/Include/ScriptInterfaceLevel.h"
#include "Video/Video.h"

using namespace TEN::Renderer;
using namespace TEN::Input;
using namespace TEN::Utils;
using namespace TEN::Video;

WINAPP App;
unsigned int ThreadID, ConsoleThreadID, ThreadSuspendCount;
uintptr_t ThreadHandle, ConsoleThreadHandle;
HACCEL hAccTable;
HWND WindowsHandle;

// Indicates to hybrid graphics systems to prefer discrete part by default.
extern "C"
{
	__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

bool ArgEquals(wchar_t* incomingArg, std::string name)
{
	auto lowerArg = TEN::Utils::ToLower(TEN::Utils::ToString(incomingArg));
	return (lowerArg == "-" + name) || (lowerArg == "/" + name);
}

void CheckIfRedistInstalled()
{
	// Before doing any actions, check if VC redist is installed, because otherwise it can
	// silently crash at any moment. Still allows to run the game in any case, even if user
	// decides not to install redistributables.

	const char* redistKey =
#ifdef _WIN64
		R"(SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64)";
#else
		R"(SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x86)";
#endif

	HKEY hKey;
	LSTATUS result = RegOpenKeyExA(HKEY_LOCAL_MACHINE, redistKey, 0, KEY_READ, &hKey);
	if (result == ERROR_SUCCESS)
	{
		DWORD majorVersion = 0;
		DWORD minorVersion = 0;
		DWORD dataSize = sizeof(DWORD);

		if (RegQueryValueExA(hKey, "Major", NULL, NULL, (LPBYTE)&majorVersion, &dataSize) == ERROR_SUCCESS &&
			RegQueryValueExA(hKey, "Minor", NULL, NULL, (LPBYTE)&minorVersion, &dataSize) == ERROR_SUCCESS)
		{
			RegCloseKey(hKey);

			if (majorVersion >= 14 && minorVersion >= 40)
			{
				HMODULE hModule = LoadLibraryW(L"vcruntime140.dll");
				if (hModule != NULL)
				{
					FreeLibrary(hModule);
					return;
				}
			}
		}
		else
		{
			RegCloseKey(hKey);
		}
	}

	const char* redistUrl =
#ifdef _WIN64
		R"(https://aka.ms/vs/17/release/vc_redist.x64.exe)";
#else
		R"(https://aka.ms/vs/17/release/vc_redist.x86.exe)";
#endif

	const char* message = "TombEngine requires Visual C++ 2015-2022 Redistributable to be installed. Would you like to download it now?";
	int msgBoxResult = MessageBoxA(NULL, message, "Missing libraries", MB_ICONWARNING | MB_OKCANCEL);

	if (msgBoxResult == IDOK)
	{
		HINSTANCE hResult = ShellExecuteA(NULL, "open", redistUrl, NULL, NULL, SW_SHOWNORMAL);

		if ((intptr_t)hResult <= 32)
		{
			MessageBoxA(NULL, (LPCSTR)("Failed to start browser to download runtimes. Error code: " +
				std::to_string((long)(intptr_t)hResult)).c_str(), "Error", MB_ICONERROR | MB_OK);
		}
	}
}

Vector2i GetScreenResolution()
{
	RECT desktop;
	const HWND hDesktop = GetDesktopWindow();
	GetWindowRect(hDesktop, &desktop);
	Vector2i resolution;
	resolution.x = desktop.right;
	resolution.y = desktop.bottom;
	return resolution;
}

int GetCurrentScreenRefreshRate()
{
	DEVMODE devmode;
	memset(&devmode, 0, sizeof(devmode));
	devmode.dmSize = sizeof(devmode);
	
	if (EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &devmode))
	{
		return devmode.dmDisplayFrequency;
	}
	else
	{
		return 0;
	}
}

std::vector<Vector2i> GetAllSupportedScreenResolutions()
{
	auto resList = std::vector<Vector2i>{};

	DEVMODE dm = { 0 };
	dm.dmSize = sizeof(dm);
	for (int iModeNum = 0; EnumDisplaySettings(NULL, iModeNum, &dm) != 0; iModeNum++)
	{
		bool add = true;
		for (auto m : resList)
		{
			if (m.x == dm.dmPelsWidth && m.y == dm.dmPelsHeight)
			{
				add = false;
				break;
			}
		}
		if (add)
		{
			auto res = Vector2i(dm.dmPelsWidth, dm.dmPelsHeight);
			resList.push_back(res);
		}
	}

	std::sort(
		resList.begin(), resList.end(),
		[](Vector2i& a, Vector2i& b)
		{
			return ((a.x == b.x) ? (a.y < b.y) : (a.x < b.x));
		});

	return resList;
}

void DisableDpiAwareness()
{
	// Don't use SHCore library directly, as it's not available on pre-win 8.1 systems.

	typedef HRESULT(WINAPI* SetDpiAwarenessProc)(UINT);
	static constexpr unsigned int PROCESS_SYSTEM_DPI_AWARE = 1;

	auto lib = LoadLibrary("SHCore.dll");
	if (lib == NULL)
		return;

	auto setDpiAwareness = (SetDpiAwarenessProc)GetProcAddress(lib, "SetProcessDpiAwareness");
	if (setDpiAwareness == NULL)
		return;

	setDpiAwareness(PROCESS_SYSTEM_DPI_AWARE);
	FreeLibrary(lib);
}

bool GenerateDummyLevel(const std::string& levelPath)
{
	// Try loading embedded resource "data.bin"
	HRSRC hResource = FindResource(NULL, MAKEINTRESOURCE(IDR_TITLELEVEL), "BIN");
	if (hResource == NULL)
	{
		TENLog("Embedded title level file not found.", LogLevel::Error);
		return false;
	}

	// Load resource into memory.
	HGLOBAL hGlobal = LoadResource(NULL, hResource);
	if (hGlobal == NULL)
	{
		TENLog("Failed to load embedded title level file.", LogLevel::Error);
		return false;
	}

	// Lock resource to get data pointer.
	void* pData = LockResource(hGlobal);
	DWORD dwSize = SizeofResource(NULL, hResource);

	// Write resource data to file.
	try
	{
		auto dir = std::filesystem::path(levelPath).parent_path();
		if (!dir.empty())
			std::filesystem::create_directories(dir);

		auto outFile = std::ofstream(levelPath, std::ios::binary);
		if (!outFile)
			throw std::ios_base::failure("Failed to create title level file.");

		outFile.write(reinterpret_cast<const char*>(pData), dwSize);
		if (!outFile)
			throw std::ios_base::failure("Failed to write to title level file.");

		outFile.close();
	}
	catch (const std::exception& ex)
	{
		TENLog("Error while generating title level file: " + std::string(ex.what()), LogLevel::Error);
		return false;
	}

	return true;
}

unsigned CALLBACK ConsoleInput(void*)
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

	return true;
}

void ShowExternalMessageBox(const std::string& text)
{
	// Try to locate error message utility resource.
	HRSRC res = FindResource(NULL, MAKEINTRESOURCE(IDR_CRASHMSG), "EXE");
	if (!res)
		return;

	// Load executable, if found.
	HGLOBAL resData = LoadResource(NULL, res);
	if (!resData)
		return;

	// Lock executable resource to get pointer to data.
	void* resPtr = LockResource(resData);
	if (!resPtr)
		return;

	const auto exePath = std::string("crashmsg.exe");

	// Write executable to a disk.
	HANDLE hFile = CreateFileA(exePath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return;

	DWORD written;
	WriteFile(hFile, resPtr, SizeofResource(NULL, res), &written, NULL);
	CloseHandle(hFile);

	STARTUPINFOA si = { sizeof(si) };
	PROCESS_INFORMATION pi;

	// Execute the error message utility with the provided text.
	std::string cmdLine = "\"" + exePath + "\" " + "\"" + text + "\"";
	if (CreateProcessA(NULL, cmdLine.data(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
	{
		WaitForSingleObject(pi.hProcess, INFINITE);
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}

	// Clean up error message utility afterwards.
	DeleteFileA(exePath.c_str());
}

LONG WINAPI HandleException(EXCEPTION_POINTERS* exceptionInfo)
{
	DWORD code = exceptionInfo->ExceptionRecord->ExceptionCode;
	const char* codeName = "Unknown exception";

	// Map exception codes to strings.
	switch (code)
	{
		case EXCEPTION_ACCESS_VIOLATION:         codeName = "Access violation"; break;
		case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:    codeName = "Array out of bounds"; break;
		case EXCEPTION_BREAKPOINT:               codeName = "Breakpoint encountered"; break;
		case EXCEPTION_DATATYPE_MISALIGNMENT:    codeName = "Data type misalignment"; break;
		case EXCEPTION_FLT_DIVIDE_BY_ZERO:       codeName = "Floating-point division by zero"; break;
		case EXCEPTION_FLT_OVERFLOW:             codeName = "Floating-point overflow"; break;
		case EXCEPTION_ILLEGAL_INSTRUCTION:      codeName = "Illegal instruction"; break;
		case EXCEPTION_IN_PAGE_ERROR:            codeName = "Exception in page error"; break;
		case EXCEPTION_INT_DIVIDE_BY_ZERO:       codeName = "Integer division by zero"; break;
		case EXCEPTION_INT_OVERFLOW:             codeName = "Integer overflow"; break;
		case EXCEPTION_INVALID_DISPOSITION:      codeName = "Invalid disposition"; break;
		case EXCEPTION_NONCONTINUABLE_EXCEPTION: codeName = "Non-continuable exception"; break;
		case EXCEPTION_PRIV_INSTRUCTION:         codeName = "Private instruction exception"; break;
		case EXCEPTION_SINGLE_STEP:              codeName = "Single-step exception"; break;
		case EXCEPTION_STACK_OVERFLOW:           codeName = "Stack overflow"; break;
	}

	// Try to resolve symbol name from address.
	HANDLE process = GetCurrentProcess();
	SymInitialize(process, NULL, TRUE);

	DWORD64 address = (DWORD64)exceptionInfo->ExceptionRecord->ExceptionAddress;
	char symbolBuffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME];
	PSYMBOL_INFO symbol = (PSYMBOL_INFO)symbolBuffer;
	symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
	symbol->MaxNameLen = MAX_SYM_NAME;

	// Display function name, if debug symbols are available, otherwise display address only.
	std::ostringstream oss;
	if (SymFromAddr(process, address, 0, symbol))
	{
		oss << "address " << symbol->Name << " (0x" << std::hex << address << ")";
	}
	else
	{
		oss << "address 0x" << std::hex << address;
	}

	auto errorMessage = "Unhandled exception: " + std::string(codeName) + " at " + oss.str() + ".";

	// Log the exception and show error message.
	TENLog(errorMessage, LogLevel::Error);

	// Optionally print stack trace, if engine is in debug mode.
	if (DebugMode)
	{
		oss = std::ostringstream{};
		oss << "Stack trace:\n";

		CONTEXT ctx = *exceptionInfo->ContextRecord;
		STACKFRAME64 stack = {};

		stack.AddrPC.Mode =
		stack.AddrFrame.Mode =
		stack.AddrStack.Mode = AddrModeFlat;

#ifdef _M_IX86
		DWORD machineType = IMAGE_FILE_MACHINE_I386;
		stack.AddrPC.Offset = ctx.Eip;
		stack.AddrFrame.Offset = ctx.Ebp;
		stack.AddrStack.Offset = ctx.Esp;
#elif _M_X64
		DWORD machineType = IMAGE_FILE_MACHINE_AMD64;
		stack.AddrPC.Offset = ctx.Rip;
		stack.AddrFrame.Offset = ctx.Rsp;
		stack.AddrStack.Offset = ctx.Rsp;
#endif

		HANDLE thread = GetCurrentThread();

		constexpr int STACK_DEPTH = 32;
		for (int frame = 0; frame < STACK_DEPTH; ++frame)
		{
			if (!StackWalk64(machineType, process, thread, &stack, &ctx, NULL,
				SymFunctionTableAccess64, SymGetModuleBase64, NULL))
				break;

			if (stack.AddrPC.Offset == 0)
				break;

			if (SymFromAddr(process, stack.AddrPC.Offset, 0, symbol))
				oss << "  [" << frame << "] " << symbol->Name << " (0x" << std::hex << stack.AddrPC.Offset << ")\n";
			else
				oss << "  [" << frame << "] " << "0x" << std::hex << stack.AddrPC.Offset << "\n";
		}

		TENLog(oss.str(), LogLevel::Error);
	}

	// Set engine to debug mode to prevent losing focus in fullscreen mode and show error message.
	DebugMode = true;
	ShowExternalMessageBox(errorMessage);

	return EXCEPTION_EXECUTE_HANDLER;
}

void WinProcMsg()
{
	MSG msg;

	do
	{
		GetMessage(&msg, 0, 0, 0);
		if (!TranslateAccelerator(WindowsHandle, hAccTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	while (!ThreadEnded && msg.message != WM_QUIT);
}

void CALLBACK HandleWmCommand(unsigned short wParam)
{
	if (wParam == WM_KILLFOCUS)
	{
		// make sure we suspend the game (if focus is removed) only if the level is not being loaded
		
		if (!LevelLoadTask.valid())
		{
			SuspendThread((HANDLE)ThreadHandle);
			g_Renderer.ToggleFullScreen();
			ResumeThread((HANDLE)ThreadHandle);
		}
	}
}

LRESULT CALLBACK WinAppProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static bool receivedWmClose = false;

	// Disables ALT + SPACE
	if (msg == WM_SYSCOMMAND && wParam == SC_KEYMENU)
	{
		return 0;
	}
	
	if (msg == WM_SETCURSOR)
	{
		if (LOWORD(lParam) == HTCLIENT)
		{
			SetCursor(g_Renderer.IsFullsScreen() ? nullptr : App.WindowClass.hCursor);
			return 1;
		}
	}

	if (msg == WM_ACTIVATEAPP)
	{
		App.ResetClock = true;
		return DefWindowProcA(hWnd, msg, wParam, (LPARAM)lParam);
	}

	if (msg > WM_CLOSE)
	{
		if (msg == WM_COMMAND)
			HandleWmCommand((unsigned short)wParam);

		return DefWindowProcA(hWnd, msg, wParam, (LPARAM)lParam);
	}

	if (msg == WM_CLOSE)
	{
		receivedWmClose = true;
		PostQuitMessage(0);
		DoTheGame = false;

		return DefWindowProcA(hWnd, 0x10u, wParam, (LPARAM)lParam);
	}

	if (msg != WM_ACTIVATE)
		return DefWindowProcA(hWnd, msg, wParam, (LPARAM)lParam);

	if (receivedWmClose)
	{
		return DefWindowProcA(hWnd, msg, wParam, (LPARAM)lParam);
	}

	if ((short)wParam)
	{
		if ((signed int)(unsigned short)wParam > 0 && (signed int)(unsigned short)wParam <= 2)
		{
			SetInputLockState(false);

			if (!g_Configuration.EnableWindowedMode)
				g_Renderer.ToggleFullScreen(true);

			if (ThreadHandle > 0 && ThreadSuspendCount > 0)
			{
				TENLog("Resuming game thread", LogLevel::Info);

				if (!g_VideoPlayer.Resume())
					ResumeAllSounds(SoundPauseMode::Global);

				ResumeThread((HANDLE)ThreadHandle);
				ThreadSuspendCount--;
			}

			return 0;
		}
	}
	else
	{
		SetInputLockState(true);

		if (!g_Configuration.EnableWindowedMode)
			ShowWindow(hWnd, SW_MINIMIZE);

		if ((!DebugMode || IsIconic(hWnd)) && ThreadSuspendCount == 0)
		{
			TENLog("Suspending game thread", LogLevel::Info);

			if (!g_VideoPlayer.Pause())
				PauseAllSounds(SoundPauseMode::Global);

			SuspendThread((HANDLE)ThreadHandle);
			ThreadSuspendCount++;
		}
	}

	return 0;
}

int main() 
{
	return WinMain(GetModuleHandle(nullptr), nullptr, GetCommandLine(), SW_SHOW);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	CheckIfRedistInstalled();

	// Process command line arguments.
	bool setup = false;
	std::string levelFile = {};
	LPWSTR* argv;
	int argc;
	argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	std::string gameDir{};

	// Parse command line arguments.
	for (int i = 1; i < argc; i++)
	{
		if (ArgEquals(argv[i], "setup"))
		{
			setup = true;
		}
		else if (ArgEquals(argv[i], "debug"))
		{
			DebugMode = true;
		}
		else if (ArgEquals(argv[i], "level") && argc > (i + 1))
		{
			levelFile = TEN::Utils::ToString(argv[i + 1]);
		}
		else if (ArgEquals(argv[i], "hash") && argc > (i + 1))
		{
			SystemNameHash = std::stoul(std::wstring(argv[i + 1]));
		}
		else if (ArgEquals(argv[i], "gamedir") && argc > (i + 1))
		{
			gameDir = TEN::Utils::ToString(argv[i + 1]);
		}
	}
	LocalFree(argv);

	// Construct asset directory.
	gameDir = ConstructAssetDirectory(gameDir);

	// Hide console window if mode isn't debug.
#if !_DEBUG
	if (!DebugMode)
	{
		FreeConsole();
	}
	else
#endif
	{
		ConsoleThreadHandle = BeginThread(ConsoleInput, ConsoleThreadID);
	}

	// Clear application structure.
	memset(&App, 0, sizeof(WINAPP));
	
	// Initialize logging.
	InitTENLog(gameDir);
	SetUnhandledExceptionFilter(HandleException);

	auto windowName = std::string("Starting TombEngine");

	// Indicate version.
	auto ver = GetProductOrFileVersion(false);

	if (ver.size() == 4)
	{
		windowName = windowName + " version " +
					 std::to_string(ver[0]) + "." +
					 std::to_string(ver[1]) + "." +
					 std::to_string(ver[2]) + "." +
					 std::to_string(ver[3]);
	}

#ifdef _WIN64
	windowName = windowName + " (64-bit)";
#else
	windowName = windowName + " (32-bit)";
#endif

	TENLog(windowName, LogLevel::Info);

	// Initialize savegame and scripting systems.
	SaveGame::Init(gameDir);
	ScriptInterfaceState::Init(gameDir);

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

		//todo Major hack. This should not be needed to leak outside of
		//LogicHandler internals. In a future version stuff from FlowHandler
		//should be moved to LogicHandler or vice versa to make this stuff
		//less fragile (squidshire, 16/09/22)
		g_GameScript->ShortenTENCalls();
		g_GameFlow->SetGameDir(gameDir);
		g_GameFlow->LoadFlowScript();
	}
	catch (TENScriptException const& e)
	{
		auto errorMessage = std::string{ "A Lua error occurred while setting up scripts; " } + __func__ + ": " + e.what();
		TENLog(errorMessage, LogLevel::Error, LogConfig::All);
		ShowExternalMessageBox(errorMessage);
		ShutdownTENLog();
		return 0;
	}

	// Disable DPI scaling on Windows 8.1+ systems.
	DisableDpiAwareness();

	// Set up main window.
	INITCOMMONCONTROLSEX commCtrlInit;
	commCtrlInit.dwSize = sizeof(INITCOMMONCONTROLSEX);
	commCtrlInit.dwICC = ICC_USEREX_CLASSES | ICC_STANDARD_CLASSES;
	InitCommonControlsEx(&commCtrlInit);

	// Initialize main window.
	App.hInstance = hInstance;
	App.WindowClass.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
	App.WindowClass.lpszMenuName = NULL;
	App.WindowClass.lpszClassName = "TombEngine";
	App.WindowClass.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
	App.WindowClass.hInstance = hInstance;
	App.WindowClass.style = CS_VREDRAW | CS_HREDRAW;
	App.WindowClass.lpfnWndProc = WinAppProc;
	App.WindowClass.cbClsExtra = 0;
	App.WindowClass.cbWndExtra = 0;
	App.WindowClass.hCursor = LoadCursorA(NULL, IDC_ARROW);

	// Register main window.
	if (!RegisterClass(&App.WindowClass))
	{
		TENLog("Unable to register window class.", LogLevel::Error);
		return 0;
	}

	// Create renderer and enumerate adapters and video modes.
	g_Renderer.Create();

	// Initialize key bindings.
	g_Bindings.Initialize();

	// Load configuration and optionally show setup dialog.
	InitDefaultConfiguration();
	if (setup || !LoadConfiguration())
	{
		if (!SetupDialog())
		{
			WinClose();
			return 0;
		}

		LoadConfiguration();
	}

	// Set up window dimensions.
	RECT rect;
	rect.left = 0;
	rect.top = 0;
	rect.right = g_Configuration.ScreenWidth;
	rect.bottom = g_Configuration.ScreenHeight;
	AdjustWindowRect(&rect, WS_CAPTION, false);

	// Calculate window resolution.
	auto windowRes = Vector2i(rect.right - rect.left, rect.bottom - rect.top);

	// Get screen resolution of primary monitor.
	auto screenRes = Vector2i(GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));

	// Calculate centered window position on screen.
	auto windowPos = (screenRes - windowRes) / 2;

	// Create window handle.
	App.WindowHandle = CreateWindowEx(
		0,
		"TombEngine",
		g_GameFlow->GetString(STRING_WINDOW_TITLE),
		WS_OVERLAPPED | WS_CAPTION | WS_MINIMIZEBOX,
		windowPos.x,
		windowPos.y,
		windowRes.x,
		windowRes.y,
		NULL,
		NULL,
		App.hInstance,
		NULL);

	// Register window handle.
	if (!App.WindowHandle)
	{
		TENLog("Unable to create Window. Error: " + std::to_string(GetLastError()), LogLevel::Error);
		return 0;
	}
	else
	{
		WindowsHandle = App.WindowHandle;
	}

	try
	{
		// Unlike CoInitialize(), this line prevents event spamming if a .dll fails.
		CoInitializeEx(NULL, COINIT_MULTITHREADED);

		// Initialize audio (should be called prior to initializing renderer, because video handler needs it).
		Sound_Init(gameDir);

		// Initialize renderer.
		g_Renderer.Initialize(gameDir, g_Configuration.ScreenWidth, g_Configuration.ScreenHeight, g_Configuration.EnableWindowedMode, App.WindowHandle);

		// Initialize input.
		InitializeInput(App.WindowHandle);

		// Load level if specified in command line.
		CurrentLevel = g_GameFlow->GetLevelNumber(levelFile);

		App.bNoFocus = false;
		App.isInScene = false;

		UpdateWindow(WindowsHandle);
		ShowWindow(WindowsHandle, nShowCmd);

		hAccTable = LoadAccelerators(hInstance, (LPCSTR)0x65);
	}
	catch (std::exception& ex)
	{
		auto errorMessage = "Error during game initialization: " + std::string(ex.what());
		TENLog(errorMessage, LogLevel::Error);
		ShowExternalMessageBox(errorMessage);
		WinClose();
		exit(EXIT_FAILURE);
	}

	DoTheGame = true;

	g_Parallel.Initialize();
	ThreadEnded = false;
	ThreadSuspendCount = 0;
	ThreadHandle = BeginThread(GameMain, ThreadID);

	// The game window likes to steal input anyway, so let's put it at the
	// foreground so the user at least expects it.
	if (GetForegroundWindow() != WindowsHandle)
		SetForegroundWindow(WindowsHandle);

	WinProcMsg();
	ThreadEnded = true;

	while (DoTheGame);

	TENLog("Cleaning up and exiting...", LogLevel::Info);

	WinClose();
	exit(EXIT_SUCCESS);
}

void WinClose()
{
	if (ConsoleThreadHandle)
		CloseHandle((HANDLE)ConsoleThreadHandle);

	WaitForSingleObject((HANDLE)ThreadHandle, 5000);
	DestroyAcceleratorTable(hAccTable);
	ShutdownTENLog();
	CoUninitialize();
}
