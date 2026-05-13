param(
    [switch]$IncludeRenderSmoke,
    [switch]$RequireValidation
)

$ErrorActionPreference = 'Stop'

$TestDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$UsagiRoot = (Resolve-Path (Join-Path $TestDir '..\..\..')).Path
$ToolsRoot = Join-Path (Split-Path -Parent $UsagiRoot) 'tools'
$EnvScript = Join-Path $ToolsRoot 'usagi-dev-env.ps1'

. $EnvScript

$UsagiRoot = $env:USAGI_DIR

function Invoke-ReadinessStep {
    param(
        [string]$Name,
        [string]$Script,
        [string[]]$Arguments = @()
    )

    Write-Host "Running release readiness step: $Name"
    $Args = @('-ExecutionPolicy', 'Bypass', '-File', $Script) + $Arguments
    & powershell @Args
    if ($LASTEXITCODE -ne 0) {
        throw "Release readiness step failed: $Name"
    }
}

$Steps = @(
    @{ Name = 'resource-pak-exporter'; Script = 'Tools\Tests\ResourcePakExporter\Run.ps1'; Arguments = @() },
    @{ Name = 'level-conversion'; Script = 'Tools\Tests\LevelConversion\Run.ps1'; Arguments = @() },
    @{ Name = 'model-instancing'; Script = 'Tools\Tests\ModelInstancing\Run.ps1'; Arguments = @() },
    @{ Name = 'shader-package-rendering'; Script = 'Tools\Tests\ShaderPackageRendering\Run.ps1'; Arguments = @() },
    @{ Name = 'ecs-threading'; Script = 'Tools\Tests\RunEcsThreading.ps1'; Arguments = @() },
    @{ Name = 'performance-baseline'; Script = 'Tools\Tests\PerformanceBaseline\Run.ps1'; Arguments = @() }
)

if ($IncludeRenderSmoke) {
    $ParticleArgs = @('-Launch')
    if ($RequireValidation) {
        $ParticleArgs += '-RequireValidation'
    }
    $Steps += @{ Name = 'particle-editor-render-smoke'; Script = 'Tools\Tests\ParticleEditorSmoke\Run.ps1'; Arguments = $ParticleArgs }
}

Push-Location $UsagiRoot
try {
    foreach ($Step in $Steps) {
        Invoke-ReadinessStep `
            -Name $Step.Name `
            -Script (Join-Path $UsagiRoot $Step.Script) `
            -Arguments $Step.Arguments
    }
}
finally {
    Pop-Location
}

Write-Host 'Release readiness smoke passed'
