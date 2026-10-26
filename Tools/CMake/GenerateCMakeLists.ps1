# Generates CMakeLists.txt from TombEngine/TombEngine.vcxproj. The Visual Studio project stays the reference build:
# sources, compiler and linker settings, libraries and build events are read from it, so rerun this script after
# changing the project. Run it through GenerateCMakeLists.cmd (Windows) or GenerateCMakeLists.sh (PowerShell 7 on
# Linux and macOS) in the repository root.

param(
    [string]$ProjectFile = "",
    [string]$OutputFile = ""
)

$ErrorActionPreference = "Stop"

# Paths are built with Join-Path and normalized separators, so the script also runs with PowerShell 7 on Linux and macOS.
$rootDir = [IO.Path]::GetFullPath([IO.Path]::Combine($PSScriptRoot, "..", ".."))
if ($ProjectFile -eq "")
{
    $ProjectFile = Join-Path (Join-Path $rootDir "TombEngine") "TombEngine.vcxproj"
}
if ($OutputFile -eq "")
{
    $OutputFile = Join-Path $rootDir "CMakeLists.txt"
}

$ProjectFile = (Resolve-Path $ProjectFile).Path
$solutionDir = Split-Path -Parent ([IO.Path]::GetFullPath($OutputFile))
$projectDir = Split-Path -Parent $ProjectFile
$projectName = [IO.Path]::GetFileNameWithoutExtension($ProjectFile)

if (-not $projectDir.StartsWith($solutionDir, [StringComparison]::OrdinalIgnoreCase))
{
    throw "The project must be inside the output directory '$solutionDir'."
}

$projectRelDir = $projectDir.Substring($solutionDir.Length).Replace('\', '/').TrimStart('/')
[xml]$project = Get-Content -Raw $ProjectFile

$warnings = New-Object System.Collections.Generic.List[string]

function Add-Warning([string]$text)
{
    if (-not $warnings.Contains($text))
    {
        $warnings.Add($text)
    }
}

# Configurations and per-configuration settings.

$configs = New-Object System.Collections.Generic.List[object]
foreach ($group in $project.Project.ItemGroup)
{
    foreach ($item in $group.ChildNodes)
    {
        if ($item.LocalName -ne "ProjectConfiguration")
        {
            continue
        }

        $configs.Add([pscustomobject]@{
            Key      = $item.GetAttribute("Include")
            Config   = $item.Configuration
            Platform = $item.Platform
        })
    }
}

$configNames = @($configs | ForEach-Object { $_.Config } | Select-Object -Unique)
$platformNames = @($configs | ForEach-Object { $_.Platform } | Select-Object -Unique)

$settings = @{}
foreach ($config in $configs)
{
    $settings[$config.Key] = @{}
}

function Get-ConditionKey([string]$condition)
{
    if ($condition -match "==\s*'([^|']+)\|([^']+)'")
    {
        return "$($Matches[1])|$($Matches[2])"
    }

    return ""
}

function Set-Setting([string]$key, [string]$name, [string]$value)
{
    $keys = if ($key -eq "") { @($settings.Keys) } else { @($key) }
    foreach ($k in $keys)
    {
        if ($settings.ContainsKey($k))
        {
            $settings[$k][$name] = $value
        }
    }
}

function Read-SettingElements($parent, [string]$prefix, [string]$groupKey)
{
    foreach ($node in $parent.ChildNodes)
    {
        if ($node.NodeType -ne [Xml.XmlNodeType]::Element)
        {
            continue
        }

        $key = Get-ConditionKey $node.GetAttribute("Condition")
        if ($key -eq "")
        {
            $key = $groupKey
        }

        Set-Setting $key "$prefix$($node.LocalName)" $node.InnerText
    }
}

foreach ($group in $project.Project.PropertyGroup)
{
    $label = $group.GetAttribute("Label")
    if ($label -ne "" -and $label -ne "Configuration")
    {
        continue
    }

    Read-SettingElements $group "" (Get-ConditionKey $group.GetAttribute("Condition"))
}

foreach ($group in $project.Project.ItemDefinitionGroup)
{
    $groupKey = Get-ConditionKey $group.GetAttribute("Condition")
    foreach ($tool in $group.ChildNodes)
    {
        if ($tool.NodeType -eq [Xml.XmlNodeType]::Element)
        {
            Read-SettingElements $tool "$($tool.LocalName)." $groupKey
        }
    }
}

# Source files.

$sources = New-Object System.Collections.Generic.List[string]
$headers = New-Object System.Collections.Generic.List[string]
$resources = New-Object System.Collections.Generic.List[string]
$noPchSources = New-Object System.Collections.Generic.List[string]

function Convert-ItemPath([string]$include)
{
    $path = $include.Replace('\', '/')
    while ($path.StartsWith("./"))
    {
        $path = $path.Substring(2)
    }

    return "`${TEN_PROJECT_DIR}/$path"
}

foreach ($group in $project.Project.ItemGroup)
{
    foreach ($item in $group.ChildNodes)
    {
        if ($item.NodeType -ne [Xml.XmlNodeType]::Element)
        {
            continue
        }

        $include = $item.GetAttribute("Include")
        switch ($item.LocalName)
        {
            "ClCompile"
            {
                $path = Convert-ItemPath $include
                $sources.Add($path)

                foreach ($child in $item.ChildNodes)
                {
                    if ($child.LocalName -eq "PrecompiledHeader" -and $child.InnerText -eq "NotUsing")
                    {
                        $noPchSources.Add($path)
                        break
                    }
                    elseif ($child.LocalName -eq "ExcludedFromBuild")
                    {
                        Add-Warning "ExcludedFromBuild on '$include' is not translated."
                    }
                }
            }

            "ClInclude"
            {
                $headers.Add((Convert-ItemPath $include))
            }

            "ResourceCompile"
            {
                $resources.Add((Convert-ItemPath $include))
            }
        }
    }
}

# Path and list helpers.

$inheritedMacros = @('$(IncludePath)', '$(LibraryPath)', '$(ExecutablePath)')

function Convert-MacroPath([string]$path, [string]$config, [string]$platform)
{
    $result = $path.Replace('$(SolutionDir)Build\', '${TEN_BUILD_ROOT}/')
    $result = $result.Replace('$(SolutionDir)', '${TEN_SOLUTION_DIR}/')
    $result = $result.Replace('$(ProjectDir)', '${TEN_PROJECT_DIR}/')
    $result = $result.Replace('$(Configuration)', $config).Replace('$(Platform)', $platform).Replace('$(ProjectName)', $projectName)

    if ($result -match '\$\([^)]+\)')
    {
        Add-Warning "Skipped '$path' because it uses the macro $($Matches[0])."
        return $null
    }

    $result = $result.Replace('\', '/').TrimEnd('/')
    while ($result.Contains("//"))
    {
        $result = $result.Replace("//", "/")
    }

    if (-not $result.StartsWith('${') -and -not [IO.Path]::IsPathRooted($result))
    {
        $result = "`${TEN_PROJECT_DIR}/$result"
    }

    return $result
}

function Split-List([string]$value)
{
    if ($null -eq $value)
    {
        return @()
    }

    return @($value.Split(';') | ForEach-Object { $_.Trim() } | Where-Object { $_ -ne "" -and -not $_.StartsWith('%(') -and $inheritedMacros -notcontains $_ })
}

function Split-Options([string]$value)
{
    if ($null -eq $value)
    {
        return @()
    }

    return @($value.Split(@(' ', "`t", "`r", "`n"), [StringSplitOptions]::RemoveEmptyEntries))
}

function Convert-PathList([string]$value, $config)
{
    $result = @()
    foreach ($entry in (Split-List $value))
    {
        $path = Convert-MacroPath $entry $config.Config $config.Platform
        if ($null -ne $path)
        {
            $result += $path
        }
    }

    return $result
}

# Translation of MSBuild settings to compiler and linker arguments.

function Get-Definitions($config)
{
    $s = $settings[$config.Key]
    $result = @(Split-List $s["ClCompile.PreprocessorDefinitions"])

    switch ($s["CharacterSet"])
    {
        "MultiByte" { $result += "_MBCS" }
        "Unicode"   { $result += @("UNICODE", "_UNICODE") }
    }

    return $result
}

function Get-IncludeDirectories($config)
{
    $s = $settings[$config.Key]
    return @(Convert-PathList $s["ClCompile.AdditionalIncludeDirectories"] $config) + @(Convert-PathList $s["IncludePath"] $config)
}

function Get-LinkDirectories($config)
{
    $s = $settings[$config.Key]
    return @(Convert-PathList $s["Link.AdditionalLibraryDirectories"] $config) + @(Convert-PathList $s["LibraryPath"] $config)
}

function Get-LinkLibraries($config)
{
    return @(Split-List $settings[$config.Key]["Link.AdditionalDependencies"])
}

function Get-CompileOptions($config)
{
    $s = $settings[$config.Key]
    $isDebug = $s["UseDebugLibraries"] -eq "true"
    $result = @()

    switch ($s["ClCompile.WarningLevel"])
    {
        "TurnOffAllWarnings" { $result += "/w" }
        "Level1"             { $result += "/W1" }
        "Level2"             { $result += "/W2" }
        "Level3"             { $result += "/W3" }
        "Level4"             { $result += "/W4" }
        "EnableAllWarnings"  { $result += "/Wall" }
    }

    if ($s["ClCompile.TreatWarningAsError"] -eq "true")
    {
        $result += "/WX"
    }

    foreach ($warning in (Split-List $s["ClCompile.DisableSpecificWarnings"]))
    {
        $result += "/wd$warning"
    }

    # MSBuild defaults to no optimization and runtime checks when the debug libraries are used.
    $optimization = $s["ClCompile.Optimization"]
    if ($null -eq $optimization)
    {
        $optimization = if ($isDebug) { "Disabled" } else { "MaxSpeed" }
    }

    switch ($optimization)
    {
        "Disabled" { $result += "/Od" }
        "MinSpace" { $result += "/O1" }
        "MaxSpeed" { $result += "/O2" }
        "Full"     { $result += "/Ox" }
    }

    $runtimeChecks = $s["ClCompile.BasicRuntimeChecks"]
    if ($null -eq $runtimeChecks -and $isDebug)
    {
        $runtimeChecks = "EnableFastChecks"
    }

    switch ($runtimeChecks)
    {
        "EnableFastChecks"              { $result += "/RTC1" }
        "StackFrameRuntimeCheck"        { $result += "/RTCs" }
        "UninitializedLocalUsageCheck"  { $result += "/RTCu" }
    }

    switch ($s["ClCompile.InlineFunctionExpansion"])
    {
        "Disabled"           { $result += "/Ob0" }
        "OnlyExplicitInline" { $result += "/Ob1" }
        "AnySuitable"        { $result += "/Ob2" }
    }

    switch ($s["ClCompile.FavorSizeOrSpeed"])
    {
        "Speed" { $result += "/Ot" }
        "Size"  { $result += "/Os" }
    }

    switch ($s["ClCompile.FloatingPointModel"])
    {
        "Fast"    { $result += "/fp:fast" }
        "Strict"  { $result += "/fp:strict" }
        "Precise" { $result += "/fp:precise" }
    }

    $flags = [ordered]@{
        "ClCompile.IntrinsicFunctions"           = @("true", "/Oi")
        "ClCompile.OmitFramePointers"            = @("true", "/Oy")
        "ClCompile.WholeProgramOptimization"     = @("true", "/GL")
        "ClCompile.EnableFiberSafeOptimizations" = @("true", "/GT")
        "ClCompile.StringPooling"                = @("true", "/GF")
        "ClCompile.FunctionLevelLinking"         = @("true", "/Gy")
        "ClCompile.BufferSecurityCheck"          = @("false", "/GS-")
        "ClCompile.SupportJustMyCode"            = @("true", "/JMC")
        "ClCompile.ConformanceMode"              = @("true", "/permissive-")
        "ClCompile.MultiProcessorCompilation"    = @("true", "/MP")
    }

    foreach ($name in $flags.Keys)
    {
        if ($s[$name] -eq $flags[$name][0])
        {
            $result += $flags[$name][1]
        }
    }

    $result += Split-Options $s["ClCompile.AdditionalOptions"]
    return $result
}

function Get-LinkOptions($config)
{
    $s = $settings[$config.Key]
    $result = @()

    switch ($s["Link.SubSystem"])
    {
        "Console" { $result += "/SUBSYSTEM:CONSOLE" }
        "Windows" { $result += "/SUBSYSTEM:WINDOWS" }
    }

    switch ($s["Link.GenerateDebugInformation"])
    {
        "true"      { $result += "/DEBUG" }
        "DebugFull" { $result += "/DEBUG:FULL" }
    }

    switch ($s["LinkIncremental"])
    {
        "true"  { $result += "/INCREMENTAL" }
        "false" { $result += "/INCREMENTAL:NO" }
    }

    switch ($s["Link.LinkTimeCodeGeneration"])
    {
        "UseLinkTimeCodeGeneration"     { $result += "/LTCG" }
        "UseFastLinkTimeCodeGeneration" { $result += "/LTCG:INCREMENTAL" }
    }

    if ($s["Link.LargeAddressAware"] -eq "true")
    {
        $result += "/LARGEADDRESSAWARE"
    }

    if ($s["Link.ImageHasSafeExceptionHandlers"] -eq "false" -and $config.Platform -eq "Win32")
    {
        $result += "/SAFESEH:NO"
    }

    if ($s["Link.OptimizeReferences"] -eq "true")
    {
        $result += "/OPT:REF"
    }

    if ($s["Link.EnableCOMDATFolding"] -eq "true")
    {
        $result += "/OPT:ICF"
    }

    if ($null -ne $s["Link.StackReserveSize"])
    {
        $result += "/STACK:$($s["Link.StackReserveSize"])"
    }

    $result += Split-Options $s["Link.AdditionalOptions"]
    return $result
}

function Get-ScalarSetting($config, [string]$name, [hashtable]$map)
{
    $value = $settings[$config.Key][$name]
    if ($null -eq $value -or -not $map.ContainsKey($value))
    {
        return ""
    }

    return $map[$value]
}

# Build events must be PowerShell script calls, so the same scripts run on Windows, Linux and macOS. Each line becomes
# a CMake COMMAND run by the PowerShell found at configure time, with the MSBuild macros mapped to CMake values.

function Convert-EventArgument([string]$token)
{
    $value = $token.Trim('"')
    $isPath = $value -match '\$\((SolutionDir|ProjectDir|OutDir|TargetDir)\)'

    $value = $value.Replace('$(SolutionDir)Build\', '${TEN_BUILD_ROOT}/')
    $value = $value.Replace('$(SolutionDir)', '${TEN_SOLUTION_DIR}/')
    $value = $value.Replace('$(ProjectDir)', '${TEN_PROJECT_DIR}/')
    $value = $value.Replace('$(OutDir)', '${TEN_TARGET_DIR}/').Replace('$(TargetDir)', '${TEN_TARGET_DIR}/')
    $value = $value.Replace('$(Configuration)', '$<CONFIG>').Replace('$(Platform)', '${TEN_PLATFORM}')
    $value = $value.Replace('$(ProjectName)', $projectName)

    if ($value -match '\$\([^)]+\)')
    {
        throw "Build event macro $($Matches[0]) is not supported."
    }

    if ($isPath)
    {
        $value = $value.Replace('\', '/')
        while ($value.Contains("//"))
        {
            $value = $value.Replace("//", "/")
        }
    }

    return Format-Item $value
}

function Convert-EventCommands([string]$command)
{
    $result = @()
    if ($null -eq $command)
    {
        return $result
    }

    foreach ($line in ($command -replace "`r", "").Split("`n"))
    {
        if ($line.Trim() -eq "")
        {
            continue
        }

        $tokens = @([regex]::Matches($line.Trim(), '"[^"]*"|\S+') | ForEach-Object { $_.Value })
        if ($tokens[0] -notmatch '^(powershell|pwsh)(\.exe)?$')
        {
            throw "Build event line '$($line.Trim())' is not a PowerShell call; move it into a script under Tools/BuildEvents."
        }

        $arguments = @($tokens | Select-Object -Skip 1 | Where-Object { $_ -ne "-NoProfile" } | ForEach-Object { Convert-EventArgument $_ })
        $result += "COMMAND `"`${TEN_POWERSHELL}`" -NoProfile " + ($arguments -join " ")
    }

    return $result
}

# The event commands use $<CONFIG> and ${TEN_PLATFORM}, so every configuration must give the same commands.
function Get-EventCommands([string]$name)
{
    $commands = $null
    foreach ($config in $configs)
    {
        $configCommands = @(Convert-EventCommands $settings[$config.Key]["$name.Command"])
        if ($null -eq $commands)
        {
            $commands = $configCommands
        }
        elseif (($commands -join "`n") -ne ($configCommands -join "`n"))
        {
            throw "$name differs between configurations; pass the configuration to the script instead."
        }
    }

    return $commands
}

# Emission helpers.

$out = New-Object System.Collections.Generic.List[string]

function Emit([string]$line = "")
{
    $out.Add($line)
}

function Format-Item([string]$item)
{
    return "`"" + $item.Replace('"', '\"') + "`""
}

function Emit-Set([string]$indent, [string]$name, [string[]]$items, [bool]$append = $false)
{
    $command = if ($append) { "list(APPEND $name" } else { "set($name" }
    if ($items.Count -eq 0)
    {
        if (-not $append)
        {
            Emit "$indent$command)"
        }

        return
    }

    Emit "$indent$command"
    foreach ($item in $items)
    {
        Emit "$indent    $(Format-Item $item)"
    }

    Emit "$indent)"
}

function Get-Common($lists)
{
    $common = @()
    if ($lists.Count -eq 0)
    {
        return $common
    }

    foreach ($item in $lists[0])
    {
        $inAll = $true
        foreach ($list in $lists)
        {
            if ($list -notcontains $item)
            {
                $inAll = $false
                break
            }
        }

        if ($inAll -and $common -notcontains $item)
        {
            $common += $item
        }
    }

    return $common
}

function Get-Remainder($list, $exclude)
{
    $result = @()
    foreach ($item in $list)
    {
        if ($exclude -notcontains $item)
        {
            $result += $item
        }
    }

    return $result
}

function Get-Lists([string]$name, $configList, $exclude)
{
    $lists = New-Object System.Collections.Generic.List[object]
    foreach ($config in $configList)
    {
        $lists.Add([object[]]@(Get-Remainder $listValues[$name][$config.Key] $exclude))
    }

    return , $lists
}

$listSettings = [ordered]@{
    "TEN_DEFINITIONS"         = ${function:Get-Definitions}
    "TEN_INCLUDE_DIRECTORIES" = ${function:Get-IncludeDirectories}
    "TEN_CXX_OPTIONS"         = ${function:Get-CompileOptions}
    "TEN_LINK_OPTIONS"        = ${function:Get-LinkOptions}
    "TEN_LINK_DIRECTORIES"    = ${function:Get-LinkDirectories}
    "TEN_LINK_LIBRARIES"      = ${function:Get-LinkLibraries}
}

$listValues = @{}
foreach ($name in $listSettings.Keys)
{
    $listValues[$name] = @{}
    foreach ($config in $configs)
    {
        $listValues[$name][$config.Key] = @(& $listSettings[$name] $config)
    }
}

function Get-GlobalCommon([string]$name)
{
    return Get-Common (Get-Lists $name $configs @())
}

$runtimeMap = @{
    "MultiThreaded"         = "MultiThreaded"
    "MultiThreadedDebug"    = "MultiThreadedDebug"
    "MultiThreadedDLL"      = "MultiThreadedDLL"
    "MultiThreadedDebugDLL" = "MultiThreadedDebugDLL"
}

$debugFormatMap = @{
    "EditAndContinue" = "EditAndContinue"
    "ProgramDatabase" = "ProgramDatabase"
    "OldStyle"        = "Embedded"
}

$standardMap = @{
    "stdcpp14"     = "14"
    "stdcpp17"     = "17"
    "stdcpp20"     = "20"
    "stdcpplatest" = "23"
}

function Get-ConfigGenex([string]$name, [hashtable]$map)
{
    $parts = @()
    foreach ($configName in $configNames)
    {
        $config = $configs | Where-Object { $_.Config -eq $configName } | Select-Object -First 1
        $value = Get-ScalarSetting $config $name $map
        if ($value -ne "")
        {
            $parts += "`$<`$<CONFIG:$configName>:$value>"
        }
    }

    return ($parts -join "")
}

$standard = Get-ScalarSetting $configs[0] "ClCompile.LanguageStandard" $standardMap
if ($standard -eq "")
{
    $standard = "14"
}

$pchHeader = $settings[$configs[0].Key]["ClCompile.PrecompiledHeaderFile"]
$usesPch = $settings[$configs[0].Key]["ClCompile.PrecompiledHeader"] -eq "Use"

# CMakeLists.txt.

Emit "# Generated from $projectRelDir/$projectName.vcxproj by GenerateCMakeLists. Do not edit by hand: change the"
Emit "# Visual Studio project and run GenerateCMakeLists.cmd or .sh again."
Emit
Emit "cmake_minimum_required(VERSION 3.25)"
Emit
Emit "set(CMAKE_CONFIGURATION_TYPES `"$($configNames -join ';')`" CACHE STRING `"`" FORCE)"
Emit "if(NOT DEFINED CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL `"`")"
Emit "    set(CMAKE_BUILD_TYPE `"$($configNames[-1])`" CACHE STRING `"`" FORCE)"
Emit "endif()"
Emit
Emit "project($projectName LANGUAGES CXX RC)"
Emit
Emit "if(NOT MSVC)"
Emit "    message(FATAL_ERROR `"Only MSVC on Windows is supported for now: the libraries and build events come from the Visual Studio project.`")"
Emit "endif()"
Emit
Emit "get_property(TEN_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)"
Emit "if(NOT TEN_MULTI_CONFIG AND NOT CMAKE_BUILD_TYPE IN_LIST CMAKE_CONFIGURATION_TYPES)"
Emit "    message(FATAL_ERROR `"CMAKE_BUILD_TYPE must be one of: `${CMAKE_CONFIGURATION_TYPES}.`")"
Emit "endif()"
Emit
Emit "set(TEN_SOLUTION_DIR `"`${CMAKE_CURRENT_SOURCE_DIR}`")"
Emit "set(TEN_PROJECT_DIR `"`${TEN_SOLUTION_DIR}/$projectRelDir`")"
Emit "set(TEN_BUILD_ROOT `"`${TEN_SOLUTION_DIR}/Build`" CACHE PATH `"Folder holding the <Config> output, shader and script folders.`")"
Emit
Emit "if(CMAKE_SIZEOF_VOID_P EQUAL 8)"
Emit "    set(TEN_PLATFORM x64)"
Emit "else()"
Emit "    set(TEN_PLATFORM Win32)"
Emit "endif()"
Emit
Emit "# MSBuild settings are applied explicitly, so the CMake defaults per configuration are cleared."
Emit "foreach(config IN LISTS CMAKE_CONFIGURATION_TYPES)"
Emit "    string(TOUPPER `"`${config}`" config)"
Emit "    set(CMAKE_CXX_FLAGS_`${config} `"`")"
Emit "    set(CMAKE_RC_FLAGS_`${config} `"`")"
Emit "    set(CMAKE_EXE_LINKER_FLAGS_`${config} `"`")"
Emit "endforeach()"
Emit

Emit-Set "" "TEN_SOURCES" @($sources + $resources)
Emit
Emit-Set "" "TEN_HEADERS" @($headers)
Emit
Emit-Set "" "TEN_NO_PCH_SOURCES" @($noPchSources)
Emit

Emit "# Settings shared by all configurations and platforms."
foreach ($name in $listSettings.Keys)
{
    Emit-Set "" $name (Get-GlobalCommon $name)
}
Emit

$firstPlatform = $true
foreach ($platform in $platformNames)
{
    $platformConfigs = @($configs | Where-Object { $_.Platform -eq $platform })
    $keyword = if ($firstPlatform) { "if" } else { "elseif" }
    $firstPlatform = $false

    Emit "$keyword(TEN_PLATFORM STREQUAL `"$platform`")"

    foreach ($name in $listSettings.Keys)
    {
        $global = @(Get-GlobalCommon $name)
        $platformCommon = @(Get-Common (Get-Lists $name $platformConfigs $global))
        Emit-Set "    " $name $platformCommon $true

        foreach ($config in $platformConfigs)
        {
            $remainder = @(Get-Remainder $listValues[$name][$config.Key] @($global + $platformCommon))
            Emit-Set "    " "$($name)_$($config.Config.ToUpper())" $remainder
        }
    }

    foreach ($config in $platformConfigs)
    {
        $outDir = Convert-MacroPath $settings[$config.Key]["OutDir"] $config.Config $config.Platform
        if ($null -eq $outDir)
        {
            $outDir = "`${CMAKE_CURRENT_BINARY_DIR}/$($config.Config)"
        }

        Emit "    set(TEN_OUTPUT_DIR_$($config.Config.ToUpper()) `"$outDir`")"
    }
}

Emit "else()"
Emit "    message(FATAL_ERROR `"Platform `${TEN_PLATFORM} is not configured in the Visual Studio project.`")"
Emit "endif()"
Emit

Emit "add_executable($projectName `${TEN_SOURCES} `${TEN_HEADERS})"
Emit
Emit "set_target_properties($projectName PROPERTIES"
Emit "    OUTPUT_NAME `"$projectName`""
Emit "    CXX_STANDARD $standard"
Emit "    CXX_STANDARD_REQUIRED ON"
Emit "    CXX_EXTENSIONS OFF"
Emit "    MSVC_RUNTIME_LIBRARY `"$(Get-ConfigGenex "ClCompile.RuntimeLibrary" $runtimeMap)`""
Emit "    MSVC_DEBUG_INFORMATION_FORMAT `"$(Get-ConfigGenex "ClCompile.DebugInformationFormat" $debugFormatMap)`""
foreach ($configName in $configNames)
{
    Emit "    RUNTIME_OUTPUT_DIRECTORY_$($configName.ToUpper()) `"`${TEN_OUTPUT_DIR_$($configName.ToUpper())}`""
}
Emit ")"
Emit

$configGenex = @{}
foreach ($name in $listSettings.Keys)
{
    $configGenex[$name] = @($configNames | ForEach-Object { "`"`$<`$<CONFIG:$_>:`${$($name)_$($_.ToUpper())}>`"" }) -join " "
}

$cxxGenex = @($configNames | ForEach-Object { "`"`$<`$<AND:`$<COMPILE_LANGUAGE:CXX>,`$<CONFIG:$_>>:`${TEN_CXX_OPTIONS_$($_.ToUpper())}>`"" }) -join " "

Emit "target_compile_definitions($projectName PRIVATE `${TEN_DEFINITIONS} $($configGenex["TEN_DEFINITIONS"]))"
Emit "target_include_directories($projectName PRIVATE `${TEN_INCLUDE_DIRECTORIES} $($configGenex["TEN_INCLUDE_DIRECTORIES"]))"
Emit "target_compile_options($projectName PRIVATE `"`$<`$<COMPILE_LANGUAGE:CXX>:`${TEN_CXX_OPTIONS}>`" $cxxGenex)"
Emit "target_link_options($projectName PRIVATE `${TEN_LINK_OPTIONS} $($configGenex["TEN_LINK_OPTIONS"]))"
Emit "target_link_directories($projectName PRIVATE `${TEN_LINK_DIRECTORIES} $($configGenex["TEN_LINK_DIRECTORIES"]))"
Emit "target_link_libraries($projectName PRIVATE `${TEN_LINK_LIBRARIES} $($configGenex["TEN_LINK_LIBRARIES"]))"
Emit

if ($usesPch -and $null -ne $pchHeader)
{
    Emit "target_precompile_headers($projectName PRIVATE `"`${TEN_PROJECT_DIR}/$($pchHeader.Replace('\', '/'))`")"
    Emit "set_source_files_properties(`${TEN_NO_PCH_SOURCES} PROPERTIES SKIP_PRECOMPILE_HEADERS ON)"
    Emit
}

$preBuildCommands = @(Get-EventCommands "PreBuildEvent")
$postBuildCommands = @(Get-EventCommands "PostBuildEvent")

Emit "# Build events: PowerShell scripts shared with the Visual Studio project (Windows PowerShell or PowerShell 7)."
Emit "find_program(TEN_POWERSHELL NAMES pwsh powershell REQUIRED)"
Emit "set(TEN_TARGET_DIR `"`")"
Emit "foreach(config IN LISTS CMAKE_CONFIGURATION_TYPES)"
Emit "    string(TOUPPER `"`${config}`" configUpper)"
Emit "    string(APPEND TEN_TARGET_DIR `"`$<`$<CONFIG:`${config}>:`${TEN_OUTPUT_DIR_`${configUpper}}>`")"
Emit "endforeach()"
Emit

if ($preBuildCommands.Count -gt 0)
{
    $message = $settings[$configs[0].Key]["PreBuildEvent.Message"]
    if ($null -eq $message -or $message -eq "")
    {
        $message = "Running the pre-build event..."
    }

    Emit "add_custom_target($($projectName)PreBuild"
    foreach ($command in $preBuildCommands)
    {
        Emit "    $command"
    }

    Emit "    COMMENT `"$message`""
    Emit "    VERBATIM"
    Emit ")"
    Emit "add_dependencies($projectName $($projectName)PreBuild)"
    Emit
}

if ($postBuildCommands.Count -gt 0)
{
    Emit "add_custom_command(TARGET $projectName POST_BUILD"
    foreach ($command in $postBuildCommands)
    {
        Emit "    $command"
    }

    Emit "    VERBATIM"
    Emit ")"
    Emit
}

Emit "source_group(TREE `"`${TEN_SOLUTION_DIR}`" FILES `${TEN_SOURCES} `${TEN_HEADERS})"
Emit "set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT $projectName)"
Emit "set_target_properties($projectName PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY `"`$<TARGET_FILE_DIR:$projectName>`")"

# Write with CRLF line endings and ASCII only.

$text = ($out -join "`r`n") + "`r`n"
foreach ($char in $text.ToCharArray())
{
    if ([int]$char -gt 127)
    {
        throw "The generated file contains non-ASCII characters."
    }
}

[IO.File]::WriteAllText($OutputFile, $text, (New-Object Text.ASCIIEncoding))

Write-Host "Generated $OutputFile from $ProjectFile ($($sources.Count) sources, $($headers.Count) headers)."
foreach ($warning in $warnings)
{
    Write-Host "Warning: $warning"
}

Write-Host ""
Write-Host "Configure and build, for example:"
Write-Host "    cmake -S . -B Build/CMake -G `"Visual Studio 18 2026`" -A x64"
Write-Host "    cmake --build Build/CMake --config Release"
