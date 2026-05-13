param(
    [switch]$IncludeRenderSmoke,
    [switch]$RequireValidation,
    [string]$OutputDir
)

$ErrorActionPreference = 'Stop'

$TestDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$UsagiRoot = (Resolve-Path (Join-Path $TestDir '..\..\..')).Path
$ToolsRoot = Join-Path (Split-Path -Parent $UsagiRoot) 'tools'
$EnvScript = Join-Path $ToolsRoot 'usagi-dev-env.ps1'

. $EnvScript

$UsagiRoot = $env:USAGI_DIR

if (-not $OutputDir) {
    $OutputDir = Join-Path $ToolsRoot 'test-build\PerformanceBaseline'
}

$LogDir = Join-Path $OutputDir 'logs'
New-Item -ItemType Directory -Force -Path $OutputDir, $LogDir | Out-Null

function Invoke-BaselineCommand {
    param(
        [string]$Name,
        [string]$Script,
        [string[]]$Arguments = @()
    )

    if (-not (Test-Path $Script)) {
        throw "Baseline command script not found: $Script"
    }

    $StdOutLog = Join-Path $LogDir "$Name.stdout.log"
    $StdErrLog = Join-Path $LogDir "$Name.stderr.log"
    Remove-Item -LiteralPath $StdOutLog, $StdErrLog -Force -ErrorAction SilentlyContinue

    $ArgumentList = @(
        '-NoProfile',
        '-ExecutionPolicy', 'Bypass',
        '-File', $Script
    ) + $Arguments

    $Timer = [System.Diagnostics.Stopwatch]::StartNew()
    $Process = Start-Process `
        -FilePath 'powershell.exe' `
        -ArgumentList $ArgumentList `
        -WorkingDirectory $UsagiRoot `
        -PassThru `
        -Wait `
        -WindowStyle Hidden `
        -RedirectStandardOutput $StdOutLog `
        -RedirectStandardError $StdErrLog
    $Timer.Stop()

    [PSCustomObject]@{
        name = $Name
        script = $Script
        arguments = $Arguments
        exitCode = $Process.ExitCode
        durationSeconds = [Math]::Round($Timer.Elapsed.TotalSeconds, 3)
        stdoutLog = $StdOutLog
        stderrLog = $StdErrLog
    }
}

$Commands = @(
    [PSCustomObject]@{
        Name = 'system-scheduler'
        Script = Join-Path $UsagiRoot 'Tools\Tests\SystemScheduler\Run.ps1'
        Arguments = @()
        Category = 'heavy ECS scheduling'
    },
    [PSCustomObject]@{
        Name = 'ecs-frame-threading'
        Script = Join-Path $UsagiRoot 'Tools\Tests\ECSFrameThreading\Run.ps1'
        Arguments = @()
        Category = 'heavy ECS frame traversal'
    },
    [PSCustomObject]@{
        Name = 'shader-package-rendering'
        Script = Join-Path $UsagiRoot 'Tools\Tests\ShaderPackageRendering\Run.ps1'
        Arguments = @()
        Category = 'many shader/material variants'
    }
)

if ($IncludeRenderSmoke) {
    $ParticleArgs = @('-Launch')
    if ($RequireValidation) {
        $ParticleArgs += '-RequireValidation'
    }

    $Commands += [PSCustomObject]@{
        Name = 'particle-editor-render-smoke'
        Script = Join-Path $UsagiRoot 'Tools\Tests\ParticleEditorSmoke\Run.ps1'
        Arguments = $ParticleArgs
        Category = 'particles, Vulkan startup, frame present'
    }
}

$StartedAt = [DateTimeOffset]::Now
$Results = @()

foreach ($Command in $Commands) {
    Write-Host "Running performance baseline: $($Command.Name)"
    $Result = Invoke-BaselineCommand -Name $Command.Name -Script $Command.Script -Arguments $Command.Arguments
    $Result | Add-Member -NotePropertyName category -NotePropertyValue $Command.Category
    $Results += $Result

    if ($Result.exitCode -ne 0) {
        $StdErr = if (Test-Path $Result.stderrLog) { (Get-Content $Result.stderrLog -Tail 40) -join [Environment]::NewLine } else { '' }
        throw "Performance baseline command failed: $($Command.Name), exit code $($Result.exitCode).`nSTDERR:`n$StdErr"
    }
}

$Report = [PSCustomObject]@{
    generatedAt = $StartedAt.ToString('o')
    usagiRoot = $UsagiRoot
    outputDir = $OutputDir
    machine = $env:COMPUTERNAME
    commands = $Results
}

$JsonPath = Join-Path $OutputDir 'baseline.json'
$MarkdownPath = Join-Path $OutputDir 'baseline.md'

$Report | ConvertTo-Json -Depth 5 | Set-Content -Path $JsonPath -Encoding ASCII

$Markdown = New-Object System.Collections.Generic.List[string]
$Markdown.Add('# Usagi Performance Baseline')
$Markdown.Add('')
$Markdown.Add("Generated: $($Report.generatedAt)")
$Markdown.Add('')
$Markdown.Add('| Command | Category | Duration (s) | Exit |')
$Markdown.Add('| --- | --- | ---: | ---: |')
foreach ($Result in $Results) {
    $Markdown.Add("| $($Result.name) | $($Result.category) | $($Result.durationSeconds) | $($Result.exitCode) |")
}
$Markdown.Add('')
$Markdown.Add("JSON: $JsonPath")
$Markdown.Add("Logs: $LogDir")
$Markdown | Set-Content -Path $MarkdownPath -Encoding ASCII

Write-Host "Performance baseline passed: $JsonPath"
