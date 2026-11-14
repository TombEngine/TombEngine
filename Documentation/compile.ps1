# Builds the LDoc documentation into doc: runs LDoc, generates the Settings classes, copies the resources and writes
# the search index. Runs with Windows PowerShell, using the Lua of the compiler folder, and with PowerShell 7 on Linux
# and macOS, where LDoc needs Lua with LuaFileSystem on the PATH; without it the documentation is skipped.

param(
    [string]$Source = "."
)

$ErrorActionPreference = "Stop"

# Windows PowerShell 5 has no $IsWindows; there it is always Windows.
$isWindowsHost = ($IsWindows -ne $false)

function Find-Lua
{
    if ($isWindowsHost)
    {
        return (Join-Path (Join-Path $PSScriptRoot "compiler") "lua.exe")
    }

    foreach ($name in @("lua", "lua5.4", "lua5.3", "lua5.1", "luajit"))
    {
        $command = Get-Command $name -ErrorAction SilentlyContinue
        if ($command)
        {
            return $command.Source
        }
    }

    return $null
}

$lua = Find-Lua
if (-not $lua)
{
    Write-Warning "Lua not found: the documentation is not built."
    exit 0
}

$previousDirectory = [Environment]::CurrentDirectory
Push-Location $PSScriptRoot
[Environment]::CurrentDirectory = $PSScriptRoot

try
{
    $docDir = Join-Path "." "doc"
    $resourcesDir = Join-Path $docDir "resources"
    $compilerDir = Join-Path "." "compiler"

    # The compiler folder holds LDoc and Penlight; on Windows also LuaFileSystem.
    $env:LUA_PATH = (Join-Path $compilerDir "?.lua") + ";;"
    if ($isWindowsHost)
    {
        $env:LUA_CPATH = (Join-Path $compilerDir "?.dll") + ";;"
    }

    Write-Host "Building LDoc documentation..."
    if (Test-Path $docDir)
    {
        Remove-Item $docDir -Recurse -Force
    }

    New-Item -ItemType Directory -Force -Path $resourcesDir | Out-Null

    Write-Host "Running LDoc compiler..."
    & $lua (Join-Path (Join-Path $compilerDir "ldoc") "ldoc.lua") $Source
    if ($LASTEXITCODE -ne 0)
    {
        throw "LDoc compilation failed with error code $LASTEXITCODE."
    }

    Write-Host "Post-processing Settings XML structure..."
    $global:LASTEXITCODE = 0
    & (Join-Path "." "settings_class_post_process.ps1") -SettingsCppFile "../TombEngine/Scripting/Internal/TEN/Flow/Settings/Settings.cpp"
    if ($LASTEXITCODE -ne 0)
    {
        throw "XML post-processing failed with error code $LASTEXITCODE."
    }

    Write-Host "Copying resources..."
    $css = Join-Path $docDir "ldoc.css"
    if (Test-Path $css)
    {
        Move-Item $css (Join-Path $resourcesDir "ldoc.css") -Force
    }

    Copy-Item (Join-Path ".." "TEN logo.png") (Join-Path $resourcesDir "TEN logo.png") -Force
    Copy-Item (Join-Path "." "docs-search.js") (Join-Path $resourcesDir "docs-search.js") -Force

    Write-Host "Generating documentation search index..."
    & (Join-Path "." "generate_search_index.ps1") -DocRoot $docDir -OutputFile (Join-Path $resourcesDir "search-index.js")
    if ($LASTEXITCODE -ne 0)
    {
        throw "Documentation search index generation failed with error code $LASTEXITCODE."
    }

    Write-Host "Documentation build completed successfully!"
}
finally
{
    Pop-Location
    [Environment]::CurrentDirectory = $previousDirectory
}
