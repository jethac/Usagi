$ErrorActionPreference = 'Stop'

$TestDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$UsagiRoot = (Resolve-Path (Join-Path $TestDir '..\..\..')).Path
$ToolsRoot = Join-Path (Split-Path -Parent $UsagiRoot) 'tools'
$EnvScript = Join-Path $ToolsRoot 'usagi-dev-env.ps1'

. $EnvScript

$UsagiRoot = $env:USAGI_DIR
$BuildDir = Join-Path $ToolsRoot 'test-build\ShaderPackageRendering'
$PackageDir = Join-Path $BuildDir 'packages'
$TempDir = Join-Path $BuildDir 'tmp'
$ShaderDir = Join-Path $UsagiRoot 'Data\GLSL\shaders'
$ShaderPackage = Join-Path $UsagiRoot 'Tools\bin\ShaderPackage.exe'

if (-not (Test-Path $ShaderPackage)) {
    throw "ShaderPackage.exe not found: $ShaderPackage"
}

New-Item -ItemType Directory -Force -Path $PackageDir, $TempDir | Out-Null

$Packages = @(
    @{ Name = 'Model'; Effect = 'Data\GLSL\effects\Model.yml' },
    @{ Name = 'PostProcess'; Effect = 'Data\GLSL\effects\PostProcess.yml' },
    @{ Name = 'Particles'; Effect = 'Data\GLSL\effects\Particles.yml' }
)

foreach ($Package in $Packages) {
    $EffectPath = Join-Path $UsagiRoot $Package.Effect
    $OutputPath = Join-Path $PackageDir "$($Package.Name).pak"

    & $ShaderPackage $EffectPath -o $OutputPath -t $TempDir -s $ShaderDir -a vulkan
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }

    if (-not (Test-Path $OutputPath)) {
        throw "Shader package was not produced: $OutputPath"
    }
}

Write-Host 'Rendering shader package smoke test passed'
