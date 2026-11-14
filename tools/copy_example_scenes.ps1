param(
    [string]$SourceRoot = 'F:\code\LightTransport\scenes',
    [string]$DestinationRoot = (Join-Path (Split-Path $PSScriptRoot -Parent) 'scenes')
)
$ErrorActionPreference = 'Stop'
$SourceRoot = [System.IO.Path]::GetFullPath($SourceRoot)
$DestinationRoot = [System.IO.Path]::GetFullPath($DestinationRoot)
foreach ($name in @('bathroom', 'spaceship')) {
    $source = Join-Path $SourceRoot $name
    $destination = Join-Path $DestinationRoot $name
    $files = @(
        Get-Item -LiteralPath (Join-Path $source 'scene-v4.pbrt')
        Get-Item -LiteralPath (Join-Path $source 'LICENSE.txt')
        foreach ($directory in @('models', 'textures')) {
            $path = Join-Path $source $directory
            if (Test-Path -LiteralPath $path) {
                Get-ChildItem -LiteralPath $path -Recurse -File | Where-Object { $_.Extension -ne '.lt' }
            }
        }
    )
    $entries = foreach ($file in $files) {
        $relative = [System.IO.Path]::GetRelativePath($source, $file.FullName)
        $target = Join-Path $destination $relative
        if ([System.IO.Path]::GetFullPath($target) -eq $file.FullName) { throw 'Source and destination must differ.' }
        $hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
        if (Test-Path -LiteralPath $target) {
            if ((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ne $hash) {
                throw "Preserving different existing file: $target"
            }
        } else {
            New-Item -ItemType Directory -Force -Path (Split-Path $target -Parent) | Out-Null
            Copy-Item -LiteralPath $file.FullName -Destination $target
        }
        if ((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ne $hash) { throw "Copy hash mismatch: $target" }
        [pscustomobject]@{ path = $relative.Replace('\', '/'); bytes = $file.Length; sha256 = $hash }
    }
    $ltFiles = @(Get-ChildItem -LiteralPath $destination -Recurse -File -Filter '*.lt')
    if ($ltFiles.Count) { throw "Destination contains excluded .lt files: $destination" }
    $manifest = [ordered]@{
        source = $source
        copied_utc = [DateTime]::UtcNow.ToString('o')
        exclusions = @('*.lt', 'TungstenRender.exr', 'TungstenRender.png')
        file_count = @($entries).Count
        total_bytes = ($entries | Measure-Object -Property bytes -Sum).Sum
        files = @($entries)
    }
    $manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $destination 'asset-manifest.json') -Encoding utf8
    [pscustomobject]@{ scene = $name; files = $manifest.file_count; bytes = $manifest.total_bytes; lt_files = $ltFiles.Count; destination = $destination }
}
