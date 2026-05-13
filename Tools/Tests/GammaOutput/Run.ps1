$ErrorActionPreference = 'Stop'

$TestDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$UsagiRoot = (Resolve-Path (Join-Path $TestDir '..\..\..')).Path
$ToolsRoot = Join-Path (Split-Path -Parent $UsagiRoot) 'tools'
$EnvScript = Join-Path $ToolsRoot 'usagi-dev-env.ps1'

. $EnvScript

$UsagiRoot = $env:USAGI_DIR

if (-not $env:MSBUILD_DIR) {
    throw 'MSBUILD_DIR is not set. Install Visual Studio Build Tools or update tools\usagi-dev-env.ps1.'
}

$BuildDir = Join-Path $ToolsRoot 'test-build\GammaOutput'
$PackageDir = Join-Path $BuildDir 'packages'
$TempDir = Join-Path $BuildDir 'tmp'
New-Item -ItemType Directory -Force -Path $BuildDir, $PackageDir, $TempDir | Out-Null

$ShaderPackage = Join-Path $UsagiRoot 'Tools\bin\ShaderPackage.exe'
$PostProcessEffect = Join-Path $UsagiRoot 'Data\GLSL\effects\PostProcess.yml'
$ShaderDir = Join-Path $UsagiRoot 'Data\GLSL\shaders'
$PostProcessPackage = Join-Path $PackageDir 'PostProcess.pak'

if (-not (Test-Path $ShaderPackage)) {
    throw "ShaderPackage.exe not found: $ShaderPackage"
}

& $ShaderPackage $PostProcessEffect -o $PostProcessPackage -t $TempDir -s $ShaderDir -a vulkan
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}
if (-not (Test-Path $PostProcessPackage)) {
    throw "Shader package was not produced: $PostProcessPackage"
}

$Source = Join-Path $TestDir 'GammaOutputTests.cpp'
$Exe = Join-Path $BuildDir 'GammaOutputTests.exe'
$Ramp = Join-Path $BuildDir 'display_encode_expected.ppm'
$VcVars = 'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat'

if (-not (Test-Path $VcVars)) {
    throw "Unable to find Visual Studio environment script: $VcVars"
}

$CommandParts = @(
    "`"$VcVars`" >nul &&",
    'cl /nologo /std:c++17 /EHsc /MTd /Zi',
    "/Fo$BuildDir\",
    "/Fe$Exe",
    $Source
)

$Command = $CommandParts -join ' '

cmd /c $Command
if ($LASTEXITCODE -ne 0) {
    throw "Gamma output test build failed with exit code $LASTEXITCODE"
}

& $Exe $UsagiRoot $Ramp
if ($LASTEXITCODE -ne 0) {
    throw "Gamma output test executable failed with exit code $LASTEXITCODE"
}
