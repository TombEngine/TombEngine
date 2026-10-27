# Pre-build step of the Visual Studio project and of CMake: generates the object ID list, the FlatBuffers code and the
# documentation, compiles the shaders and copies the shader sources, native libraries and scripts into the build folder.
# Runs with Windows PowerShell and with PowerShell 7 on Linux and macOS.

param(
    [Parameter(Mandatory = $true)][string]$Configuration,
    [string]$Platform = "x64",
    [string]$BuildDir = "",
    [string]$TargetDir = ""
)

$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "BuildCommon.ps1")

$rootDir = Get-RootDirectory
$projectDir = Join-Path $rootDir "TombEngine"
if ($BuildDir -eq "")
{
    $BuildDir = Join-Path (Join-Path $rootDir "Build") $Configuration
}

if ($TargetDir -eq "")
{
    $TargetDir = Get-TargetDirectory $BuildDir $Platform
}

# Generated sources.
Invoke-Script $projectDir (Join-Path (Join-Path $rootDir "Documentation") "generate_objectlist.ps1")
Invoke-Script $projectDir (Join-Path (Join-Path (Join-Path (Join-Path $projectDir "Specific") "serialization") "schema") "generate_code.ps1")

# The documentation is not needed to run the engine, so its errors do not stop the build.
try
{
    Invoke-Script (Join-Path $rootDir "Documentation") (Join-Path (Join-Path $rootDir "Documentation") "compile.ps1")
}
catch
{
    Write-Warning "Documentation not built: $_"
}

# Shaders: sources next to the binaries for the hot reload, then the binaries of every available backend.
$shaderSourceDir = Join-Path $projectDir "Shaders"
$shaderDir = Join-Path $BuildDir "Shaders"
Copy-Newer $shaderSourceDir $shaderDir "*"

$shaderArgs = @{ OutputDir = (Join-Path $shaderDir "Bin") }
if ($Configuration -eq "Debug")
{
    $shaderArgs["DebugInfo"] = $true
}

Invoke-Script $projectDir (Join-Path $shaderSourceDir "CompileShaders.ps1") $shaderArgs

# Native libraries of this platform.
$libraryDir = Get-LibraryDirectoryName $Platform
New-Item -ItemType Directory -Force -Path $TargetDir | Out-Null
foreach ($library in Get-ChildItem -Path (Join-Path $rootDir "Libs") -Directory)
{
    $platformDir = Join-Path $library.FullName $libraryDir
    if (Test-Path $platformDir)
    {
        foreach ($pattern in @("*.dll", "*.so", "*.so.*", "*.dylib"))
        {
            Copy-Newer $platformDir $TargetDir $pattern $false
        }
    }
}

# The Windows executable embeds the dummy title level as a resource; other platforms load it from the executable folder.
if (-not $script:IsWindowsHost)
{
    Copy-Item (Join-Path (Join-Path $projectDir "Resources") "title.bin") (Join-Path $TargetDir "dummy.ten") -Force
}

# Scripts: engine scripts and system strings are refreshed, level scripts are copied only once.
$scriptSourceDir = Join-Path $rootDir "Scripts"
$scriptDir = Join-Path $BuildDir "Scripts"
Copy-Newer (Join-Path $scriptSourceDir "Engine") (Join-Path $scriptDir "Engine") "*"
Copy-Newer $scriptSourceDir $scriptDir "SystemStrings.lua" $false

foreach ($name in @("Gameflow.lua", "Settings.lua", "Strings.lua"))
{
    $destination = Join-Path $scriptDir $name
    if (-not (Test-Path $destination))
    {
        Copy-Item (Join-Path $scriptSourceDir $name) $destination
    }
}
