#include "framework.h"
#include "Specific/Platform/WindowsSubsystem.h"

#ifdef SDL_PLATFORM_WIN32

#include <DbgHelp.h>
#include <shellapi.h>
#include <sstream>
#include <iostream>
#include <commctrl.h>

#include "Game/control/control.h"
#include "Specific/trutils.h"

extern "C"
{
	__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

namespace TEN::Platform
{
	// Free function to show error via external crashmsg.exe.
	// Must be a free function (not an instance method) because it is called
	// from the static SEH handler where no object instance is available.
	static void ShowExternalMessageBox(const std::string& text)
	{
		// Try to locate error message utility resource.
		HRSRC res = FindResource(nullptr, MAKEINTRESOURCE(IDR_CRASHMSG), "EXE");
		if (!res)
			return;

		// Load executable, if found.
		auto resData = LoadResource(nullptr, res);
		if (resData == nullptr)
			return;

		// Lock executable resource to get data pointer.
		void* resPtr = LockResource(resData);
		if (resPtr == nullptr)
			return;

		const auto exePath = std::string("crashmsg.exe");

		// Write executable to disk.
		auto handleFile = CreateFileA(exePath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (handleFile == INVALID_HANDLE_VALUE)
			return;

		DWORD written = 0;
		WriteFile(handleFile, resPtr, SizeofResource(nullptr, res), &written, nullptr);
		CloseHandle(handleFile);

		STARTUPINFOA startupInfo = { sizeof(startupInfo) };
		auto processInfo = PROCESS_INFORMATION{};

		// Execute the error message utility with the provided text.
		auto cmdLine = std::string("\"" + exePath + "\" " + "\"" + text + "\"");
		if (CreateProcessA(nullptr, cmdLine.data(), nullptr, nullptr, false, 0, nullptr, nullptr, &startupInfo, &processInfo))
		{
			WaitForSingleObject(processInfo.hProcess, INFINITE);
			CloseHandle(processInfo.hProcess);
			CloseHandle(processInfo.hThread);
		}

		// Clean up error message utility afterwards.
		DeleteFileA(exePath.c_str());
	}

	static LONG WINAPI HandleException(EXCEPTION_POINTERS* exceptionInfo)
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
		auto process = GetCurrentProcess();
		SymInitialize(process, nullptr, TRUE);

		DWORD64 address = (DWORD64)exceptionInfo->ExceptionRecord->ExceptionAddress;
		char symbolBuffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME];
		PSYMBOL_INFO symbol = reinterpret_cast<PSYMBOL_INFO>(symbolBuffer);
		symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
		symbol->MaxNameLen = MAX_SYM_NAME;

		// Display function name if debug symbols are available, otherwise display address only.
		auto oss = std::ostringstream();
		if (SymFromAddr(process, address, 0, symbol))
		{
			oss << "address " << symbol->Name << " (0x" << std::hex << address << ")";
		}
		else
		{
			oss << "address 0x" << std::hex << address;
		}

		auto errorMessage = "Unhandled exception: " + std::string(codeName) + " at " + oss.str() + ".";

		// Log the exception.
		TENLog(errorMessage, LogLevel::Error);

		// Print stack trace if engine is in debug mode.
		if (DebugMode)
		{
			oss = std::ostringstream{};
			oss << "Stack trace:\n";

			CONTEXT ctx = *exceptionInfo->ContextRecord;
			STACKFRAME64 stack = {};

			stack.AddrPC.Mode =
			stack.AddrFrame.Mode =
			stack.AddrStack.Mode = AddrModeFlat;

#if PLATFORM_64BIT
			DWORD machineType = IMAGE_FILE_MACHINE_AMD64;
			stack.AddrPC.Offset = ctx.Rip;
			stack.AddrFrame.Offset = ctx.Rsp;
			stack.AddrStack.Offset = ctx.Rsp;
#else
			DWORD machineType = IMAGE_FILE_MACHINE_I386;
			stack.AddrPC.Offset = ctx.Eip;
			stack.AddrFrame.Offset = ctx.Ebp;
			stack.AddrStack.Offset = ctx.Esp;
#endif

			auto thread = GetCurrentThread();

			constexpr int STACK_DEPTH = 32;
			for (int frame = 0; frame < STACK_DEPTH; ++frame)
			{
				if (!StackWalk64(machineType, process, thread, &stack, &ctx, nullptr,
					SymFunctionTableAccess64, SymGetModuleBase64, nullptr))
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

		// Set engine to debug mode to prevent losing focus in fullscreen mode, then show error message.
		DebugMode = true;
		ShowExternalMessageBox(errorMessage);

		// Instruct Windows to terminate process after handling exception.
		return EXCEPTION_EXECUTE_HANDLER;
	}

	WindowsSubsystem::WindowsSubsystem()
	{

	}

	WindowsSubsystem::~WindowsSubsystem()
	{
		// Nothing special to do here yet. If Windows-specific resources are allocated later, they should be released here.
	}

	void WindowsSubsystem::Initialize()
	{
		constexpr unsigned int PROCESS_SYSTEM_DPI_AWARE = 1;

		typedef HRESULT(WINAPI* SetDpiAwarenessProc)(UINT);

		_hInstance = GetModuleHandle(nullptr);

		auto commCtrlInit = INITCOMMONCONTROLSEX{};
		commCtrlInit.dwSize = sizeof(INITCOMMONCONTROLSEX);
		commCtrlInit.dwICC = ICC_USEREX_CLASSES | ICC_STANDARD_CLASSES;
		InitCommonControlsEx(&commCtrlInit);

		// Don't use SHCore library directly as it's not available on pre-Windows 8.1 systems.

		auto lib = LoadLibrary("SHCore.dll");
		if (lib != nullptr)
		{
			auto setDpiAwareness = (SetDpiAwarenessProc)GetProcAddress(lib, "SetProcessDpiAwareness");
			if (setDpiAwareness != nullptr)
				setDpiAwareness(PROCESS_SYSTEM_DPI_AWARE);

			FreeLibrary(lib);
		}

		CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	}

	void WindowsSubsystem::CheckVcRedist()
	{
		// Registry path where the VC++ 2015�2022 runtime stores its version info.
		const char* redistKey =
#if PLATFORM_64BIT
			R"(SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64)";
#else
			R"(SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x86)";
#endif

		HKEY hKey;
		auto result = RegOpenKeyExA(HKEY_LOCAL_MACHINE, redistKey, 0, KEY_READ, &hKey);
		if (result == ERROR_SUCCESS)
		{
			DWORD majorVersion = 0;
			DWORD minorVersion = 0;
			DWORD dataSize = sizeof(DWORD);

			bool okMajor = RegQueryValueExA(hKey, "Major", nullptr, nullptr, reinterpret_cast<LPBYTE>(&majorVersion), &dataSize) == ERROR_SUCCESS;
			bool okMinor = RegQueryValueExA(hKey, "Minor", nullptr, nullptr, reinterpret_cast<LPBYTE>(&minorVersion), &dataSize) == ERROR_SUCCESS;
			RegCloseKey(hKey);

			// Minimum runtime v14.40 is required. Also ensure that vcruntime140.dll can actually be loaded.
			if (okMajor && okMinor && majorVersion >= 14 && minorVersion >= 40)
			{
				auto hModule = LoadLibraryW(L"vcruntime140.dll");
				if (hModule != nullptr)
				{
					FreeLibrary(hModule);
					return; // Runtime is installed correctly; nothing else to do.
				}
			}
		}

		// If reached this point, either key was not found or version/dll is not acceptable. Prompt user to install it.
		const char* redistUrl =
#if PLATFORM_64BIT
			R"(https://aka.ms/vs/17/release/vc_redist.x64.exe)";
#else
			R"(https://aka.ms/vs/17/release/vc_redist.x86.exe)";
#endif

		const char* message =
			"Tomb Engine requires the Microsoft Visual C++ 2015-2022 Redistributable to be installed.\n"
			"Would you like to download it now?";

		SDL_MessageBoxButtonData buttons[] =
		{
			{ SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Cancel" },
			{ SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "OK" }
		};

		SDL_MessageBoxData msgBoxData = {};
		msgBoxData.flags = SDL_MESSAGEBOX_WARNING;
		msgBoxData.title = "Missing libraries";
		msgBoxData.message = message;
		msgBoxData.numbuttons = 2;
		msgBoxData.buttons = buttons;

		int buttonID = 0;
		SDL_ShowMessageBox(&msgBoxData, &buttonID);

		if (buttonID == 1)
		{
			// Launch default browser to download redistributable.
			auto hResult = ShellExecuteA(
				nullptr,
				"open",
				redistUrl,
				nullptr,
				nullptr,
				SW_SHOWNORMAL);

			if ((intptr_t)hResult <= 32)
			{
				// ShellExecute failed. Show more detailed error.
				auto err = std::string("Failed to start browser to download runtimes. Error code: ");
				err += std::to_string(static_cast<long>(reinterpret_cast<intptr_t>(hResult)));

				SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", err.c_str(), nullptr);
			}
		}
	}

	void WindowsSubsystem::InstallExceptionFilter()
	{
		SetUnhandledExceptionFilter(HandleException);
	}

	void WindowsSubsystem::CheckPrerequisites()
	{
		CheckVcRedist();
	}

	void WindowsSubsystem::InstallCrashHandler()
	{
		InstallExceptionFilter();
	}

	void WindowsSubsystem::ShowErrorMessage(const std::string& msg)
	{
		ShowExternalMessageBox(msg);
	}

	void WindowsSubsystem::Tick()
	{
		// No periodic Windows-specific work is required at the moment.
		// This method exists as a hook for future functionality
		// (e.g. console polling, OS integration, etc.).
	}

	void WindowsSubsystem::Shutdown()
	{
		CoUninitialize();
	}

	std::wstring WindowsSubsystem::GetBinaryPath(bool includeExeName)
	{
		const char* basePath = SDL_GetBasePath();
		if (basePath == nullptr)
		{
			TENLog("Can't get current assembly path", LogLevel::Error);
			return std::wstring();
		}

		auto dirPath = std::filesystem::path(basePath);
		auto result = dirPath.wstring();
		std::replace(result.begin(), result.end(), L'\\', L'/');

		if (!includeExeName)
			return result;

		// Append executable filename for the rare callers that need it.
		static const int MAX_PATH_LENGTH = 1024;
		wchar_t fileName[MAX_PATH_LENGTH] = {};

		if (!GetModuleFileNameW(nullptr, fileName, MAX_PATH_LENGTH))
			return result;

		auto exeName = std::filesystem::path(fileName).filename().wstring();
		return result + exeName;
	}

	std::vector<unsigned short> WindowsSubsystem::GetProductOrFileVersion(bool productVersion)
	{
		auto fileName = GetBinaryPath(true);

		DWORD dummy = 0;
		DWORD size = GetFileVersionInfoSizeW(fileName.data(), &dummy);

		if (size == 0)
		{
			TENLog("GetFileVersionInfoSizeW failed.", LogLevel::Error);
			return {};
		}

		auto buffer = std::unique_ptr<unsigned char[]>(new unsigned char[size]);

		// Load version info.
		if (!GetFileVersionInfoW(fileName.data(), 0, size, buffer.get()))
		{
			TENLog("GetFileVersionInfoW failed.", LogLevel::Error);
			return {};
		}

		VS_FIXEDFILEINFO* info = nullptr;
		unsigned int infoSize = 0;

		if (!VerQueryValueW(buffer.get(), L"\\", (void**)&info, &infoSize))
		{
			TENLog("VerQueryValueW failed.", LogLevel::Error);
			return {};
		}

		if (infoSize != sizeof(VS_FIXEDFILEINFO))
		{
			TENLog("VerQueryValueW returned wrong size for VS_FIXEDFILEINFO.", LogLevel::Error);
			return {};
		}

		if (productVersion)
		{
			return
			{
				HIWORD(info->dwProductVersionMS),
				LOWORD(info->dwProductVersionMS),
				HIWORD(info->dwProductVersionLS),
				LOWORD(info->dwProductVersionLS)
			};
		}
		else
		{
			return
			{
				HIWORD(info->dwFileVersionMS),
				LOWORD(info->dwFileVersionMS),
				HIWORD(info->dwFileVersionLS),
				LOWORD(info->dwFileVersionLS)
			};
		}
	}

	std::unique_ptr<ISubsystem> CreatePlatformSubsystem()
	{
		// On Windows, return concrete WindowsSubsystem.
		return std::make_unique<WindowsSubsystem>();
	}

	void WindowsSubsystem::SetSDL3Window(SDL_Window* window)
	{
		_window = window;
	}

	SDL_Window* WindowsSubsystem::GetSDL3Window()
	{
		return _window;
	}

	void WindowsSubsystem::InitialiseAudioCodecs()
	{
		// HACK: Manually force-load ADPCM codec, because on Win11 systems
		// it may suddenly unload otherwise. BASS supports MSADPCM natively
		// on all platforms, but on Windows it relies on the system ACM codec.
		_adpcmLibrary = LoadLibrary("msadp32.acm");
	}

	void WindowsSubsystem::ReleaseAudioCodecs()
	{
		if (_adpcmLibrary != nullptr)
		{
			FreeLibrary(_adpcmLibrary);
			_adpcmLibrary = nullptr;
		}
	}

	void WindowsSubsystem::ConfigureConsole()
	{
		// Set console to UTF-8 mode for proper Unicode character display.
		SetConsoleOutputCP(CP_UTF8);
		SetConsoleCP(CP_UTF8);
	}

	void WindowsSubsystem::HideConsole()
	{
		FreeConsole();
	}

	bool WindowsSubsystem::CreateDummyTitleLevel(const std::string& levelPath)
	{
		// Try loading embedded resource "data.bin"
		HRSRC hResource = FindResource(nullptr, MAKEINTRESOURCE(IDR_TITLELEVEL), "BIN");
		if (hResource == nullptr)
		{
			TENLog("Embedded title level file not found.", LogLevel::Error);
			return false;
		}

		// Load resource into memory.
		auto hGlobal = LoadResource(nullptr, hResource);
		if (hGlobal == nullptr)
		{
			TENLog("Failed to load embedded title level file.", LogLevel::Error);
			return false;
		}

		// Lock resource to get data pointer.
		void* pData = LockResource(hGlobal);
		DWORD dwSize = SizeofResource(nullptr, hResource);

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
}

#endif
