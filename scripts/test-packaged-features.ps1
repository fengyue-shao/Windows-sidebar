param(
    [Parameter(Mandatory = $true)][string]$LogPath
)
$ErrorActionPreference = 'Stop'
$package = Get-AppxPackage -Name WindowsSidebar | Select-Object -First 1
if (-not $package) { throw 'WindowsSidebar is not installed.' }
$resolvedLog = [System.IO.Path]::GetFullPath($LogPath)
if ($resolvedLog.Contains('"')) { throw 'Invalid log path.' }
if (Test-Path -LiteralPath $resolvedLog) { throw 'Use a new log path to avoid stale test results.' }
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
[ComImport, Guid("2e941141-7f97-4756-ba1d-9decde894a3d"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
interface ISidebarActivationManager {
    [PreserveSig] int ActivateApplication([MarshalAs(UnmanagedType.LPWStr)] string appId,
        [MarshalAs(UnmanagedType.LPWStr)] string arguments, uint options, out uint processId);
}
[ComImport, Guid("45ba127d-10a8-46ea-8ab7-56ea9078943c")]
class SidebarActivationManager {}
public static class SidebarFeatureActivation {
    public static uint Start(string appId, string arguments) {
        var manager = (ISidebarActivationManager)new SidebarActivationManager();
        try {
            uint processId;
            Marshal.ThrowExceptionForHR(manager.ActivateApplication(appId, arguments, 0, out processId));
            return processId;
        } finally { Marshal.ReleaseComObject(manager); }
    }
}
'@
$appId = $package.PackageFamilyName + '!WindowsSidebar'
$taskProcessId = [SidebarFeatureActivation]::Start($appId, '--feature-self-test="' + $resolvedLog + '"')
try {
    $taskProcess = [System.Diagnostics.Process]::GetProcessById($taskProcessId)
    if (-not $taskProcess.WaitForExit(30000)) { throw 'Packaged feature test timed out.' }
} catch [System.ArgumentException] {
    # A fast test can exit before GetProcessById; the fresh log is authoritative.
}
if (-not (Test-Path -LiteralPath $resolvedLog)) { throw 'Feature test did not create its log.' }
$output = Get-Content -LiteralPath $resolvedLog -Raw -Encoding UTF8
Write-Output $output
if ($output -match '(?m)^FAIL ' -or $output -notmatch 'FEATURE_TEST PASS') { throw 'Packaged feature test failed.' }
