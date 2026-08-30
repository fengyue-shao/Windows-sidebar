[CmdletBinding()]
param(
    [string]$QtBin,
    [string]$BuildDir,
    [switch]$SkipInstall,
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Find-FirstExistingPath([string[]]$Candidates) {
    foreach ($candidate in $Candidates) {
        if ($candidate -and (Test-Path -LiteralPath $candidate)) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }
    return $null
}

function Find-QtBin([string]$RequestedPath) {
    if ($RequestedPath) {
        $resolved = (Resolve-Path -LiteralPath $RequestedPath).Path
        if (-not (Test-Path -LiteralPath (Join-Path $resolved 'qmake.exe'))) {
            throw "qmake.exe was not found in the supplied Qt bin directory: $resolved"
        }
        return $resolved
    }

    $candidates = @()
    if ($env:QTDIR) {
        $candidates += (Join-Path $env:QTDIR 'bin')
    }
    foreach ($qtRoot in @('C:\Qt', 'D:\Qt')) {
        if (Test-Path -LiteralPath $qtRoot) {
            $candidates += Get-ChildItem -Path $qtRoot -Filter qmake.exe -File -Recurse -ErrorAction SilentlyContinue |
                Where-Object { $_.DirectoryName -match 'msvc\d*_64\\bin$' } |
                Sort-Object FullName -Descending |
                ForEach-Object DirectoryName
        }
    }

    $result = Find-FirstExistingPath $candidates
    if (-not $result) {
        throw 'A 64-bit MSVC Qt installation was not found. Pass -QtBin "C:\Qt\<version>\msvc2022_64\bin".'
    }
    return $result
}

function Find-CMake {
    $command = Get-Command cmake.exe -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    $candidates = @(
        'C:\Qt\Tools\CMake_64\bin\cmake.exe',
        'D:\Qt\Tools\CMake_64\bin\cmake.exe',
        'C:\Program Files\CMake\bin\cmake.exe'
    )
    $result = Find-FirstExistingPath $candidates
    if (-not $result) {
        throw 'CMake was not found. Install CMake or add cmake.exe to PATH.'
    }
    return $result
}

function Find-VcVars {
    if ($env:VSINSTALLDIR) {
        $candidate = Join-Path $env:VSINSTALLDIR 'VC\Auxiliary\Build\vcvars64.bat'
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }

    $vswhere = Find-FirstExistingPath @(
        (Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'),
        (Join-Path $env:ProgramFiles 'Microsoft Visual Studio\Installer\vswhere.exe')
    )
    if ($vswhere) {
        $installPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($installPath) {
            $candidate = Join-Path $installPath 'VC\Auxiliary\Build\vcvars64.bat'
            if (Test-Path -LiteralPath $candidate) {
                return $candidate
            }
        }
    }
    throw 'Visual Studio C++ x64 build tools were not found. Install the Desktop development with C++ workload.'
}

function Import-VcEnvironment([string]$VcVarsPath) {
    $commandLine = "call `"$VcVarsPath`" >nul && set"
    $environmentLines = & $env:ComSpec /d /c $commandLine
    if ($LASTEXITCODE -ne 0) {
        throw "vcvars64.bat failed with exit code $LASTEXITCODE"
    }
    $vcPath = $null
    foreach ($line in $environmentLines) {
        $separator = $line.IndexOf('=')
        if ($separator -gt 0) {
            $name = $line.Substring(0, $separator)
            $value = $line.Substring($separator + 1)
            if ($name -ceq 'PATH') {
                $vcPath = $value
            } elseif ($name.Equals('Path', [StringComparison]::OrdinalIgnoreCase)) {
                if (-not $vcPath) {
                    $vcPath = $value
                }
            } else {
                [Environment]::SetEnvironmentVariable($name, $value, 'Process')
            }
        }
    }
    if (-not $vcPath) {
        throw 'vcvars64.bat did not return a compiler PATH.'
    }
    [Environment]::SetEnvironmentVariable('Path', $vcPath, 'Process')
}

if ($env:OS -ne 'Windows_NT') {
    throw 'This deployment script only supports Windows.'
}

$sourceDir = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
if (-not $BuildDir) {
    $BuildDir = Join-Path $sourceDir 'build\test-machine-release'
}
$resolvedBuildDir = [IO.Path]::GetFullPath($BuildDir)
$resolvedQtBin = Find-QtBin $QtBin
$cmake = Find-CMake
$vcVars = Find-VcVars

Write-Host "Qt:     $resolvedQtBin"
Write-Host "CMake:  $cmake"
Write-Host "MSVC:   $vcVars"
Write-Host "Build:  $resolvedBuildDir"

Import-VcEnvironment $vcVars

if (-not (Get-Command nmake.exe -ErrorAction SilentlyContinue)) {
    throw 'The MSVC environment loaded successfully, but nmake.exe is unavailable. Repair the Visual Studio C++ build tools installation.'
}

if ($Clean -and (Test-Path -LiteralPath $resolvedBuildDir)) {
    $resolvedSource = (Resolve-Path -LiteralPath $sourceDir).Path
    $resolvedBuild = (Resolve-Path -LiteralPath $resolvedBuildDir).Path
    if (-not $resolvedBuild.StartsWith($resolvedSource + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to clean a build directory outside the project: $resolvedBuild"
    }
    Remove-Item -LiteralPath $resolvedBuild -Recurse -Force
}

$qtPrefix = Split-Path -Parent $resolvedQtBin
& $cmake -S $sourceDir -B $resolvedBuildDir -G 'NMake Makefiles' `
    -DCMAKE_BUILD_TYPE=Release `
    "-DCMAKE_PREFIX_PATH=$qtPrefix"
if ($LASTEXITCODE -ne 0) {
    throw "CMake configuration failed with exit code $LASTEXITCODE"
}

& $cmake --build $resolvedBuildDir --target package_msix
if ($LASTEXITCODE -ne 0) {
    throw "Build or MSIX packaging failed with exit code $LASTEXITCODE"
}

$packagePath = Join-Path $resolvedBuildDir 'msix\WindowsSidebar.msix'
if (-not $SkipInstall) {
    & (Join-Path $PSScriptRoot 'install-msix.ps1') `
        -PackagePath $packagePath `
        -TrustDevelopmentCertificate
    if ($LASTEXITCODE -ne 0) {
        throw "MSIX installation failed with exit code $LASTEXITCODE"
    }
}

Write-Host ''
Write-Host 'Build and packaging completed successfully.' -ForegroundColor Green
Write-Host "Package: $packagePath"
if ($SkipInstall) {
    Write-Host 'Installation was skipped.'
} else {
    Write-Host 'Windows Sidebar is installed. Launch it from the Start menu.'
}
