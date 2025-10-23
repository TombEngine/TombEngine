#pragma once
#pragma comment(linker,"/manifestdependency:\"" \
    "type='win32' " \
    "name='Microsoft.Windows.Common-Controls' " \
    "version='6.0.0.0' " \
    "processorArchitecture='*' "  \
    "publicKeyToken='6595b64144ccf1df' " \
    "language='*'\"")

#include <process.h>
#include <vector>

#include "Math/Math.h"

using namespace TEN::Math;

struct WINAPP
{
    HINSTANCE hInstance;
    int nFillMode;
    WNDCLASS WindowClass;
    HWND WindowHandle;
    bool bNoFocus;
    bool isInScene;
    bool ResetClock;
};

extern HWND WindowsHandle;
extern WINAPP App;

// return handle
#define BeginThread(function, threadid) _beginthreadex(0, 0, &function, 0, 0, &threadid)
#define EndThread() _endthreadex(1)

void WinProcMsg();
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd);
void WinClose();
LRESULT CALLBACK WinAppProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
void CALLBACK HandleWmCommand(unsigned short wParam);

Vector2i GetScreenResolution();
std::vector<Vector2i> GetAllSupportedScreenResolutions();
int GetCurrentScreenRefreshRate();

bool GenerateDummyLevel(const std::string& levelPath);
