param([Parameter(Mandatory = $true)][string]$Executable, [string]$CapturePath = '')
$ErrorActionPreference = 'Stop'
$resolvedExecutable = (Resolve-Path -LiteralPath $Executable).Path
$startInfo = New-Object System.Diagnostics.ProcessStartInfo
$startInfo.FileName = $resolvedExecutable
$startInfo.Arguments = '--ui-self-test'
if ($CapturePath) {
    if ($CapturePath.Contains('"')) { throw 'Invalid capture path' }
    $startInfo.Arguments += ' "--ui-capture=' + [System.IO.Path]::GetFullPath($CapturePath) + '"'
}
$startInfo.WorkingDirectory = Split-Path -Parent $resolvedExecutable
$startInfo.UseShellExecute = $false
$startInfo.CreateNoWindow = $true
$startInfo.RedirectStandardOutput = $true
$startInfo.RedirectStandardError = $true
$startInfo.StandardErrorEncoding = [System.Text.Encoding]::UTF8
$startInfo.StandardOutputEncoding = [System.Text.Encoding]::UTF8
$startInfo.EnvironmentVariables['QT_FORCE_STDERR_LOGGING'] = '1'
$process = New-Object System.Diagnostics.Process
$process.StartInfo = $startInfo
[void]$process.Start()
$stdoutTask = $process.StandardOutput.ReadToEndAsync()
$stderrTask = $process.StandardError.ReadToEndAsync()
if (-not $process.WaitForExit(15000)) {
    $process.Kill()
    throw 'Dropdown UI smoke test timed out.'
}
$output = $stdoutTask.Result + $stderrTask.Result
Write-Output $output
if ($process.ExitCode -ne 0 -or $output -notmatch 'UI_SELF_TEST PASS') {
    throw "Dropdown UI smoke test failed: exit=$($process.ExitCode)"
}
if ($output -match 'ReferenceError|TypeError|Binding loop|failed to load component|Cannot assign|Invalid property|Failed to get image|Error:|invalid dimensions|does not support customization') {
    throw 'QML runtime diagnostics reported an error.'
}
Write-Output 'Dropdown UI smoke test passed.'
