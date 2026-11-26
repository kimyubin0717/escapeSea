$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot
$sourceContent = 'C:\Users\USER\Downloads\twon\twon\Content'
$moves = Get-Content -LiteralPath (Join-Path $projectRoot 'Saved\AbyssalAssetMoves.json') -Raw | ConvertFrom-Json
$mapping = @{}
foreach ($move in $moves) { $mapping[($move.old -split '\.')[0]] = $move.new }
$failures = [System.Collections.Generic.List[object]]::new()
$checked = 0
foreach ($oldPackage in $mapping.Keys) {
    $sourceFile = Join-Path $sourceContent ($oldPackage.Substring(6).Replace('/','\') + '.uasset')
    if (!(Test-Path -LiteralPath $sourceFile)) { continue }
    $destination = Join-Path (Join-Path $projectRoot 'Content') ($mapping[$oldPackage].Substring(6).Replace('/','\') + '.uasset')
    if (!(Test-Path -LiteralPath $destination)) { $failures.Add(@{asset=$mapping[$oldPackage];missing='asset file'}); continue }
    $originalData = [Text.Encoding]::UTF8.GetString([IO.File]::ReadAllBytes($sourceFile))
    $currentData = [Text.Encoding]::UTF8.GetString([IO.File]::ReadAllBytes($destination))
    foreach ($reference in [regex]::Matches($originalData,'/Game/[A-Za-z0-9_/]+').Value | Select-Object -Unique) {
        if ($reference -eq $oldPackage -or !$mapping.ContainsKey($reference)) { continue }
        $expected = $mapping[$reference]
        if (!$currentData.Contains($expected)) { $failures.Add(@{asset=$mapping[$oldPackage];missing=$expected}) }
    }
    $checked++
}
$report = @{assets_checked=$checked; missing_reference_count=$failures.Count; failures=@($failures.ToArray())}
$report | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $projectRoot 'Saved\AbyssalReferenceAudit.json') -Encoding UTF8
$report | Select-Object assets_checked,missing_reference_count | Format-List
if ($failures.Count -gt 0) { $failures | Select-Object -First 12 | ConvertTo-Json; exit 1 }
