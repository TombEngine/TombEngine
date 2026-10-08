# Compiles the RHI shaders offline into DXBC (FXC, for DX11) and SPIR-V (DXC, for SDL GPU / Vulkan).
# Each .hlsl declares its entry points and resource counts in header lines:
#   // TEN_SHADER: <Vertex|Pixel|Compute> <Entry> samplers=N storage_textures=N storage_buffers=N uniform_buffers=N [rw_storage_textures=N rw_storage_buffers=N threads=X,Y,Z]
# Outputs: <OutputDir>\dxbc\<Name>.<Entry>.dxbc, <OutputDir>\spirv\<Name>.<Entry>.spv, <OutputDir>\<Name>.<Entry>.meta
# SPIR-V needs DXC from the Vulkan SDK (VULKAN_SDK); without it only DXBC is produced.

param(
    [Parameter(Mandatory = $true)][string]$OutputDir,
    [switch]$DebugInfo
)

$ErrorActionPreference = "Stop"
$sourceDir = $PSScriptRoot

function Find-Fxc
{
    $kits = "${env:ProgramFiles(x86)}\Windows Kits\10\bin"
    $candidates = Get-ChildItem -Path $kits -Filter fxc.exe -Recurse -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match '\\x64\\fxc\.exe$' } |
        Sort-Object FullName -Descending
    if ($candidates) { return $candidates[0].FullName }
    return $null
}

function Get-StageInfo([string]$stage)
{
    switch ($stage)
    {
        "Vertex"  { return @{ Fxc = "vs_5_0"; Dxc = "vs_6_0" } }
        "Pixel"   { return @{ Fxc = "ps_5_0"; Dxc = "ps_6_0" } }
        "Compute" { return @{ Fxc = "cs_5_0"; Dxc = "cs_6_0" } }
        default   { throw "Unknown shader stage '$stage'." }
    }
}

function Test-UpToDate([string]$output, [datetime]$newestInput)
{
    return (Test-Path $output) -and ((Get-Item $output).LastWriteTime -ge $newestInput)
}

$fxc = Find-Fxc
if (-not $fxc)
{
    throw "fxc.exe not found in the Windows SDK."
}

$dxc = $null
if ($env:VULKAN_SDK -and (Test-Path "$env:VULKAN_SDK\Bin\dxc.exe"))
{
    $dxc = "$env:VULKAN_SDK\Bin\dxc.exe"
}
else
{
    Write-Warning "VULKAN_SDK not found: SPIR-V shaders are not compiled, the Vulkan backend will be unavailable."
}

$dxbcDir = Join-Path $OutputDir "dxbc"
$spirvDir = Join-Path $OutputDir "spirv"
New-Item -ItemType Directory -Force -Path $dxbcDir, $spirvDir | Out-Null

# Any include change recompiles everything.
$includeTime = (Get-ChildItem -Path $sourceDir -Filter *.hlsli | Measure-Object -Property LastWriteTime -Maximum).Maximum
$scriptTime = (Get-Item $PSCommandPath).LastWriteTime

$failed = $false

foreach ($file in Get-ChildItem -Path $sourceDir -Filter *.hlsl)
{
    $newestInput = @($file.LastWriteTime, $includeTime, $scriptTime) | Sort-Object -Descending | Select-Object -First 1
    $name = [IO.Path]::GetFileNameWithoutExtension($file.Name)

    foreach ($line in Get-Content $file.FullName)
    {
        if ($line -notmatch '^//\s*TEN_SHADER:\s*(\w+)\s+(\w+)\s*(.*)$')
        {
            continue
        }

        $stage = $Matches[1]
        $entry = $Matches[2]
        $settings = $Matches[3]
        $info = Get-StageInfo $stage

        $base = "$name.$entry"
        $dxbcFile = Join-Path $dxbcDir "$base.dxbc"
        $spirvFile = Join-Path $spirvDir "$base.spv"
        $metaFile = Join-Path $OutputDir "$base.meta"

        if ((Test-UpToDate $dxbcFile $newestInput) -and (Test-UpToDate $metaFile $newestInput) -and (-not $dxc -or (Test-UpToDate $spirvFile $newestInput)))
        {
            continue
        }

        Write-Host "Compiling $base ($stage)"

        $fxcArgs = @("/nologo", "/T", $info.Fxc, "/E", $entry, "/Zpr", "/Fo", $dxbcFile)
        if ($DebugInfo) { $fxcArgs += @("/Zi", "/Od") } else { $fxcArgs += "/O3" }
        & $fxc @fxcArgs $file.FullName | Out-Null
        if ($LASTEXITCODE -ne 0)
        {
            & $fxc @fxcArgs $file.FullName
            Write-Error "FXC failed for $base" -ErrorAction Continue
            $failed = $true
            continue
        }

        if ($dxc)
        {
            $dxcArgs = @("-spirv", "-T", $info.Dxc, "-E", $entry, "-Zpr", "-fvk-use-dx-layout", "-fspv-target-env=vulkan1.0", "-Fo", $spirvFile)
            if ($DebugInfo) { $dxcArgs += @("-Zi", "-Od") } else { $dxcArgs += "-O3" }
            & $dxc @dxcArgs $file.FullName
            if ($LASTEXITCODE -ne 0)
            {
                Write-Error "DXC failed for $base" -ErrorAction Continue
                $failed = $true
                continue
            }
        }

        $meta = @{ samplers = 0; storage_textures = 0; storage_buffers = 0; uniform_buffers = 0; rw_storage_textures = 0; rw_storage_buffers = 0; threads = "1,1,1" }
        foreach ($pair in ($settings -split '\s+' | Where-Object { $_ -match '=' }))
        {
            $key, $value = $pair -split '=', 2
            $meta[$key] = $value
        }

        $metaText = "stage=$stage entry=$entry"
        foreach ($key in @("samplers", "storage_textures", "storage_buffers", "uniform_buffers", "rw_storage_textures", "rw_storage_buffers", "threads"))
        {
            $metaText += " $key=$($meta[$key])"
        }

        Set-Content -Path $metaFile -Value $metaText -Encoding ascii
    }
}

if ($failed)
{
    exit 1
}
