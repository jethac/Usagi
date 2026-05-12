$ErrorActionPreference = 'Stop'

$TestRoot = Split-Path -Parent $MyInvocation.MyCommand.Path

$Tests = @(
    'SystemScheduler',
    'ComponentMutation'
)

foreach ($Test in $Tests) {
    $Script = Join-Path $TestRoot "$Test\Run.ps1"
    if (-not (Test-Path $Script)) {
        throw "Missing test script: $Script"
    }

    Write-Host "== $Test =="
    powershell -ExecutionPolicy Bypass -File $Script
}

Write-Host 'ECS threading tests passed'
