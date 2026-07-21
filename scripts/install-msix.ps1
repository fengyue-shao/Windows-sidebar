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
$trusted = Get-ChildItem Cert:\CurrentUser\Root |
    Where-Object { $_.Thumbprint -eq $certificate.Thumbprint } |
    Select-Object -First 1

if (-not $trusted -and $TrustDevelopmentCertificate) {
    Write-Host 'Windows will ask you to confirm trusting the development certificate.'
    & certutil.exe -user -addstore Root $certificatePath
    if ($LASTEXITCODE -ne 0) {
        throw "Certificate import failed with exit code $LASTEXITCODE"
    }
    $trusted = Get-ChildItem Cert:\CurrentUser\Root |
        Where-Object { $_.Thumbprint -eq $certificate.Thumbprint } |
        Select-Object -First 1
}

if (-not $trusted) {
    throw 'The development certificate is not trusted. Re-run with -TrustDevelopmentCertificate and accept the Windows security prompt.'
}

Add-AppxPackage -Path $resolvedPackage -ForceApplicationShutdown -ForceUpdateFromAnyVersion

$package = Get-AppxPackage -Name 'WindowsSidebar' | Select-Object -First 1
if (-not $package) {
    throw 'WindowsSidebar was not found after installation.'
}

Write-Host "Installed package: $($package.PackageFullName)"
Write-Host 'Launch Windows Sidebar from the Start menu so it receives package identity.'
Write-Host 'Do not launch appqt_rainmeter.exe directly from the build directory.'
