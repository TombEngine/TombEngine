@echo off
echo ============================================================
echo  TombEngine - Generate Visual Studio 2022 Solution
echo ============================================================
echo.

where cmake >nul 2>&1
if errorlevel 1 (
    echo ERROR: CMake not found.
    echo.
    echo Download and install CMake from: https://cmake.org/download/
    echo Make sure to check "Add CMake to PATH" during installation.
    echo After installing, close and reopen this window, then try again.
    echo.
    pause
    exit /b 1
)

if exist "Build\msvc-2022\CMakeCache.txt" (
    echo Updating existing configuration...
) else (
    echo First run: downloading dependencies (this may take a few minutes^)...
)
echo.

cmake --preset win-x64-vs2022
if errorlevel 1 (
    echo.
    echo ERROR: Configuration failed.
    echo.
    echo Possible causes:
    echo   - Visual Studio 2022 is not installed.
    echo   - The "Desktop development with C++" workload is missing.
    echo   - No internet connection (needed to download dependencies^).
    echo.
    pause
    exit /b 1
)

echo.
echo ============================================================
echo  Done! Open this file in Visual Studio:
echo.
echo    Build\msvc-2022\TombEngine.sln
echo.
echo  Select Debug^|x64 or Release^|x64, then build (Ctrl+Shift+B).
echo ============================================================
echo.
pause
