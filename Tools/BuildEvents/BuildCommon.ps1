# Helpers shared by the build scripts. Works with Windows PowerShell and with PowerShell 7 on Linux and macOS.

# Windows PowerShell 5 has no $IsWindows, $IsLinux and $IsMacOS; there it is always Windows.
$script:IsWindowsHost = ($IsWindows -ne $false)

function Get-RootDirectory
{
    return [IO.Path]::GetFullPath([IO.Path]::Combine($PSScriptRoot, "..", ".."))
}

# Executable folder inside Build/<Config>, as written by the Visual Studio project and CMake.
function Get-TargetDirectory([string]$buildDir, [string]$platform)
{
    $platformName = if ($platform -eq "Win32") { "x86" } else { $platform }
    return Join-Path (Join-Path $buildDir "Bin") $platformName
}

# Folder of the prebuilt native libraries inside each Libs/<Library> folder.
function Get-LibraryDirectoryName([string]$platform)
{
    if ($IsLinux)
    {
        $architecture = [Runtime.InteropServices.RuntimeInformation]::OSArchitecture
        return Join-Path "linux" $(if ($architecture -eq [Runtime.InteropServices.Architecture]::Arm64) { "aarch64" } else { "x86_64" })
    }

    if ($IsMacOS)
    {
        return "macos"
    }

    if ($platform -eq "Win32")
    {
        return "x86"
    }

    return $platform
}

# Copies the files matching the filter that are missing or different in the destination. Copies keep the time of their
# source, so a library replaced by an older release is copied too.
function Copy-Changed([string]$source, [string]$destination, [string]$filter, [bool]$recurse = $true)
{
    if (-not (Test-Path $source))
    {
        return
    }

    $sourceRoot = (Resolve-Path $source).Path
    foreach ($file in Get-ChildItem -Path $sourceRoot -Filter $filter -File -Recurse:$recurse)
    {
        $relativePath = $file.FullName.Substring($sourceRoot.Length).TrimStart('\', '/')
        $target = Join-Path $destination $relativePath
        if (Test-Path $target)
        {
            $targetFile = Get-Item $target
            if ($targetFile.LastWriteTimeUtc -eq $file.LastWriteTimeUtc -and $targetFile.Length -eq $file.Length)
            {
                continue
            }
        }

        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $target) | Out-Null
        Copy-Item $file.FullName $target -Force
    }
}

# Runs a script from its working folder, which relative paths in it expect, and stops the build when it fails.
function Invoke-Script([string]$directory, [string]$path, [hashtable]$arguments = @{})
{
    $previousDirectory = [Environment]::CurrentDirectory
    Push-Location $directory
    [Environment]::CurrentDirectory = (Get-Location).ProviderPath

    try
    {
        $global:LASTEXITCODE = 0
        & $path @arguments
        if ($LASTEXITCODE -ne 0)
        {
            throw "$path failed with exit code $LASTEXITCODE."
        }
    }
    finally
    {
        Pop-Location
        [Environment]::CurrentDirectory = $previousDirectory
    }
}
