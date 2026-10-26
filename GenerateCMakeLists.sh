#!/bin/sh
# Generates CMakeLists.txt from TombEngine/TombEngine.vcxproj with PowerShell 7. The Visual Studio project stays the
# reference build, so run this again after changing it.

exec pwsh -NoProfile -File "$(dirname "$0")/Tools/CMake/GenerateCMakeLists.ps1" "$@"
