param(
    [Parameter(Mandatory = $true)][string]$LogPath,
    [Parameter(Mandatory = $true)][string]$CapturePath
)
$ErrorActionPreference = 'Stop'
# Runs only safe feature tests and loads the package-identity activation helper.
& (Join-Path $PSScriptRoot 'test-packaged-features.ps1') -LogPath $LogPath
$resolvedCapture = [System.IO.Path]::GetFullPath($CapturePath)
if ($resolvedCapture.Contains('"')) { throw 'Invalid capture path.' }
if (Test-Path -LiteralPath $resolvedCapture) { throw 'Use a fresh capture path.' }
Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
public static class SidebarProcessExit {
    [DllImport("kernel32.dll", SetLastError=true)] static extern IntPtr OpenProcess(uint access, bool inherit, uint processId);
    [DllImport("kernel32.dll", SetLastError=true)] static extern uint WaitForSingleObject(IntPtr handle, uint milliseconds);
    [DllImport("kernel32.dll", SetLastError=true)] static extern bool GetExitCodeProcess(IntPtr handle, out uint exitCode);
    [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr handle);
    public static uint Wait(uint processId) {
        var handle = OpenProcess(0x00101000, false, processId);
        if (handle == IntPtr.Zero) throw new Win32Exception(Marshal.GetLastWin32Error());
        try {
            if (WaitForSingleObject(handle, 20000) != 0) throw new Exception("Packaged UI wait failed or timed out.");
            uint code;
            if (!GetExitCodeProcess(handle, out code)) throw new Win32Exception(Marshal.GetLastWin32Error());
            return code;
        } finally { CloseHandle(handle); }
    }
}
'@
$package = Get-AppxPackage -Name WindowsSidebar | Select-Object -First 1
$taskUiId = [SidebarFeatureActivation]::Start($package.PackageFamilyName + '!WindowsSidebar', '--ui-self-test "--ui-capture=' + $resolvedCapture + '"')
$taskExitCode = [SidebarProcessExit]::Wait($taskUiId)
if ($taskExitCode -ne 0) { throw "Packaged UI self-test exited with $taskExitCode" }
foreach ($suffix in @('', '.layout.png', '.island.png', '.features.png', '.ocr.png')) {
    $capture = Get-Item -LiteralPath ($resolvedCapture + $suffix)
    if ($capture.Length -eq 0) { throw "Empty capture: $($capture.FullName)" }
}
Write-Output 'PACKAGED_UI_TEST PASS: native exit code 0; five fresh captures.'
