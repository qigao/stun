param(
  [string[]]$Name = @(),
  [string]$WhitelistPath = "tools/shadcn-ir/schema/utility_whitelist.json",
  [string]$RegistryDir = "flexUI/tests/shadcn/registry-cache",
  [switch]$Quiet
)

if ($PSVersionTable.PSVersion.Major -lt 7) {
  $pwsh = Get-Command pwsh -ErrorAction SilentlyContinue
  if ($pwsh) {
    & $pwsh.Source -ExecutionPolicy Bypass -File $PSCommandPath `
      -Name $Name `
      -WhitelistPath $WhitelistPath `
      -RegistryDir $RegistryDir `
      -Quiet:$Quiet
    exit $LASTEXITCODE
  }
}

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$root = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$whitelistFile = Join-Path $root $WhitelistPath
$registryPath = Join-Path $root $RegistryDir

if (-not (Test-Path -LiteralPath $whitelistFile)) {
  throw "utility whitelist not found: $whitelistFile"
}
if (-not (Test-Path -LiteralPath $registryPath)) {
  throw "registry cache directory not found: $registryPath"
}

$whitelist = Get-Content -Raw $whitelistFile | ConvertFrom-Json -Depth 64
if (-not ($whitelist.PSObject.Properties.Name -contains "tokens")) {
  throw "utility whitelist must contain tokens"
}

$knownTokens = [System.Collections.Generic.HashSet[string]]::new(
  [StringComparer]::Ordinal
)
foreach ($token in $whitelist.tokens.PSObject.Properties.Name) {
  [void]$knownTokens.Add($token)
}

function Split-ComponentNames([string[]]$values) {
  $result = @()
  foreach ($entry in $values) {
    foreach ($part in ($entry -split ",")) {
      if (-not [string]::IsNullOrWhiteSpace($part)) {
        $result += $part.Trim()
      }
    }
  }
  return $result
}

function Test-SourceIdentifierToken([string]$token) {
  if ([string]::IsNullOrWhiteSpace($token)) {
    return $false
  }
  return $token -cmatch "^[A-Z][A-Za-z0-9]*$" -or
         $token -cmatch "^[A-Z][A-Za-z0-9]*\[$" -or
         $token -cmatch "^[a-z][A-Za-z0-9]*Variants$"
}

$components = @(Split-ComponentNames $Name)
$summaryFiles = @()
if ($components.Count -gt 0) {
  foreach ($component in $components) {
    $summaryPath = Join-Path $registryPath "$component.summary.json"
    if (-not (Test-Path -LiteralPath $summaryPath)) {
      throw "registry summary not found for $component`: $summaryPath"
    }
    $summaryFiles += Get-Item -LiteralPath $summaryPath
  }
} else {
  $summaryFiles = @(Get-ChildItem -LiteralPath $registryPath -Filter "*.summary.json")
}

if ($summaryFiles.Count -eq 0) {
  throw "no registry summary files found in $registryPath"
}

$rows = @()
$failures = @()
foreach ($summaryFile in $summaryFiles) {
  $summary = Get-Content -Raw $summaryFile.FullName | ConvertFrom-Json -Depth 64
  if (-not ($summary.PSObject.Properties.Name -contains "extracted_class_tokens")) {
    throw "registry summary missing extracted_class_tokens: $($summaryFile.FullName)"
  }

  $tokens = @($summary.extracted_class_tokens | Where-Object {
    -not (Test-SourceIdentifierToken $_)
  })
  $missing = @($tokens | Where-Object { -not $knownTokens.Contains($_) })
  $component = if ($summary.PSObject.Properties.Name -contains "name") {
    $summary.name
  } else {
    $summaryFile.BaseName -replace "\.summary$", ""
  }

  $rows += [pscustomobject]@{
    name = $component
    total = $tokens.Count
    missing = $missing.Count
    missing_tokens = ($missing -join ", ")
  }

  if ($missing.Count -gt 0) {
    $failures += "$component`: $($missing -join ', ')"
  }
}

if (-not $Quiet) {
  $rows | Sort-Object missing,total,name | Format-Table -AutoSize
}

if ($failures.Count -gt 0) {
  Write-Error ("missing shadcn utility whitelist coverage:`n" +
               ($failures -join "`n"))
  exit 1
}

exit 0
