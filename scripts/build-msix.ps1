param(
    [Parameter(Mandatory = $true)]
    [string]$Executable,

    [Parameter(Mandatory = $true)]
    [string]$QtBin,

    [Parameter(Mandatory = $true)]
    [string]$SourceDir,

    [Parameter(Mandatory = $true)]
    [string]$OutputDir
)

$ErrorActionPreference = 'Stop'

function Find-WindowsSdkTool([string]$Name) {
    $sdkBins = @()
    if ($env:WindowsSdkDir) {
        $sdkBins += Join-Path $env:WindowsSdkDir 'bin'
    }
    $sdkBins += @(
        (Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\bin'),
        'C:\Windows Kits\10\bin',
        'D:\Windows Kits\10\bin'
    )
    $tool = $sdkBins |
        Where-Object { Test-Path -LiteralPath $_ } |
        Select-Object -Unique |
        ForEach-Object { Get-ChildItem -LiteralPath $_ -Filter $Name -File -Recurse } |
        Where-Object { $_.FullName -match '\\x64\\' } |
        Sort-Object FullName -Descending |
        Select-Object -First 1
    if (-not $tool) {
        throw "$Name was not found. Install the Windows SDK first."
    }
    return $tool.FullName
}

$resolvedExecutable = (Resolve-Path -LiteralPath $Executable).Path
$resolvedQtBin = (Resolve-Path -LiteralPath $QtBin).Path
$resolvedSourceDir = (Resolve-Path -LiteralPath $SourceDir).Path
$resolvedOutputDir = [System.IO.Path]::GetFullPath($OutputDir)
$stageDir = Join-Path $resolvedOutputDir 'stage'
$packagePath = Join-Path $resolvedOutputDir 'WindowsSidebar.msix'

if (Test-Path -LiteralPath $stageDir) {
    $resolvedStage = (Resolve-Path -LiteralPath $stageDir).Path
    if (-not $resolvedStage.StartsWith($resolvedOutputDir, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to clean stage directory outside output directory: $resolvedStage"
    }
    Remove-Item -LiteralPath $resolvedStage -Recurse -Force
}
New-Item -ItemType Directory -Path $stageDir -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $stageDir 'Assets') -Force | Out-Null

Copy-Item -LiteralPath $resolvedExecutable -Destination $stageDir
$runnerPath = Join-Path (Split-Path $resolvedExecutable) 'sidebar-terminal-runner.exe'
if (!(Test-Path -LiteralPath $runnerPath)) { throw "Terminal Runner missing: $runnerPath" }
Copy-Item -LiteralPath $runnerPath -Destination $stageDir
Copy-Item -LiteralPath (Join-Path $resolvedSourceDir 'plugins') -Destination $stageDir -Recurse
Copy-Item -LiteralPath (Join-Path $resolvedSourceDir 'packaging\AppxManifest.xml') -Destination $stageDir
Copy-Item -Path (Join-Path $resolvedSourceDir 'packaging\Assets\*.png') -Destination (Join-Path $stageDir 'Assets')

$winDeployQt = Join-Path $resolvedQtBin 'windeployqt.exe'
if (-not (Test-Path -LiteralPath $winDeployQt)) {
    throw "windeployqt.exe was not found in $resolvedQtBin"
}
& $winDeployQt --release --qmldir (Join-Path $resolvedSourceDir 'qml') --no-translations $stageDir
if ($LASTEXITCODE -ne 0) {
    throw "windeployqt failed with exit code $LASTEXITCODE"
}

$makeAppx = Find-WindowsSdkTool 'makeappx.exe'
$signTool = Find-WindowsSdkTool 'signtool.exe'
New-Item -ItemType Directory -Path $resolvedOutputDir -Force | Out-Null
& $makeAppx pack /o /d $stageDir /p $packagePath
if ($LASTEXITCODE -ne 0) {
    throw "MakeAppx failed with exit code $LASTEXITCODE"
}

Import-Module (Join-Path $PSHOME 'Modules\Microsoft.PowerShell.Security\Microsoft.PowerShell.Security.psd1') -ErrorAction Stop
if (-not (Get-PSDrive -Name Cert -ErrorAction SilentlyContinue)) {
    throw 'The Windows certificate provider is unavailable in this PowerShell process.'
}

$subject = 'CN=WindowsSidebarDev'
$certificate = Get-ChildItem Cert:\CurrentUser\My |
    Where-Object { $_.Subject -eq $subject -and $_.HasPrivateKey -and $_.NotAfter -gt (Get-Date) } |
    Sort-Object NotAfter -Descending |
    Select-Object -First 1

if (-not $certificate) {
    $certificate = New-SelfSignedCertificate `
        -Type Custom `
        -Subject $subject `
        -KeyUsage DigitalSignature `
        -FriendlyName 'Windows Sidebar development signing certificate' `
        -CertStoreLocation 'Cert:\CurrentUser\My' `
        -TextExtension @('2.5.29.37={text}1.3.6.1.5.5.7.3.3') `
        -NotAfter (Get-Date).AddYears(2)
}

$certificateFile = Join-Path $resolvedOutputDir 'WindowsSidebarDev.cer'
Export-Certificate -Cert $certificate -FilePath $certificateFile -Force | Out-Null

& $signTool sign /fd SHA256 /sha1 $certificate.Thumbprint $packagePath
if ($LASTEXITCODE -ne 0) {
    throw "SignTool failed with exit code $LASTEXITCODE"
}

Write-Host "Created and signed package: $packagePath"
Write-Host "Development certificate: $certificateFile"
Write-Host "Install with: Add-AppxPackage -ForceApplicationShutdown '$packagePath'"
Write-Host "Launch Windows Sidebar from the Start menu after installation."
