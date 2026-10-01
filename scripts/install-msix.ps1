param(
    [Parameter(Mandatory = $true)]
    [string]$PackagePath,

    [switch]$TrustDevelopmentCertificate
)

$ErrorActionPreference = 'Stop'
$resolvedPackage = (Resolve-Path -LiteralPath $PackagePath).Path
$certificatePath = Join-Path (Split-Path -Parent $resolvedPackage) 'WindowsSidebarDev.cer'
if (-not (Test-Path -LiteralPath $certificatePath)) {
    throw "Development certificate was not found: $certificatePath"
}

$certificate = New-Object System.Security.Cryptography.X509Certificates.X509Certificate2($certificatePath)
$trusted = Get-ChildItem Cert:\LocalMachine\TrustedPeople |
    Where-Object { $_.Thumbprint -eq $certificate.Thumbprint } |
    Select-Object -First 1

if (-not $trusted -and $TrustDevelopmentCertificate) {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($identity)
    $isAdministrator = $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)

    if (-not $isAdministrator) {
        Write-Host 'Windows will ask for administrator approval to trust the development certificate.'
        $arguments = @(
            '-NoProfile',
            '-ExecutionPolicy', 'Bypass',
            '-File', $PSCommandPath,
            '-PackagePath', $resolvedPackage,
            '-TrustDevelopmentCertificate'
        )
        $process = Start-Process -FilePath 'powershell.exe' -Verb RunAs `
            -ArgumentList $arguments -WindowStyle Hidden -Wait -PassThru
        if ($process.ExitCode -ne 0) {
            throw "Elevated MSIX installation failed with exit code $($process.ExitCode)"
        }
        exit 0
    }

    Import-Certificate -FilePath $certificatePath `
        -CertStoreLocation 'Cert:\LocalMachine\TrustedPeople' | Out-Null
    $trusted = Get-ChildItem Cert:\LocalMachine\TrustedPeople |
        Where-Object { $_.Thumbprint -eq $certificate.Thumbprint } |
        Select-Object -First 1
}

if (-not $trusted) {
    throw 'The development certificate is not trusted by the local machine. Re-run with -TrustDevelopmentCertificate and accept the administrator prompt.'
}

Add-AppxPackage -Path $resolvedPackage -ForceApplicationShutdown -ForceUpdateFromAnyVersion

$package = Get-AppxPackage -Name 'WindowsSidebar' | Select-Object -First 1
if (-not $package) {
    throw 'WindowsSidebar was not found after installation.'
}

Write-Host "Installed package: $($package.PackageFullName)"
Write-Host 'Launch Windows Sidebar from the Start menu so it receives package identity.'
Write-Host 'Do not launch appqt_rainmeter.exe directly from the build directory.'
