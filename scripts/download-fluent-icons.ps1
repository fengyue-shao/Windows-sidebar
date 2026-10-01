param([ValidatePattern('^\d+\.\d+\.\d+$')][string]$Version = '1.1.343')
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$cache = Join-Path (Split-Path $repo -Parent) "work/vendor/fluent-svg-icons-$Version"
if (Test-Path -LiteralPath $cache) { throw "Cache already exists: $cache. Preserve it; use another version for an update." }
$meta = Invoke-RestMethod "https://registry.npmjs.org/@fluentui%2fsvg-icons/$Version"
if ($meta.name -ne '@fluentui/svg-icons' -or $meta.license -ne 'MIT' -or $meta.repository.url -ne 'git+https://github.com/microsoft/fluentui-system-icons.git') { throw 'Unexpected package identity' }
if ($meta.dist.tarball -notlike 'https://registry.npmjs.org/@fluentui/svg-icons/-/*') { throw 'Unexpected download host' }
New-Item -ItemType Directory -Path $cache | Out-Null
$archive = Join-Path $cache 'icons.tgz'
Invoke-WebRequest -Uri $meta.dist.tarball -OutFile $archive
$sha = [System.Security.Cryptography.SHA512]::Create()
$stream = [System.IO.File]::OpenRead($archive)
try { $actual = 'sha512-' + [Convert]::ToBase64String($sha.ComputeHash($stream)) } finally { $stream.Dispose(); $sha.Dispose() }
if ($actual -ne $meta.dist.integrity) { throw 'Package integrity mismatch; not extracting.' }
$entries = & tar.exe -tzf $archive
if ($LASTEXITCODE -ne 0 -or @($entries | Where-Object { $_ -notmatch '^package/' -or $_ -match '(^|/)\.\.(/|$)' }).Count -gt 0) { throw 'Unsafe archive paths' }
& tar.exe -xzf $archive -C $cache
if ($LASTEXITCODE -ne 0) { throw 'Extraction failed' }
$selected = @('desktop','alert','timer','clipboard','toolbox','settings','window_console','play','pause','arrow_clockwise','delete','pin','checkmark','chevron_up','chevron_down','eye_off')
$destination = Join-Path $repo 'assets/fluent'
New-Item -ItemType Directory -Path $destination -Force | Out-Null
foreach ($name in $selected) {
    $file = "${name}_20_regular.svg"
    $source = Join-Path $cache "package/icons/$file"
    if (!(Test-Path -LiteralPath $source)) { throw "Missing selected icon: $file" }
    if (Test-Path -LiteralPath (Join-Path $destination $file)) { throw "Do not overwrite existing icon: $file" }
    Copy-Item -LiteralPath $source -Destination $destination
}
Invoke-WebRequest -Uri "https://raw.githubusercontent.com/microsoft/fluentui-system-icons/$Version/LICENSE" -OutFile (Join-Path $destination 'LICENSE')
[pscustomobject]@{ Version=$Version; Integrity=$actual; Cache=$cache; SvgCount=@(Get-ChildItem -LiteralPath (Join-Path $cache 'package/icons') -Filter '*.svg').Count; Selected=$selected.Count } | ConvertTo-Json
