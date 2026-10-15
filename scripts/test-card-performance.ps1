param(
    [string]$BuildDirectory = "",
    [Parameter(Mandatory=$true)][string]$QtRoot,
    [string]$OutputDirectory = "",
    [ValidateSet('Release','Debug')][string]$Configuration = 'Release',
    [switch]$ProfileQml
)
$ErrorActionPreference = "Stop"
$taskRepo = Split-Path $PSScriptRoot -Parent
$taskWorkspace = Split-Path $taskRepo -Parent
if (!$BuildDirectory) { $BuildDirectory = Join-Path $taskWorkspace "build-host" }
if (!$OutputDirectory) { $OutputDirectory = Join-Path $taskWorkspace ("work\performance\" + [guid]::NewGuid().ToString('N')) }
$taskBin = if (Test-Path -LiteralPath (Join-Path $BuildDirectory ($Configuration+'\sidebar-card-layout-benchmark.exe'))) { Join-Path $BuildDirectory $Configuration } else { $BuildDirectory }
$taskBenchmark = Join-Path $taskBin "sidebar-card-layout-benchmark.exe"
$taskCanvas = Join-Path $taskBin "sidebar-card-canvas-test.exe"
$taskManager = Join-Path $taskBin "sidebar-card-manager-test.exe"
$taskDesktopManager = Join-Path $taskBin "sidebar-desktop-card-manager-test.exe"
$taskDesktopCanvas = Join-Path $taskBin "sidebar-desktop-card-canvas-test.exe"
$taskDesktopFixture = Join-Path $taskRepo "tests\DesktopCardCanvasFixture.qml"
$taskProfiler = Join-Path $QtRoot "bin\qmlprofiler.exe"
$taskFixture = Join-Path $taskRepo "tests\NativeCardCanvasFixture.qml"
$taskRequired = @($taskBenchmark, $taskCanvas, $taskManager, $taskFixture, $taskDesktopManager, $taskDesktopCanvas, $taskDesktopFixture, (Join-Path $QtRoot "plugins\platforms\qoffscreen.dll"))
if ($ProfileQml) { $taskRequired += $taskProfiler }
foreach ($taskFile in $taskRequired) {
    if (!(Test-Path -LiteralPath $taskFile -PathType Leaf)) { throw "Missing prerequisite: $taskFile. Build the card test targets first." }
}
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$taskEnvNames = @("PATH", "QT_QPA_PLATFORM", "QT_QUICK_BACKEND", "QT_PLUGIN_PATH", "QT_QPA_PLATFORM_PLUGIN_PATH", "QT_QPA_FONTDIR", "SIDEBAR_QML_PROFILE")
$taskSavedEnv = @{}
foreach ($taskName in $taskEnvNames) { $taskSavedEnv[$taskName] = [Environment]::GetEnvironmentVariable($taskName, "Process") }
try {
    $env:PATH = (Join-Path $QtRoot "bin") + ";" + $env:PATH
    $env:QT_QPA_PLATFORM = "offscreen"
    $env:QT_QUICK_BACKEND = "software"
    $env:QT_PLUGIN_PATH = Join-Path $QtRoot "plugins"
    $env:QT_QPA_PLATFORM_PLUGIN_PATH = Join-Path $QtRoot "plugins\platforms"
    $env:QT_QPA_FONTDIR = "C:\Windows\Fonts"
    $env:SIDEBAR_QML_PROFILE = "0"
    & $taskManager
    if ($LASTEXITCODE -ne 0) { throw "Card manager test failed: $LASTEXITCODE" }
    & $taskDesktopManager
    if ($LASTEXITCODE -ne 0) { throw "Desktop card manager test failed: $LASTEXITCODE" }
    & $taskDesktopCanvas $taskDesktopFixture (Join-Path $OutputDirectory "desktop-canvas")
    if ($LASTEXITCODE -ne 0) { throw "Desktop card canvas test failed: $LASTEXITCODE" }
    & $taskCanvas $taskFixture (Join-Path $OutputDirectory "card-canvas.png")
    if ($LASTEXITCODE -ne 0) { throw "Card canvas test failed: $LASTEXITCODE" }
    & $taskBenchmark -median 5 -minimumtotal 200 -o ((Join-Path $OutputDirectory "card-layout.txt") + ",txt")
    if ($LASTEXITCODE -ne 0) { throw "Card benchmark failed: $LASTEXITCODE" }
    if ($ProfileQml) {
        $env:SIDEBAR_QML_PROFILE = "1"
        $taskTrace = Join-Path $OutputDirectory "card-canvas.qmltrace"
        & $taskProfiler --port 37981 --include creating,binding,handlingsignal,javascript,painting,inputevents --output $taskTrace $taskCanvas $taskFixture (Join-Path $OutputDirectory "card-canvas-profile.png")
        if ($LASTEXITCODE -ne 0) { throw "QML Profiler failed: $LASTEXITCODE" }
        if (!(Test-Path -LiteralPath $taskTrace)) { throw "QML Profiler produced no trace" }
        [xml]$taskXml = Get-Content -Raw -LiteralPath $taskTrace
        if (@($taskXml.trace.profilerDataModel.range).Count -eq 0) { throw "QML trace contains no events" }
    }
    Write-Output "Card regression and performance results: $OutputDirectory"
}
finally {
    foreach ($taskName in $taskEnvNames) { [Environment]::SetEnvironmentVariable($taskName, $taskSavedEnv[$taskName], "Process") }
}
