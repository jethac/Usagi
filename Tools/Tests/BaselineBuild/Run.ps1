$ErrorActionPreference = 'Stop'

$TestDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$UsagiRoot = (Resolve-Path (Join-Path $TestDir '..\..\..')).Path
$ToolsRoot = Join-Path (Split-Path -Parent $UsagiRoot) 'tools'
$EnvScript = Join-Path $ToolsRoot 'usagi-dev-env.ps1'

. $EnvScript

$UsagiRoot = $env:USAGI_DIR
$MSBuild = Join-Path $env:MSBUILD_DIR 'MSBuild.exe'

if (-not (Test-Path $MSBuild)) {
    throw "MSBuild.exe not found: $MSBuild"
}

Push-Location $UsagiRoot
try {
    & rake platform=win build=debug,release projects
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }

    $engineProjects = @(
        'AI',
        'Audio',
        'Common',
        'Core',
        'Debug',
        'Framework',
        'Game',
        'Graphics',
        'GUI',
        'HID',
        'Layout',
        'Maths',
        'Memory',
        'Network',
        'Particles',
        'Physics',
        'PostFX',
        'Resource',
        'Scene',
        'System',
        'ThirdParty'
    )

    $failed = @()

    foreach ($config in @('Debug', 'Release')) {
        foreach ($project in $engineProjects) {
            $projectFile = Join-Path $UsagiRoot "_build\projects\Engine\$project\$project.vcxproj"
            Write-Host "=== $config x64 Engine/$project"

            & $MSBuild $projectFile /p:Configuration=$config /p:Platform=x64 /m /nologo
            if ($LASTEXITCODE -ne 0) {
                $failed += "$config|Engine/$project|$LASTEXITCODE"
            }
        }
    }

    if ($failed.Count -gt 0) {
        Write-Host 'FAILED_PROJECTS'
        $failed | ForEach-Object { Write-Host $_ }
        exit 1
    }

    Write-Host 'Baseline engine build passed'
}
finally {
    Pop-Location
}
