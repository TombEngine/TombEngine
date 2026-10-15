# Compiles the RHI shaders offline into DXBC (FXC, for DX11), SPIR-V (DXC, for SDL GPU on Vulkan), DXIL (DXC, for SDL GPU
# on Direct3D 12) and GLSL (SPIRV-Cross from the SPIR-V, for OpenGL).
# Each .hlsl declares its entry points and resource counts in header lines:
#   // TEN_SHADER: <Vertex|Pixel|Compute> <Entry> samplers=N storage_textures=N storage_buffers=N uniform_buffers=N [rw_storage_textures=N rw_storage_buffers=N threads=X,Y,Z permutations=DEFINE,... options=DEFINE,... hlsl=YEAR]
# Outputs: <OutputDir>\dxbc\<Name>.<Entry>.dxbc, <OutputDir>\spirv\<Name>.<Entry>.spv, <OutputDir>\dxil\<Name>.<Entry>.dxil,
# <OutputDir>\glsl\<Name>.<Entry>.glsl, <OutputDir>\<Name>.<Entry>.meta
# Each permutation adds a variant compiled with DEFINE=1, named <Name>.<Entry>.<DEFINE>.
# Options add a variant for every combination of them, compiled with each DEFINE=1 and named with the chosen
# DEFINEs joined by dots in the declared order, such as <Name>.<Entry>.<DEFINE1>.<DEFINE3>.
# hlsl=YEAR sets the HLSL language version of DXC, for sources written for an older version.
# DXBC needs FXC from the Windows SDK; without it only SPIR-V is produced and the DirectX 11 backend is unavailable.
# SPIR-V needs DXC from the Vulkan SDK (VULKAN_SDK) or the PATH; without it only DXBC is produced. At least one is required.
# DXIL needs the DXC of the Windows SDK, which signs the code with the dxil.dll next to it; without it Direct3D 12 is unavailable.
# GLSL needs SPIRV-Cross from the Vulkan SDK and the SPIR-V output; without it OpenGL is unavailable.

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

# Compiles an empty pixel shader to SPIR-V: the DXC of the Windows SDK is built without SPIR-V code generation.
function Test-SpirvSupport([string]$dxc)
{
    $ErrorActionPreference = "Continue"
    $source = Join-Path ([IO.Path]::GetTempPath()) "ten_spirv_probe.hlsl"
    $output = Join-Path ([IO.Path]::GetTempPath()) "ten_spirv_probe.spv"
    Set-Content -Path $source -Value "float4 main() : SV_Target { return 0; }" -Encoding ascii
    & $dxc -spirv -T ps_6_0 -E main -Fo $output $source 2>&1 | Out-Null
    $isSupported = ($LASTEXITCODE -eq 0)
    Remove-Item $source, $output -ErrorAction SilentlyContinue
    return $isSupported
}

# A Visual Studio started before the SDK installation lacks VULKAN_SDK in its environment, so the variable is
# also read from the stored user and machine environment, and the default installation folder is searched last.
function Find-VulkanSdkTool([string]$name)
{
    $sdkPaths = @($env:VULKAN_SDK)
    if ($IsWindows -ne $false)
    {
        $sdkPaths += [Environment]::GetEnvironmentVariable("VULKAN_SDK", "User")
        $sdkPaths += [Environment]::GetEnvironmentVariable("VULKAN_SDK", "Machine")
        $sdkPaths += Get-ChildItem -Path "C:\VulkanSDK" -Directory -ErrorAction SilentlyContinue |
            Sort-Object { try { [version]$_.Name } catch { [version]"0.0" } } -Descending |
            ForEach-Object { $_.FullName }
    }

    foreach ($sdkPath in ($sdkPaths | Where-Object { $_ }))
    {
        foreach ($candidate in @("Bin/$name.exe", "bin/$name", "Bin/$name"))
        {
            $path = Join-Path $sdkPath $candidate
            if (Test-Path $path) { return $path }
        }
    }

    return $null
}

# DXC of the Vulkan SDK, named dxc.exe on Windows and dxc elsewhere, or one with SPIR-V support on the PATH.
function Find-Dxc
{
    $path = Find-VulkanSdkTool "dxc"
    if ($path) { return $path }

    $command = Get-Command dxc -ErrorAction SilentlyContinue
    if ($command -and (Test-SpirvSupport $command.Source)) { return $command.Source }
    return $null
}

# DXC of the newest Windows SDK: the Vulkan SDK one has no dxil.dll to sign DXIL, and Direct3D 12 rejects unsigned code.
function Find-DxilCompiler
{
    $kits = "${env:ProgramFiles(x86)}\Windows Kits\10\bin"
    $candidates = Get-ChildItem -Path $kits -Filter dxc.exe -Recurse -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match '\\x64\\dxc\.exe$' -and (Test-Path (Join-Path $_.DirectoryName "dxil.dll")) } |
        Sort-Object FullName -Descending
    if ($candidates) { return $candidates[0].FullName }
    return $null
}

# DXC compiles the min, max and clamp of HLSL to the NaN-ignoring NMin, NMax and NClamp of GLSL.std.450, which
# SPIRV-Cross writes as isnan tests around each call. Direct3D hardware ignores NaN operands anyway, so the copy given
# to SPIRV-Cross uses FMin, FMax and FClamp instead.
function Convert-NanMinMax([string]$source, [string]$destination)
{
    $bytes = [IO.File]::ReadAllBytes($source)
    $words = New-Object uint32[] ($bytes.Length / 4)
    [Buffer]::BlockCopy($bytes, 0, $words, 0, $bytes.Length)

    $replacements = @{ 79 = 37; 80 = 40; 81 = 43 } # NMin, NMax, NClamp to FMin, FMax, FClamp.
    $glslSet = -1
    $index = 5
    while ($index -lt $words.Length)
    {
        $count = $words[$index] -shr 16
        $opcode = $words[$index] -band 0xFFFF
        if ($count -eq 0) { break }

        if ($opcode -eq 11 -and [Text.Encoding]::ASCII.GetString($bytes, ($index + 2) * 4, ($count - 2) * 4).TrimEnd([char]0) -eq "GLSL.std.450")
        {
            $glslSet = $words[$index + 1]
        }
        elseif ($opcode -eq 12 -and $words[$index + 3] -eq $glslSet -and $replacements.ContainsKey([int]$words[$index + 4]))
        {
            $words[$index + 4] = $replacements[[int]$words[$index + 4]]
        }

        $index += $count
    }

    [Buffer]::BlockCopy($words, 0, $bytes, 0, $bytes.Length)
    [IO.File]::WriteAllBytes($destination, $bytes)
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

# Newest write time of a source and the files it includes, followed recursively.
function Get-NewestDependencyTime([string]$path, [hashtable]$visited)
{
    $fullPath = [IO.Path]::GetFullPath($path)
    if ($visited.ContainsKey($fullPath) -or -not (Test-Path $fullPath))
    {
        return [datetime]::MinValue
    }

    $visited[$fullPath] = $true
    $newest = (Get-Item $fullPath).LastWriteTime
    $directory = [IO.Path]::GetDirectoryName($fullPath)
    foreach ($match in (Select-String -Path $fullPath -Pattern '^\s*#include\s+"([^"]+)"'))
    {
        $includeTime = Get-NewestDependencyTime (Join-Path $directory $match.Matches[0].Groups[1].Value) $visited
        if ($includeTime -gt $newest)
        {
            $newest = $includeTime
        }
    }

    return $newest
}

$fxc = Find-Fxc
$dxc = Find-Dxc
if (-not $fxc -and -not $dxc)
{
    throw "Neither fxc (Windows SDK) nor dxc (Vulkan SDK) was found."
}

if (-not $fxc)
{
    Write-Warning "fxc not found: DXBC shaders are not compiled, the DirectX 11 backend will be unavailable."
}

if (-not $dxc)
{
    Write-Warning "dxc not found: SPIR-V shaders are not compiled, the Vulkan backend will be unavailable."
}
else
{
    Write-Host "Using $dxc"
}

$dxilCompiler = Find-DxilCompiler
if (-not $dxilCompiler)
{
    Write-Warning "dxc with dxil.dll not found in the Windows SDK: DXIL shaders are not compiled, the Direct3D 12 backend will be unavailable."
}

$spirvCross = if ($dxc) { Find-VulkanSdkTool "spirv-cross" } else { $null }
if (-not $spirvCross)
{
    Write-Warning "spirv-cross not found: GLSL shaders are not generated, the OpenGL backend will be unavailable."
}

$dxbcDir = Join-Path $OutputDir "dxbc"
$spirvDir = Join-Path $OutputDir "spirv"
$dxilDir = Join-Path $OutputDir "dxil"
$glslDir = Join-Path $OutputDir "glsl"
New-Item -ItemType Directory -Force -Path $dxbcDir, $spirvDir, $dxilDir, $glslDir | Out-Null

$scriptTime = (Get-Item $PSCommandPath).LastWriteTime

$failed = $false

foreach ($file in Get-ChildItem -Path $sourceDir -Filter *.hlsl)
{
    $newestInput = @((Get-NewestDependencyTime $file.FullName @{}), $scriptTime) | Sort-Object -Descending | Select-Object -First 1
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

        $meta = @{ samplers = 0; storage_textures = 0; storage_buffers = 0; uniform_buffers = 0; rw_storage_textures = 0; rw_storage_buffers = 0; threads = "1,1,1"; permutations = ""; options = "" }
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

        $variants = @("") + @($meta["permutations"] -split ',' | Where-Object { $_ -ne "" })
        $options = @($meta["options"] -split ',' | Where-Object { $_ -ne "" })
        for ($mask = 1; $mask -lt (1 -shl $options.Count); $mask++)
        {
            $variants += (@(for ($i = 0; $i -lt $options.Count; $i++) { if ($mask -band (1 -shl $i)) { $options[$i] } }) -join '.')
        }

        foreach ($variant in $variants)
        {
            $base = if ($variant) { "$name.$entry.$variant" } else { "$name.$entry" }
            $dxbcFile = Join-Path $dxbcDir "$base.dxbc"
            $spirvFile = Join-Path $spirvDir "$base.spv"
            $dxilFile = Join-Path $dxilDir "$base.dxil"
            $glslFile = Join-Path $glslDir "$base.glsl"
            $metaFile = Join-Path $OutputDir "$base.meta"

            if ((-not $fxc -or (Test-UpToDate $dxbcFile $newestInput)) -and (Test-UpToDate $metaFile $newestInput) -and
                (-not $dxc -or (Test-UpToDate $spirvFile $newestInput)) -and (-not $dxilCompiler -or (Test-UpToDate $dxilFile $newestInput)) -and
                (-not $spirvCross -or (Test-UpToDate $glslFile $newestInput)))
            {
                continue
            }

            $defines = @($variant -split '\.' | Where-Object { $_ })

            Write-Host "Compiling $base ($stage)"

            if ($fxc)
            {
                $fxcArgs = @("/nologo", "/T", $info.Fxc, "/E", $entry, "/Zpr", "/Fo", $dxbcFile)
                foreach ($define in $defines) { $fxcArgs += @("/D", "$define=1") }
                if ($DebugInfo) { $fxcArgs += @("/Zi", "/Od") } else { $fxcArgs += "/O3" }
                & $fxc @fxcArgs $file.FullName | Out-Null
                if ($LASTEXITCODE -ne 0)
                {
                    & $fxc @fxcArgs $file.FullName
                    Write-Error "FXC failed for $base" -ErrorAction Continue
                    $failed = $true
                    continue
                }
            }

            if ($dxc)
            {
                $dxcArgs = @("-spirv", "-T", $info.Dxc, "-E", $entry, "-Zpr", "-fvk-use-dx-layout", "-fspv-target-env=vulkan1.0", "-Fo", $spirvFile)
                foreach ($define in $defines) { $dxcArgs += @("-D", "$define=1") }
                if ($meta["hlsl"]) { $dxcArgs += @("-HV", $meta["hlsl"]) }
                if ($DebugInfo) { $dxcArgs += @("-Zi", "-Od") } else { $dxcArgs += "-O3" }
                & $dxc @dxcArgs $file.FullName
                if ($LASTEXITCODE -ne 0)
                {
                    Write-Error "DXC failed for $base" -ErrorAction Continue
                    $failed = $true
                    continue
                }
            }

            if ($dxilCompiler)
            {
                $dxilArgs = @("-T", $info.Dxc, "-E", $entry, "-Zpr", "-D", "TEN_DXIL=1", "-Fo", $dxilFile)
                foreach ($define in $defines) { $dxilArgs += @("-D", "$define=1") }
                if ($meta["hlsl"]) { $dxilArgs += @("-HV", $meta["hlsl"]) }
                if ($DebugInfo) { $dxilArgs += @("-Zi", "-Qembed_debug", "-Od") } else { $dxilArgs += "-O3" }
                & $dxilCompiler @dxilArgs $file.FullName
                if ($LASTEXITCODE -ne 0)
                {
                    Write-Error "DXC failed for the DXIL of $base" -ErrorAction Continue
                    $failed = $true
                    continue
                }
            }

            # Separate shader objects keep the bindings of each stage apart; the OpenGL backend offsets them per stage.
            if ($spirvCross)
            {
                $glslSpirvFile = "$glslFile.spv"
                Convert-NanMinMax $spirvFile $glslSpirvFile
                & $spirvCross $glslSpirvFile --version 450 --no-es --separate-shader-objects --output $glslFile
                $exitCode = $LASTEXITCODE
                Remove-Item $glslSpirvFile
                if ($exitCode -ne 0)
                {
                    Write-Error "SPIRV-Cross failed for $base" -ErrorAction Continue
                    $failed = $true
                    continue
                }
            }

            Set-Content -Path $metaFile -Value $metaText -Encoding ascii
        }
    }
}

if ($failed)
{
    exit 1
}
