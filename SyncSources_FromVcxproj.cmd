@echo off
echo ============================================================
echo  TombEngine - Sync Source Files (vcxproj -> CMake)
echo ============================================================
echo.
echo  Use this after adding or removing .cpp/.h files in
echo  Visual Studio to update the CMake source list.
echo.

where python >nul 2>&1
if errorlevel 1 (
    echo ERROR: Python not found.
    echo.
    echo Download and install Python from: https://www.python.org/downloads/
    echo Make sure to check "Add Python to PATH" during installation.
    echo After installing, close and reopen this window, then try again.
    echo.
    pause
    exit /b 1
)

python Tools\sync_vcxproj.py --from-vcxproj
if errorlevel 1 (
    echo.
    echo ERROR: Sync failed. Check the output above for details.
    echo.
    pause
    exit /b 1
)

echo.
echo ============================================================
echo  Done! Sources.cmake has been updated.
echo.
echo  Now re-run GenerateSolution_VS20xx.cmd to refresh
echo  the Visual Studio solution.
echo ============================================================
echo.
pause
