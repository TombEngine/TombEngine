@echo off
rem Generates CMakeLists.txt from TombEngine\TombEngine.vcxproj. The Visual Studio project stays the reference
rem build, so run this again after changing it.

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Tools\CMake\GenerateCMakeLists.ps1" %*
if errorlevel 1 exit /b 1