# Visual parity test of the next renderer against the legacy renderer.
# Runs the benchmark scenario (Benchmark.lua) on every benchmark level and backend, compares the screenshots both
# renderers save at the same game frame (Logs/Compare_Legacy.png and Logs/Compare_Next.png), and checks the logs for
# Vulkan validation errors and device loss. Exits with 1 when a run fails, otherwise 0.
#
# Usage, from any directory:
#   powershell -ExecutionPolicy Bypass -File Documentation\Renderer\RendererParity.ps1 [-BuildDir Build\Debug]
#       [-Levels Benchmark_coastal,...] [-Backends vulkan,dx11] [-Window preview|next] [-Threshold 5] [-Seconds 12]
#
# Threshold is the largest share of pixels, in percent, whose channels differ by more than 8 between the renderers.
# Animated water and particles differ by timing, so a few percent is expected.

param(
    [string]$BuildDir = "",
    [string[]]$Levels = @("Benchmark_coastal", "Benchmark_karnak", "Benchmark_tut1_water"),
    [string[]]$Backends = @("vulkan", "dx11"),
    [string]$Window = "preview",
    [double]$Threshold = 5.0,
    [int]$Seconds = 12,
    [string]$OutputDir = ""
)

$ErrorActionPreference = "Stop"

# With -File, lists arrive as one comma separated string.
$Levels = @($Levels | ForEach-Object { $_ -split "," } | Where-Object { $_ })
$Backends = @($Backends | ForEach-Object { $_ -split "," } | Where-Object { $_ })

Add-Type -AssemblyName System.Drawing

$repository = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
if (-not $BuildDir) { $BuildDir = Join-Path $repository "Build\Debug" }
if (-not $OutputDir) { $OutputDir = Join-Path $BuildDir "Logs\Parity" }

$executable = Join-Path $BuildDir "Bin\x64\TombEngine.exe"
$logDir = Join-Path $BuildDir "Logs"
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
Copy-Item (Join-Path $PSScriptRoot "Benchmark.lua") (Join-Path $BuildDir "Scripts\Levels\Benchmark.lua")

# Share of sampled pixels (every third row and column) differing by more than 8 in any channel.
function Get-DifferingShare([string]$first, [string]$second)
{
    $a = [System.Drawing.Bitmap]::new($first)
    $b = [System.Drawing.Bitmap]::new($second)
    try
    {
        if ($a.Width -ne $b.Width -or $a.Height -ne $b.Height) { return 100.0 }

        $count = 0
        $differing = 0
        for ($y = 0; $y -lt $a.Height; $y += 3)
        {
            for ($x = 0; $x -lt $a.Width; $x += 3)
            {
                $p = $a.GetPixel($x, $y)
                $q = $b.GetPixel($x, $y)
                $difference = [math]::Max([math]::Max([math]::Abs($p.R - $q.R), [math]::Abs($p.G - $q.G)), [math]::Abs($p.B - $q.B))
                $count++
                if ($difference -gt 8) { $differing++ }
            }
        }

        return 100.0 * $differing / $count
    }
    finally
    {
        $a.Dispose()
        $b.Dispose()
    }
}

$failed = $false
foreach ($level in $Levels)
{
    foreach ($backend in $Backends)
    {
        Remove-Item (Join-Path $logDir "Compare_*.png") -ErrorAction SilentlyContinue
        $name = "$level`_$backend"
        $stdout = Join-Path $OutputDir "$name.out.txt"
        $stderr = Join-Path $OutputDir "$name.err.txt"

        $process = Start-Process -FilePath $executable -WorkingDirectory $BuildDir -PassThru `
            -ArgumentList @("debug", "level", "Data/$level.ten", $Window, $backend, "benchmark", "$Seconds") `
            -RedirectStandardOutput $stdout -RedirectStandardError $stderr
        if (-not $process.WaitForExit(($Seconds + 120) * 1000))
        {
            $process.Kill()
            "$name : timed out"
            $failed = $true
            continue
        }

        $log = (Get-Content $stdout, $stderr -ErrorAction SilentlyContinue) -join "`n"
        $validationErrors = ([regex]::Matches($log, "VUID-")).Count
        $deviceLosses = ([regex]::Matches($log, "DEVICE_LOST")).Count

        $legacyShot = Join-Path $logDir "Compare_Legacy.png"
        $nextShot = Join-Path $logDir "Compare_Next.png"
        if (-not (Test-Path $legacyShot) -or -not (Test-Path $nextShot))
        {
            "$name : no screenshots, validation errors $validationErrors, device losses $deviceLosses"
            $failed = $true
            continue
        }

        Copy-Item $legacyShot (Join-Path $OutputDir "$name.legacy.png")
        Copy-Item $nextShot (Join-Path $OutputDir "$name.next.png")

        $share = Get-DifferingShare $legacyShot $nextShot
        $isPassed = ($validationErrors -eq 0 -and $deviceLosses -eq 0 -and $share -le $Threshold)
        if (-not $isPassed) { $failed = $true }

        "{0} : {1}, differing {2:N1}%, validation errors {3}, device losses {4}" -f $name, ($(if ($isPassed) { "passed" } else { "FAILED" })), $share, $validationErrors, $deviceLosses
    }
}

if ($failed) { exit 1 }
exit 0
