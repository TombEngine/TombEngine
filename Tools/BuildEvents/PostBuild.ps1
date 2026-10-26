# Post-build step of the Visual Studio project and of CMake: removes the debug and link by-products of Release builds
# from the executable folder. Runs with Windows PowerShell and with PowerShell 7 on Linux and macOS.

param(
    [Parameter(Mandatory = $true)][string]$Configuration,
    [string]$Platform = "x64",
    [string]$BuildDir = "",
    [string]$TargetDir = ""
)

$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "BuildCommon.ps1")

if ($Configuration -ne "Release")
{
    exit 0
}

if ($BuildDir -eq "")
{
    $BuildDir = Join-Path (Join-Path (Get-RootDirectory) "Build") $Configuration
}

if ($TargetDir -eq "")
{
    $TargetDir = Get-TargetDirectory $BuildDir $Platform
}

foreach ($pattern in @("*.pdb", "*.lib", "*.exp"))
{
    Remove-Item -Path (Join-Path $TargetDir $pattern) -Force -ErrorAction SilentlyContinue
}
