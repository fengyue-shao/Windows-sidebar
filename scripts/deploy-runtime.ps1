[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$Executable,
    [Parameter(Mandatory=$true)][string]$QtBin,
    [Parameter(Mandatory=$true)][string]$SourceDir,
    [switch]$VerifyOnly
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$taskExecutable = (Resolve-Path -LiteralPath $Executable).Path
$taskDestination = Split-Path -Parent $taskExecutable
$taskSource = (Resolve-Path -LiteralPath $SourceDir).Path
if (!$VerifyOnly) {
    $taskProcessName = [IO.Path]::GetFileNameWithoutExtension($taskExecutable)
    $taskRunning = @(Get-Process -Name $taskProcessName -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $taskExecutable })
    if ($taskRunning.Count) { throw 'Close this preview application before updating its Qt runtime.' }
    $taskQtBin = (Resolve-Path -LiteralPath $QtBin).Path
    $taskDeploy = Join-Path $taskQtBin 'windeployqt.exe'
    if (!(Test-Path -LiteralPath $taskDeploy -PathType Leaf)) { throw "Missing deployment tool: $taskDeploy" }
    $taskOriginalPath = $env:PATH
    try {
        $env:PATH = $taskQtBin + ';' + $env:PATH
        & $taskDeploy --release --qmldir (Join-Path $taskSource 'qml') --no-translations --verbose 0 $taskExecutable
        if ($LASTEXITCODE -ne 0) { throw "windeployqt failed with exit code $LASTEXITCODE" }
    } finally { $env:PATH = $taskOriginalPath }
    Copy-Item -LiteralPath (Join-Path $taskSource 'packaging\qt.conf') -Destination (Join-Path $taskDestination 'qt.conf') -Force
}
# Guard the main application's known runtime requirements, including the import dialog.
# Do not scan optional plugin UI or bundle plugin executables into the main application.
$taskRequired = @(
    'qt.conf', 'Qt6Core.dll', 'Qt6Gui.dll', 'Qt6Qml.dll', 'Qt6Quick.dll',
    'Qt6Network.dll', 'Qt6Svg.dll', 'Qt6QuickControls2.dll', 'Qt6QuickLayouts.dll',
    'Qt6QuickDialogs2.dll', 'Qt6QuickDialogs2QuickImpl.dll', 'Qt6QuickDialogs2Utils.dll',
    'platforms\qwindows.dll', 'qml\QtQuick\qmldir', 'qml\QtQuick\Controls\qmldir',
    'qml\QtQuick\Layouts\qmldir', 'qml\QtQuick\Dialogs\qmldir',
    'qml\QtQuick\Dialogs\qtquickdialogsplugin.dll',
    'qml\QtQuick\Dialogs\quickimpl\qmldir',
    'qml\QtQuick\Dialogs\quickimpl\qtquickdialogs2quickimplplugin.dll'
)
$taskMissing = @($taskRequired | Where-Object { !(Test-Path -LiteralPath (Join-Path $taskDestination $_) -PathType Leaf) })
if ($taskMissing.Count) { throw ('Incomplete Qt deployment: ' + ($taskMissing -join ', ')) }
$taskConfig = Get-Content -Raw -LiteralPath (Join-Path $taskDestination 'qt.conf')
foreach ($taskEntry in @('Prefix=.', 'Plugins=.', 'QmlImports=qml')) {
    if ($taskConfig -notmatch ('(?m)^' + [regex]::Escape($taskEntry) + '\s*$')) { throw "Non-local Qt path configuration: $taskEntry" }
}
Write-Output "Qt runtime deployment verified: $taskDestination"
