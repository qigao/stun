param(
  [string[]]$Name = @("button", "input", "select"),
  [string]$RegistryBase = "https://ui.shadcn.com/r",
  [string]$OutputDir = "flexUI/tests/shadcn/registry-cache"
)

if ($PSVersionTable.PSVersion.Major -lt 7) {
  $pwsh = Get-Command pwsh -ErrorAction SilentlyContinue
  if ($pwsh) {
    & $pwsh.Source -ExecutionPolicy Bypass -File $PSCommandPath `
      -Name $Name `
      -RegistryBase $RegistryBase `
      -OutputDir $OutputDir
    exit $LASTEXITCODE
  }
}

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

$root = Resolve-Path (Join-Path $PSScriptRoot "..\\..")
$out = Join-Path $root $OutputDir
New-Item -ItemType Directory -Force -Path $out | Out-Null

$names = @()
foreach ($entry in $Name) {
  foreach ($part in ($entry -split ',')) {
    if (-not [string]::IsNullOrWhiteSpace($part)) {
      $names += $part.Trim()
    }
  }
}

function Get-ClassTokens([string]$content) {
  $tokens = [System.Collections.Generic.HashSet[string]]::new()
  if ([string]::IsNullOrWhiteSpace($content)) {
    return @()
  }

  $trimChars = [char[]]@('"', "'", ',', ';', '{', '}')
  $literalPattern = '"([^"\\]*(?:\\.[^"\\]*)*)"|''([^''\\]*(?:\\.[^''\\]*)*)'''
  foreach ($match in [regex]::Matches($content, $literalPattern)) {
    $literal = $null
    for ($i = 1; $i -lt $match.Groups.Count; ++$i) {
      if ($match.Groups[$i].Success) {
        $literal = $match.Groups[$i].Value
        break
      }
    }
    if ($null -eq $literal) {
      continue
    }

    foreach ($rawToken in ($literal -split '\s+')) {
      $token = $rawToken.Trim().Trim($trimChars)
      if ([string]::IsNullOrWhiteSpace($token)) {
        continue
      }
      if ($token -notmatch '^[A-Za-z0-9_\-\[\]\/\\:&.=><()%+]+$') {
        continue
      }
      if ($token -notmatch '[A-Za-z]') {
        continue
      }
      if ($token -match '^(import|from|as|use|client|type|position|size:|VariantProps|SelectPrimitive|Slot|Check|ChevronDown|ChevronUp)$') {
        continue
      }
      if ($token -match '^<.*>$') {
        continue
      }
      if ($token -match '^(react|lucide-react|class-variance-authority|react-hook-form|react-day-picker|react-resizable-panels|recharts|next-themes|sonner|vaul)$') {
        continue
      }
      if ($token -match '^(default|button|input|textarea|popper|id|alert|center|horizontal|vertical|true|false|left|right|top|bottom|start|end)$') {
        continue
      }
      if ($token -match '^(menubar-sub|input-otp|separator|cmdk|calendar|pagination|navigation|previous|next|page|ghost|icon|outline|size|variant|short|label)$') {
        continue
      }
      if ($token -match '^(a|div|li|nav|span|ul|be|should|used|useFormField|useChart|within|must|children|color|const|dashed|dot|line|none|null|object|payload|return|string|theme|system|toast|toaster|to|value|verticalAlign)$') {
        continue
      }
      if ($token -match '^(\.dark|--color-bg|--color-border|\\n|<ChartContainer)$') {
        continue
      }
      if ($token.StartsWith("(")) {
        continue
      }
      if ($token -cmatch '^[A-Z_][A-Z0-9_]*$') {
        continue
      }
      if ($token -cmatch '^[A-Z][A-Za-z0-9]*$') {
        continue
      }
      if ($token -cmatch '^[A-Z][A-Za-z0-9]*\[$') {
        continue
      }
      if ($token -cmatch '^[a-z][A-Za-z0-9]*Variants$') {
        continue
      }
      if ($token.EndsWith(":") -and $token -notlike "data-*") {
        continue
      }
      [void]$tokens.Add($token)
    }
  }

  return @($tokens) | Sort-Object
}

foreach ($component in $names) {
  $headers = @{ "User-Agent" = "codex" }
  $candidates = @(
    "$RegistryBase/$component.json",
    "https://ui.shadcn.com/r/styles/new-york/$component.json",
    "https://ui.shadcn.com/r/styles/new-york-v4/$component.json",
    "https://api.github.com/repos/shadcn-ui/ui/contents/apps/v4/registry/new-york-v4/ui/$component.tsx?ref=main"
  )

  $response = $null
  $resolvedUri = $null
  foreach ($candidate in $candidates) {
    try {
      Write-Host "fetch $candidate"
      $response = Invoke-WebRequest -Headers $headers -Uri $candidate
      $resolvedUri = $candidate
      break
    } catch {
      continue
    }
  }

  if ($null -eq $response) {
    throw "unable to fetch shadcn source for $component"
  }

  $isJson = $resolvedUri.EndsWith(".json")
  $isGithubContentsApi = $resolvedUri -like "https://api.github.com/repos/*"
  $tokens = [System.Collections.Generic.HashSet[string]]::new()

  if ($isGithubContentsApi) {
    $payload = $response.Content | ConvertFrom-Json -Depth 64
    $sourceText = [System.Text.Encoding]::UTF8.GetString(
      [System.Convert]::FromBase64String(($payload.content -replace '\s', ''))
    )
    $target = Join-Path $out "$component.source.tsx"
    $sourceText | Set-Content -Path $target -Encoding UTF8
    foreach ($token in (Get-ClassTokens -content $sourceText)) {
      [void]$tokens.Add($token)
    }
  } elseif ($isJson) {
    $target = Join-Path $out "$component.registry.json"
    $response.Content | Set-Content -Path $target -Encoding UTF8
    $payload = $response.Content | ConvertFrom-Json -Depth 64
    if ($payload.PSObject.Properties.Name -contains "files") {
      foreach ($file in $payload.files) {
        if ($file.PSObject.Properties.Name -contains "content") {
          foreach ($token in (Get-ClassTokens -content $file.content)) {
            [void]$tokens.Add($token)
          }
        }
      }
    }
  } else {
    $target = Join-Path $out "$component.source.tsx"
    $response.Content | Set-Content -Path $target -Encoding UTF8
    foreach ($token in (Get-ClassTokens -content $response.Content)) {
      [void]$tokens.Add($token)
    }
  }

  $summary = [ordered]@{
    name = $component
    source = $resolvedUri
    format = if ($isJson) { "registry-item-json" } else { "raw-tsx" }
    extracted_class_tokens = @($tokens) | Sort-Object
  }

  $summaryPath = Join-Path $out "$component.summary.json"
  $summary | ConvertTo-Json -Depth 16 | Set-Content -Path $summaryPath -Encoding UTF8
}
