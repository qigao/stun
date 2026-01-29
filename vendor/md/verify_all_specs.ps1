$ErrorActionPreference = "Continue"
$binDir = "C:\projects\cpp\nanogui\build\Ninja\Msvc\bin"
$specTests = Get-ChildItem -Path $binDir -Filter "spec_test_*.exe"

Write-Host "Running all discovered spec tests..." -ForegroundColor Cyan
Write-Host "========================================"

$totalPassed = 0
$totalFailed = 0
$results = @()

foreach ($test in $specTests) {
    Write-Host "Running $($test.Name)..." -ForegroundColor Yellow
    $process = Start-Process -FilePath $test.FullName -Wait -NoNewWindow -PassThru -RedirectStandardOutput "tmp_out.txt"
    $output = Get-Content "tmp_out.txt"
    Remove-Item "tmp_out.txt"
    
    $summaryLine = $output | Where-Object { $_ -match "Results: (\d+) passed, (\d+) failed" } | Select-Object -Last 1
    
    if ($summaryLine -match "Results: (\d+) passed, (\d+) failed") {
        $passed = [int]$matches[1]
        $failed = [int]$matches[2]
        $totalPassed += $passed
        $totalFailed += $failed
        
        $status = if ($failed -eq 0) { "PASSED" } else { "FAILED" }
        $color = if ($failed -eq 0) { "Green" } else { "Red" }
        Write-Host "  $status ($passed passed, $failed failed)" -ForegroundColor $color
    }
    else {
        Write-Host "  Error: Could not parse results for $($test.Name)" -ForegroundColor Red
        # Show last few lines of output for debugging
        $output | Select-Object -Last 3 | ForEach-Object { Write-Host "    > $_" -ForegroundColor Gray }
    }
}

Write-Host "========================================"
Write-Host "SUMMARY:" -ForegroundColor Cyan
Write-Host "Total Passed: $totalPassed" -ForegroundColor Green
Write-Host "Total Failed: $totalFailed" -ForegroundColor $color
$successRate = if ($totalPassed + $totalFailed -gt 0) { [math]::Round((100.0 * $totalPassed / ($totalPassed + $totalFailed)), 2) } else { 0 }
Write-Host "Overall Success Rate: $successRate%" -ForegroundColor Cyan
