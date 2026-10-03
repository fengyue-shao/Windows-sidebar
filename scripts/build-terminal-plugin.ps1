param(
    [Parameter(Mandatory=$true)][string]$Executable,
    [Parameter(Mandatory=$true)][string]$QtBin,
    [Parameter(Mandatory=$true)][string]$OutputDir,
    [string]$MsvcRuntimeDir=''
)
$ErrorActionPreference='Stop'
$pluginSource=Join-Path (Split-Path $PSScriptRoot) 'plugins/com.windows-sidebar.terminal'
$resolvedExecutable=(Resolve-Path -LiteralPath $Executable).Path
$runner=Join-Path (Split-Path $resolvedExecutable) 'sidebar-terminal-runner.exe'
if (!(Test-Path -LiteralPath $runner -PathType Leaf)) { throw 'Runner missing' }
$destination=[IO.Path]::GetFullPath($OutputDir)
if (Test-Path -LiteralPath $destination) { throw 'Output already exists; select a fresh directory (no overwrite/deletion)' }
$bin=Join-Path $destination 'bin'
New-Item -ItemType Directory -Path $bin -Force | Out-Null
Copy-Item -LiteralPath $resolvedExecutable,$runner -Destination $bin
$deploy=Join-Path $QtBin 'windeployqt.exe'
& $deploy --release --qmldir (Join-Path $pluginSource 'ui') --no-translations --no-system-d3d-compiler --no-opengl-sw --compiler-runtime (Join-Path $bin 'sidebar-terminal-plugin.exe')
if ($LASTEXITCODE) { throw 'Qt deployment failed; partial artifact retained for diagnostics' }
if ($MsvcRuntimeDir) { Copy-Item -LiteralPath @((Get-ChildItem -LiteralPath $MsvcRuntimeDir -Filter '*.dll' -File).FullName) -Destination $bin }
foreach ($name in @('msvcp140.dll','vcruntime140.dll','vcruntime140_1.dll')) {
    if (!(Test-Path -LiteralPath (Join-Path $bin $name))) { throw ('Missing MSVC runtime: '+$name+'. Supply -MsvcRuntimeDir with the x64 VC redistributable CRT folder.') }
}
$manifest=Get-Content -LiteralPath (Join-Path $pluginSource 'plugin.json') -Raw | ConvertFrom-Json
$hashes=[ordered]@{}
foreach ($file in Get-ChildItem -LiteralPath $destination -File -Recurse) {
    $relative=$file.FullName.Substring($destination.Length+1).Replace('\','/')
    $hashes[$relative]=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
}
$manifest.files=$hashes
$json=$manifest | ConvertTo-Json -Depth 10
[IO.File]::WriteAllText((Join-Path $destination 'plugin.json'),$json,(New-Object Text.UTF8Encoding($false)))
Write-Output ('Independent terminal package: '+$destination)
Write-Output 'Import this folder from Toolbox after confirming local trust; not bundled in the host MSIX.'
