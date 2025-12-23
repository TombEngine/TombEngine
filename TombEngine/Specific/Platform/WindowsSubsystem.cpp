#include "framework.h"
#include "Specific/Platform/WindowsSubsystem.h"

#ifdef SDL_PLATFORM_WIN32

#include <DbgHelp.h>
#include <shellapi.h>
#include <sstream>
#include <iostream>
#include <commctrl.h>

#include "Specific/trutils.h"

extern "C"
{
	__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

namespace TEN::Platform
{
	static LONG WINAPI HandleException(EXCEPTION_POINTERS* exceptionInfo)
	{
		DWORD code = exceptionInfo->ExceptionRecord->ExceptionCode;
		const char* codeName = "Unknown exception.";

		// Translate common exception codes to human-readable text.
		switch (code)
		{
		case EXCEPTION_ACCESS_VIOLATION:
			codeName = "Access violation.";
			break;

		case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
			codeName = "Array out of bounds.";
			break;

		case EXCEPTION_BREAKPOINT:
			codeName = "Breakpoint encountered.";
			break;

		case EXCEPTION_DATATYPE_MISALIGNMENT:
			codeName = "Data type misalignment.";
			break;

		case EXCEPTION_FLT_DIVIDE_BY_ZERO:
			codeName = "Floating-point division by zero.";
			break;

		case EXCEPTION_FLT_OVERFLOW:
			codeName = "Floating-point overflow.";
			break;

		case EXCEPTION_ILLEGAL_INSTRUCTION:
			codeName = "Illegal instruction.";
			break;

		case EXCEPTION_IN_PAGE_ERROR:
			codeName = "Exception in page error.";
			break;

		case EXCEPTION_INT_DIVIDE_BY_ZERO:
			codeName = "Integer division by zero.";
			break;

		case EXCEPTION_INT_OVERFLOW:
			codeName = "Integer overflow.";
			break;

		case EXCEPTION_INVALID_DISPOSITION:
			codeName = "Invalid disposition.";
			break;

		case EXCEPTION_NONCONTINUABLE_EXCEPTION:
			codeName = "Non-continuable exception.";
			break;

		case EXCEPTION_PRIV_INSTRUCTION:
			codeName = "Privileged instruction.";
			break;

		case EXCEPTION_SINGLE_STEP:
			codeName = "Single-step exception.";
			break;

		case EXCEPTION_STACK_OVERFLOW:
			codeName = "Stack overflow.";
			break;

		default:
			break;
		}

		// Initialize symbol handler to resolve function name addresses.
		auto process = GetCurrentProcess();
		SymInitialize(process, nullptr, true);

		DWORD64 address = (DWORD64)exceptionInfo->ExceptionRecord->ExceptionAddress;
		char symbolBuffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME];
		PSYMBOL_INFO symbol = reinterpret_cast<PSYMBOL_INFO>(symbolBuffer);
		symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
		symbol->MaxNameLen = MAX_SYM_NAME;

		// Build string describing where crash happened.
		auto oss = std::ostringstream();
		if (SymFromAddr(process, address, 0, symbol))
		{
			// If debug symbols are available, show function name + address.
			oss << "address " << symbol->Name << " (0x" << std::hex << address << ")";
		}
		else
		{
			// FALLBACK: Only show raw address if a symbol cannot be resolved.
			oss << "address 0x" << std::hex << address;
		}

		auto errorMsg = std::string("Unhandled exception: ");
		errorMsg += codeName;
		errorMsg += " at ";
		errorMsg += oss.str();
		errorMsg += ".";

		TENLog(errorMsg, LogLevel::Error);

		// Show blocking message box to user.
		MessageBoxA(
			nullptr,
			errorMsg.c_str(),
			"TombEngine - Crash",
			MB_ICONERROR | MB_OK);

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
		if (lib == nullptr)
			return;

		auto setDpiAwareness = (SetDpiAwarenessProc)GetProcAddress(lib, "SetProcessDpiAwareness");
		if (setDpiAwareness == nullptr)
			return;

		setDpiAwareness(PROCESS_SYSTEM_DPI_AWARE);
		FreeLibrary(lib);

		CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	}

	void WindowsSubsystem::CheckVcRedist()
	{
		// Registry path where the VC++ 2015–2022 runtime stores its version info.
		const char* redistKey =
#ifdef _WIN64
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
#ifdef _WIN64
			R"(https://aka.ms/vs/17/release/vc_redist.x64.exe)";
#else
			R"(https://aka.ms/vs/17/release/vc_redist.x86.exe)";
#endif

		const char* message =
			"Tomb Engine requires the Microsoft Visual C++ 2015-2022 Redistributable to be installed.\n"
			"Would you like to download it now?";

		int msgBoxResult = MessageBoxA(
			nullptr,
			message,
			"Missing libraries",
			MB_ICONWARNING | MB_OKCANCEL);

		if (msgBoxResult == IDOK)
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

				MessageBoxA(
					nullptr,
					err.c_str(),
					"Error",
					MB_ICONERROR | MB_OK);
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

		// Write executable to a disk.
		auto handleFile = CreateFileA(exePath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (handleFile == INVALID_HANDLE_VALUE)
			return;

		DWORD written = 0;
		WriteFile(handleFile, resPtr, SizeofResource(nullptr, res), &written, nullptr);
		CloseHandle(handleFile);

		// TODO: Weird line. Check if commented one works instead.
		STARTUPINFOA startupInfo = { sizeof(startupInfo) };
		//auto startupInfo = STARTUPINFOA{ sizeof(STARTUPINFOA) };
		auto processInfo = PROCESS_INFORMATION{};

		// Execute the error message utility with the provided text.
		auto cmdLine = std::string("\"" + exePath + "\" " + "\"" + msg + "\"");
		if (CreateProcessA(nullptr, cmdLine.data(), nullptr, nullptr, false, 0, nullptr, nullptr, &startupInfo, &processInfo))
		{
			WaitForSingleObject(processInfo.hProcess, INFINITE);
			CloseHandle(processInfo.hProcess);
			CloseHandle(processInfo.hThread);
		}

		// Clean up error message utility afterwards.
		DeleteFileA(exePath.c_str());
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

	bool WindowsSubsystem::Is64Bit()
	{
#ifdef _WIN64
		return true;
#else
		return false;
#endif
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
