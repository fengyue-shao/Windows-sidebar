param([Parameter(Mandatory=$true)][string]$BuildDirectory)
$ErrorActionPreference='Stop'
$buildPath=(Resolve-Path -LiteralPath $BuildDirectory).Path
$fixture=Join-Path $buildPath 'sidebar-media-fixture.exe'
$probe=Join-Path $buildPath 'sidebar-media-integration.exe'
$owned=@()
$probeLog=[System.IO.Path]::GetTempFileName()
$previousLogging=$env:QT_FORCE_STDERR_LOGGING
$env:QT_FORCE_STDERR_LOGGING='1'
try {
    $owned += Start-Process -FilePath $fixture -ArgumentList A -WorkingDirectory $buildPath -WindowStyle Hidden -PassThru
    $owned += Start-Process -FilePath $fixture -ArgumentList B -WorkingDirectory $buildPath -WindowStyle Hidden -PassThru
    # Qt executables use the GUI subsystem: PowerShell's call operator does not
    # reliably wait for them or populate LASTEXITCODE. Check the owned process.
    $result=Start-Process -FilePath $probe -WorkingDirectory $buildPath -WindowStyle Hidden -RedirectStandardError $probeLog -Wait -PassThru
    $output=Get-Content -LiteralPath $probeLog -Raw
    Write-Output $output
    if ($result.ExitCode -ne 0 -or $output -notmatch 'MEDIA_INTEGRATION PASS') { throw "Native media test failed: $($result.ExitCode)" }
} finally {
    foreach ($item in $owned) {
        $live=Get-Process -Id $item.Id -ErrorAction SilentlyContinue
        if ($live -and $live.Path -eq $fixture) { Stop-Process -Id $live.Id }
    }
    $env:QT_FORCE_STDERR_LOGGING=$previousLogging
    Remove-Item -LiteralPath $probeLog -ErrorAction SilentlyContinue
}
