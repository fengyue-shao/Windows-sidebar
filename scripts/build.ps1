[CmdletBinding()]
param(
    [string]$QtBin,
    [string]$BuildDir,
    [string]$CMake,
    [ValidateSet('Release', 'Debug')][string]$Configuration = 'Release',
    [ValidateRange(1, 64)][int]$Parallel = 2,
    [switch]$Test,
    [switch]$Deploy
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Get-FullDirectory([string]$Value) {
    $taskFullPath = [IO.Path]::GetFullPath($Value)
    if ($taskFullPath -eq [IO.Path]::GetPathRoot($taskFullPath)) { return $taskFullPath }
    return $taskFullPath.TrimEnd([IO.Path]::DirectorySeparatorChar, [IO.Path]::AltDirectorySeparatorChar)
}

function Test-SamePath([string]$Left, [string]$Right) {
    return (Get-FullDirectory $Left).Equals((Get-FullDirectory $Right), [StringComparison]::OrdinalIgnoreCase)
}

function Test-WithinPath([string]$Child, [string]$Parent) {
    $taskParent = (Get-FullDirectory $Parent) + [IO.Path]::DirectorySeparatorChar
    return (Get-FullDirectory $Child).StartsWith($taskParent, [StringComparison]::OrdinalIgnoreCase)
}

function Find-File([string[]]$Candidates) {
    foreach ($taskCandidate in $Candidates) {
        if ($taskCandidate -and (Test-Path -LiteralPath $taskCandidate -PathType Leaf)) {
            return (Resolve-Path -LiteralPath $taskCandidate).Path
        }
    }
    return $null
}

function Resolve-QtBin([string]$RequestedPath) {
    if ($RequestedPath) {
        $taskCandidates = @((Get-FullDirectory $RequestedPath))
    } else {
        $taskCandidates = @()
        if ($env:QTDIR) { $taskCandidates += Join-Path $env:QTDIR 'bin' }
        $taskQmake = Get-Command qmake.exe -ErrorAction SilentlyContinue
        if ($taskQmake) { $taskCandidates += Split-Path -Parent $taskQmake.Source }
        foreach ($taskQtRoot in @('C:\Qt', 'D:\Qt')) {
            if (Test-Path -LiteralPath $taskQtRoot -PathType Container) {
                foreach ($taskVersionDir in @(Get-ChildItem -LiteralPath $taskQtRoot -Directory | Sort-Object Name -Descending)) {
                    foreach ($taskKitDir in @(Get-ChildItem -LiteralPath $taskVersionDir.FullName -Directory -Filter 'msvc*_64' -ErrorAction SilentlyContinue)) {
                        $taskCandidates += Join-Path $taskKitDir.FullName 'bin'
                    }
                }
            }
        }
    }
    foreach ($taskCandidate in $taskCandidates) {
        $taskQmakePath = Join-Path $taskCandidate 'qmake.exe'
        if (Test-Path -LiteralPath $taskQmakePath -PathType Leaf) {
            return (Resolve-Path -LiteralPath $taskCandidate).Path
        }
    }
    throw 'No Qt MSVC x64 kit was found. Pass -QtBin "C:\Qt\<version>\msvc2022_64\bin".'
}

function Resolve-CMake([string]$RequestedPath) {
    if ($RequestedPath) {
        $taskResult = Find-File @($RequestedPath)
    } else {
        $taskCommand = Get-Command cmake.exe -ErrorAction SilentlyContinue
        if ($taskCommand) { return $taskCommand.Source }
        $taskResult = Find-File @(
            'C:\Program Files\CMake\bin\cmake.exe',
            'C:\Qt\Tools\CMake_64\bin\cmake.exe',
            'D:\Qt\Tools\CMake_64\bin\cmake.exe',
            'D:\cmake\bin\cmake.exe'
        )
    }
    if (-not $taskResult) { throw 'CMake was not found. Install CMake or pass -CMake <cmake.exe>.' }
    return $taskResult
}

function Find-VisualStudio {
    $taskVswhereCandidates = @()
    foreach ($taskProgramFiles in @(${env:ProgramFiles(x86)}, $env:ProgramFiles)) {
        if ($taskProgramFiles) {
            $taskVswhereCandidates += Join-Path $taskProgramFiles 'Microsoft Visual Studio\Installer\vswhere.exe'
        }
    }
    $taskVswhere = Find-File $taskVswhereCandidates
    if ($taskVswhere) {
        foreach ($taskVersionRange in @('[17.0,18.0)', '')) {
            $taskVswhereArguments = @('-latest', '-products', '*', '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64')
            if ($taskVersionRange) { $taskVswhereArguments += @('-version', $taskVersionRange) }
            $taskInstallPath = & $taskVswhere @taskVswhereArguments -property installationPath
            if ($LASTEXITCODE -ne 0) { throw 'vswhere could not inspect the installed C++ build tools.' }
            $taskInstallVersion = & $taskVswhere @taskVswhereArguments -property installationVersion
            if ($LASTEXITCODE -ne 0) { throw 'vswhere could not read the Visual Studio version.' }
            if ($taskInstallPath -and $taskInstallVersion) {
                $taskVcVars = Find-File @((Join-Path ([string]$taskInstallPath) 'VC\Auxiliary\Build\vcvars64.bat'))
                if ($taskVcVars) {
                    return @{ Root = [string]$taskInstallPath; Version = [version]([string]$taskInstallVersion); VcVars = $taskVcVars }
                }
            }
        }
    }
    if ($env:VSINSTALLDIR -and $env:VisualStudioVersion) {
        $taskVcVars = Find-File @((Join-Path $env:VSINSTALLDIR 'VC\Auxiliary\Build\vcvars64.bat'))
        if ($taskVcVars) {
            return @{ Root = $env:VSINSTALLDIR; Version = [version]$env:VisualStudioVersion; VcVars = $taskVcVars }
        }
    }
    throw 'MSVC x64 build tools were not found. Install Visual Studio C++ tools and a Windows 10/11 SDK.'
}

function Import-VcEnvironment([string]$VcVarsPath) {
    if ($VcVarsPath -match '["%\r\n]') { throw 'The MSVC setup path contains unsupported command-shell characters.' }
    $taskCommandLine = 'call "' + $VcVarsPath + '" >nul && set'
    $taskLines = & $env:ComSpec /d /c $taskCommandLine
    if ($LASTEXITCODE -ne 0) { throw "MSVC environment setup failed with exit code $LASTEXITCODE." }
    foreach ($taskLine in $taskLines) {
        $taskSeparator = $taskLine.IndexOf('=')
        if ($taskSeparator -gt 0) {
            $taskName = $taskLine.Substring(0, $taskSeparator)
            $taskValue = $taskLine.Substring($taskSeparator + 1)
            [Environment]::SetEnvironmentVariable($taskName, $taskValue, 'Process')
        }
    }
}

function Read-Cache([string]$CachePath) {
    $taskValues = @{}
    foreach ($taskLine in Get-Content -LiteralPath $CachePath) {
        if ($taskLine -match '^([^#/:][^:]*):[^=]+=(.*)$') {
            $taskValues[$Matches[1]] = $Matches[2]
        }
    }
    return $taskValues
}

function Assert-NinjaIncludePrefix([string]$BuildPath, [bool]$Required) {
    $taskCMakeFiles = Join-Path $BuildPath 'CMakeFiles'
    $taskCompilerFiles = @()
    if (Test-Path -LiteralPath $taskCMakeFiles -PathType Container) {
        foreach ($taskVersionDirectory in @(Get-ChildItem -LiteralPath $taskCMakeFiles -Directory)) {
            if ($taskVersionDirectory.Name -match '^\d+\.\d+\.\d+') {
                $taskCompilerFile = Join-Path $taskVersionDirectory.FullName 'CMakeCXXCompiler.cmake'
                if (Test-Path -LiteralPath $taskCompilerFile -PathType Leaf) { $taskCompilerFiles += $taskCompilerFile }
            }
        }
    }
    if ($taskCompilerFiles.Count -eq 0) {
        if ($Required) { throw 'The Ninja cache has no compiler include-prefix metadata. Choose a new -BuildDir with Visual Studio 2022.' }
        return
    }
    foreach ($taskCompilerFile in $taskCompilerFiles) {
        $taskCompilerContents = [IO.File]::ReadAllText($taskCompilerFile, [Text.Encoding]::UTF8)
        $taskPrefixMatch = [regex]::Match($taskCompilerContents, '(?m)^set\(CMAKE_CXX_CL_SHOWINCLUDES_PREFIX[ \t]+"([^"\r\n]*)"\)[ \t]*\r?$')
        if (-not $taskPrefixMatch.Success -or $taskPrefixMatch.Groups[1].Value -cnotmatch '^Note: including file:[ \t]+$') {
            throw "Unsafe Ninja MSVC dependency prefix in '$taskCompilerFile'. Select a new build directory with Visual Studio 2022, or provide an MSVC kit with matching English diagnostic resources. No cache or system language will be changed automatically."
        }
    }
}

function Assert-BuildDirectory([string]$SourcePath, [string]$BuildPath) {
    if ((Test-SamePath $BuildPath $SourcePath) -or (Test-WithinPath $BuildPath $SourcePath) -or (Test-WithinPath $SourcePath $BuildPath)) {
        throw 'Use a separate build directory outside the source tree, not the source directory or any of its parents.'
    }
    if (Test-SamePath $BuildPath ([IO.Path]::GetPathRoot($BuildPath))) {
        throw 'A drive root cannot be used as a build directory.'
    }
    $taskExistingAncestor = $BuildPath
    while ($taskExistingAncestor) {
        if (Test-Path -LiteralPath $taskExistingAncestor) {
            $taskItem = Get-Item -LiteralPath $taskExistingAncestor -Force
            if (-not $taskItem.PSIsContainer) { throw "Not a directory: $taskExistingAncestor" }
            if (($taskItem.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
                throw "Use a build path without directory junctions or symlinks: $taskExistingAncestor"
            }
        }
        $taskParent = Split-Path -Parent $taskExistingAncestor
        if (-not $taskParent -or $taskParent -eq $taskExistingAncestor) { break }
        $taskExistingAncestor = $taskParent
    }
    $taskCachePath = Join-Path $BuildPath 'CMakeCache.txt'
    if (Test-Path -LiteralPath $taskCachePath -PathType Leaf) {
        $taskCache = Read-Cache $taskCachePath
        if (-not $taskCache.ContainsKey('CMAKE_HOME_DIRECTORY') -or -not (Test-SamePath $taskCache['CMAKE_HOME_DIRECTORY'] $SourcePath)) {
            throw 'This build cache belongs to a different source directory. Choose a new, empty -BuildDir.'
        }
        foreach ($taskLegacyOption in @('SIDEBAR_BUILD_TERMINAL_PLUGIN', 'SIDEBAR_BUILD_TERMINAL_TESTS')) {
            if ($taskCache.ContainsKey($taskLegacyOption) -and $taskCache[$taskLegacyOption] -match '^(ON|TRUE|1|YES)$') {
                throw 'This cache contains the archived terminal build. Choose a new, empty -BuildDir; no cache will be removed automatically.'
            }
        }
        return $taskCache
    }
    if ((Test-Path -LiteralPath $BuildPath) -and @(Get-ChildItem -LiteralPath $BuildPath -Force).Count -gt 0) {
        throw 'The build directory is not empty and has no CMake cache. Choose a new, empty -BuildDir.'
    }
    return @{}
}

if ($env:OS -ne 'Windows_NT') { throw 'This application requires Windows.' }
if ($Deploy -and $Configuration -ne 'Release') {
    throw '-Deploy supports Release only. Build Debug without -Deploy and use the selected Qt development environment.'
}

$taskSourceDir = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
if (-not $BuildDir) { $BuildDir = Join-Path (Split-Path -Parent $taskSourceDir) 'build-host' }
$taskBuildDir = Get-FullDirectory $BuildDir
$taskCache = Assert-BuildDirectory $taskSourceDir $taskBuildDir
$taskQtBin = Resolve-QtBin $QtBin
$taskQmake = Join-Path $taskQtBin 'qmake.exe'
$taskQtVersionText = [string](& $taskQmake -query QT_VERSION)
if ($LASTEXITCODE -ne 0 -or $taskQtVersionText -notmatch '^6\.\d+\.\d+' -or [version]$taskQtVersionText -lt [version]'6.8.0') {
    throw "Qt 6.8 or newer Qt 6 is required. Selected version: $taskQtVersionText"
}
$taskQtPrefix = Split-Path -Parent $taskQtBin
$taskQtConfig = Join-Path $taskQtPrefix 'lib\cmake\Qt6\Qt6Config.cmake'
$taskQtArchitecture = Join-Path $taskQtPrefix 'mkspecs\qconfig.pri'
$taskQtSpec = [string](& $taskQmake -query QMAKE_XSPEC)
if ($LASTEXITCODE -ne 0 -or $taskQtSpec.Trim() -ne 'win32-msvc' -or
    -not (Test-Path -LiteralPath $taskQtConfig -PathType Leaf) -or
    -not (Test-Path -LiteralPath $taskQtArchitecture -PathType Leaf) -or
    (Get-Content -Raw -LiteralPath $taskQtArchitecture) -notmatch '(?m)^QT_ARCH\s*=\s*x86_64\s*$') {
    throw 'The selected Qt kit must be an MSVC x64 desktop kit, not MinGW, ARM64, or x86.'
}
$taskCMake = Resolve-CMake $CMake
$taskCMakeVersionLines = @(& $taskCMake --version)
if ($LASTEXITCODE -ne 0 -or $taskCMakeVersionLines.Count -eq 0 -or
    $taskCMakeVersionLines[0] -notmatch '^cmake version (\d+\.\d+\.\d+)' -or
    [version]$Matches[1] -lt [version]'3.21.0') {
    throw 'This build entry requires CMake 3.21 or newer (including Visual Studio 2022 and CTest support).'
}
$taskCTest = Join-Path (Split-Path -Parent $taskCMake) 'ctest.exe'
if ($Test -and -not (Test-Path -LiteralPath $taskCTest -PathType Leaf)) { throw 'ctest.exe is missing beside cmake.exe.' }
$taskVisualStudio = Find-VisualStudio
$taskOriginalEnvironment = [Environment]::GetEnvironmentVariables('Process')

try {
    Import-VcEnvironment $taskVisualStudio.VcVars
    $taskCompiler = Get-Command cl.exe -ErrorAction SilentlyContinue
    if (-not $taskCompiler -or $env:VSCMD_ARG_TGT_ARCH -ne 'x64') { throw 'The MSVC x64 compiler environment is unavailable.' }
    if (-not $env:WindowsSdkDir -or -not (Test-Path -LiteralPath $env:WindowsSdkDir -PathType Container)) {
        throw 'A Windows 10/11 SDK was not found in the MSVC environment.'
    }
    $taskNinjaCommand = Get-Command ninja.exe -ErrorAction SilentlyContinue
    $taskNinja = $null
    if ($taskNinjaCommand) { $taskNinja = $taskNinjaCommand.Source }
    if (-not $taskNinja) {
        $taskQtRoot = Split-Path -Parent (Split-Path -Parent $taskQtPrefix)
        $taskNinja = Find-File @(
            (Join-Path $taskQtRoot 'Tools\Ninja\ninja.exe'),
            (Join-Path (Split-Path -Parent $taskCMake) 'ninja.exe')
        )
    }
    $taskGenerator = $null
    if ($taskCache.ContainsKey('CMAKE_GENERATOR')) {
        $taskGenerator = $taskCache['CMAKE_GENERATOR']
        if ($taskGenerator -notin @('Ninja', 'Visual Studio 17 2022')) {
            throw "Unsupported cached generator '$taskGenerator'. Choose a new -BuildDir."
        }
    } elseif ($taskVisualStudio.Version.Major -eq 17) {
        $taskGenerator = 'Visual Studio 17 2022'
    } elseif ($taskNinja) {
        $taskGenerator = 'Ninja'
    } else {
        throw 'Install Ninja or use Visual Studio 2022. This script never downloads build tools.'
    }
    $taskConfigure = @('-S', $taskSourceDir, '-B', $taskBuildDir, '-G', $taskGenerator,
        "-DCMAKE_PREFIX_PATH=$taskQtPrefix", "-DQt6_DIR=$(Split-Path -Parent $taskQtConfig)",
        '-DSIDEBAR_BUILD_INTEGRATION_TESTS=OFF')
    if ($Test) { $taskConfigure += '-DSIDEBAR_BUILD_TESTS=ON' } else { $taskConfigure += '-DSIDEBAR_BUILD_TESTS=OFF' }
    foreach ($taskQtCacheKey in @('Qt6_DIR', 'Qt6Core_DIR')) {
        if ($taskCache.ContainsKey($taskQtCacheKey)) {
            $taskQtComponent = $taskQtCacheKey.Substring(0, $taskQtCacheKey.Length - 4)
            $taskExpectedQtDir = Join-Path $taskQtPrefix ('lib\cmake\' + $taskQtComponent)
            if (-not (Test-SamePath $taskCache[$taskQtCacheKey] $taskExpectedQtDir)) {
                throw 'The build cache uses a different Qt kit. Choose a new -BuildDir; do not mix Qt DLLs.'
            }
        }
    }
    if ($taskGenerator -eq 'Ninja') {
        if (-not $taskNinja) { throw 'This cache requires Ninja, but ninja.exe was not found.' }
        Assert-NinjaIncludePrefix $taskBuildDir ($taskCache.Count -gt 0)
        if ($taskCache.ContainsKey('CMAKE_BUILD_TYPE') -and $taskCache['CMAKE_BUILD_TYPE'] -ne $Configuration) {
            throw 'A Ninja cache uses one configuration. Use a separate -BuildDir for Debug and Release.'
        }
        if ($taskCache.ContainsKey('CMAKE_CXX_COMPILER') -and -not (Test-SamePath $taskCache['CMAKE_CXX_COMPILER'] $taskCompiler.Source)) {
            throw 'The Ninja cache uses a different compiler. Choose a new -BuildDir.'
        }
        $taskConfigure += @("-DCMAKE_BUILD_TYPE=$Configuration", "-DCMAKE_MAKE_PROGRAM=$taskNinja", "-DCMAKE_CXX_COMPILER=$($taskCompiler.Source)")
    } else {
        if ($taskVisualStudio.Version.Major -ne 17) { throw 'This cache requires Visual Studio 2022.' }
        if ($taskCache.ContainsKey('CMAKE_GENERATOR_PLATFORM') -and $taskCache['CMAKE_GENERATOR_PLATFORM'] -ne 'x64') {
            throw 'The Visual Studio cache is not x64. Choose a new -BuildDir.'
        }
        if ($taskCache.ContainsKey('CMAKE_GENERATOR_INSTANCE') -and $taskCache['CMAKE_GENERATOR_INSTANCE'] -and
            -not (Test-SamePath $taskCache['CMAKE_GENERATOR_INSTANCE'] $taskVisualStudio.Root)) {
            throw 'The build cache uses a different Visual Studio installation. Choose a new -BuildDir.'
        }
        $taskConfigure += @('-A', 'x64', "-DCMAKE_GENERATOR_INSTANCE=$($taskVisualStudio.Root)")
    }

    Write-Host "Source: $taskSourceDir"
    Write-Host "Build:  $taskBuildDir"
    Write-Host "Qt:     $taskQtBin ($taskQtVersionText)"
    Write-Host "CMake:  $taskCMake"
    Write-Host "MSVC:   $($taskCompiler.Source)"
    Write-Host "Mode:   $Configuration; generator=$taskGenerator; tests=$([bool]$Test); deploy=$([bool]$Deploy)"

    & $taskCMake @taskConfigure
    if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed with exit code $LASTEXITCODE." }
    if ($taskGenerator -eq 'Ninja') { Assert-NinjaIncludePrefix $taskBuildDir $true }
    $taskBuild = @('--build', $taskBuildDir, '--config', $Configuration, '--parallel', [string]$Parallel)
    if (-not $Test) { $taskBuild += @('--target', 'appqt_rainmeter') }
    if ($taskGenerator -like 'Visual Studio *') { $taskBuild += @('--', '/nodeReuse:false') }
    & $taskCMake @taskBuild
    if ($LASTEXITCODE -ne 0) { throw "Build failed with exit code $LASTEXITCODE." }

    if ($Test) {
        $env:Path = $taskQtBin + ';' + $env:Path
        $env:QT_PLUGIN_PATH = Join-Path $taskQtPrefix 'plugins'
        $env:QML_IMPORT_PATH = Join-Path $taskQtPrefix 'qml'
        $env:QML2_IMPORT_PATH = $env:QML_IMPORT_PATH
        & $taskCTest --test-dir $taskBuildDir -C $Configuration --output-on-failure --parallel $Parallel --no-tests=error
        if ($LASTEXITCODE -ne 0) { throw "CTest failed with exit code $LASTEXITCODE. See $taskBuildDir\Testing\Temporary\LastTest.log." }
    }
    if ($Deploy) {
        $taskDeployArguments = @('--build', $taskBuildDir, '--config', $Configuration, '--target', 'deploy_runtime', '--parallel', [string]$Parallel)
        if ($taskGenerator -like 'Visual Studio *') { $taskDeployArguments += @('--', '/nodeReuse:false') }
        & $taskCMake @taskDeployArguments
        if ($LASTEXITCODE -ne 0) { throw "Runtime deployment failed with exit code $LASTEXITCODE." }
    }
    $taskExecutableDir = $taskBuildDir
    if ($taskGenerator -like 'Visual Studio *') { $taskExecutableDir = Join-Path $taskBuildDir $Configuration }
    Write-Host "Build complete: $(Join-Path $taskExecutableDir 'appqt_rainmeter.exe')"
    if ($Test) { Write-Host 'The registered host-only CTest suite passed. This is not hardware or installed-package acceptance.' }
    if ($Deploy) { Write-Host 'Local Release Qt runtime deployed. No package was signed or installed.' }
    else { Write-Host 'Build only: no Qt runtime deployment, certificate, installation, or application launch.' }
} finally {
    $taskCurrentEnvironment = [Environment]::GetEnvironmentVariables('Process')
    foreach ($taskKey in @($taskCurrentEnvironment.Keys)) {
        if (-not $taskOriginalEnvironment.Contains($taskKey)) { [Environment]::SetEnvironmentVariable([string]$taskKey, $null, 'Process') }
    }
    foreach ($taskKey in $taskOriginalEnvironment.Keys) {
        if (-not $taskCurrentEnvironment.Contains($taskKey) -or [string]$taskCurrentEnvironment[$taskKey] -cne [string]$taskOriginalEnvironment[$taskKey]) {
            [Environment]::SetEnvironmentVariable([string]$taskKey, [string]$taskOriginalEnvironment[$taskKey], 'Process')
        }
    }
}
