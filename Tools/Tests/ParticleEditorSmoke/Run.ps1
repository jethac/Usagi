param(
    [switch]$Launch,
    [switch]$RequireValidation,
    [int]$SmokeSeconds = 2
)

$ErrorActionPreference = 'Stop'

$TestDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$UsagiRoot = (Resolve-Path (Join-Path $TestDir '..\..\..')).Path
$ToolsRoot = Join-Path (Split-Path -Parent $UsagiRoot) 'tools'
$EnvScript = Join-Path $ToolsRoot 'usagi-dev-env.ps1'

. $EnvScript

$UsagiRoot = $env:USAGI_DIR
$ValidationSdk = Get-ChildItem $ToolsRoot -Directory -Filter 'VulkanSDK-*' -ErrorAction SilentlyContinue |
    Sort-Object Name -Descending |
    Where-Object { Test-Path (Join-Path $_.FullName 'Bin\VkLayer_khronos_validation.json') } |
    Select-Object -First 1

if ($ValidationSdk) {
    $env:VK_LAYER_PATH = Join-Path $ValidationSdk.FullName 'Bin'
    $env:PATH = "$env:VK_LAYER_PATH;$env:PATH"
}
elseif ($RequireValidation) {
    throw "VK_LAYER_KHRONOS_validation is not staged under $ToolsRoot."
}

$RuntimeRoot = Join-Path (Split-Path -Parent $UsagiRoot) '_romfiles\win'
$EffectsRoot = Join-Path $RuntimeRoot 'Effects'
$TexturesRoot = Join-Path $RuntimeRoot 'Textures'
$ParticleRoot = Join-Path $RuntimeRoot 'Particle'
$BuildRoot = Join-Path $ToolsRoot 'test-build\ParticleEditorSmoke'
$ShaderTemp = Join-Path $BuildRoot 'shader-temp'
$ShaderDir = Join-Path $UsagiRoot 'Data\GLSL\shaders'
$ShaderPackage = Join-Path $UsagiRoot 'Tools\bin\ShaderPackage.exe'
$ParticleEditorProject = Join-Path $UsagiRoot 'Tools\Source\ParticleEditor\project\ParticleEditor.vcxproj'
$ParticleEditorExe = Join-Path $UsagiRoot 'Tools\bin\ParticleEditor.exe'
$RubyPbDir = Join-Path $UsagiRoot '_build\ruby'
$MSBuild = Join-Path $env:MSBUILD_DIR 'MSBuild.exe'

if (-not $env:MSBUILD_DIR) {
    throw 'MSBUILD_DIR is not set. Install Visual Studio Build Tools or update tools\usagi-dev-env.ps1.'
}

if (-not (Test-Path $ShaderPackage)) {
    throw "ShaderPackage.exe not found: $ShaderPackage"
}

if (-not (Test-Path $RubyPbDir)) {
    Push-Location $UsagiRoot
    try {
        rake platform=win build=debug,release projects
    }
    finally {
        Pop-Location
    }
}

New-Item -ItemType Directory -Force -Path $EffectsRoot, $TexturesRoot, $ParticleRoot, $ShaderTemp | Out-Null

Push-Location $UsagiRoot
try {
    foreach ($Effect in Get-ChildItem (Join-Path $UsagiRoot 'Data\GLSL\effects') -Filter '*.yml') {
        $OutputPath = Join-Path $EffectsRoot "$($Effect.BaseName).pak"
        & $ShaderPackage $Effect.FullName -o $OutputPath -t $ShaderTemp -s $ShaderDir -a vulkan
        if ($LASTEXITCODE -ne 0) {
            exit $LASTEXITCODE
        }
    }

    foreach ($Texture in Get-ChildItem (Join-Path $UsagiRoot 'Data\Textures') -Recurse -File) {
        $Relative = $Texture.FullName.Substring((Join-Path $UsagiRoot 'Data\Textures').Length).TrimStart('\')
        $OutputPath = Join-Path $TexturesRoot $Relative
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $OutputPath) | Out-Null
        Copy-Item -LiteralPath $Texture.FullName -Destination $OutputPath -Force
    }

    $ParticleSources = @(
        @{ Source = 'Data/Particle/Emitters/multi_texture_slots.yml'; Output = (Join-Path $ParticleRoot 'multi_texture_slots.pem') },
        @{ Source = 'Data/Particle/Effects/multi_texture_slots.yml'; Output = (Join-Path $ParticleRoot 'multi_texture_slots.pfx') }
    )

    foreach ($Particle in $ParticleSources) {
        ruby ./Tools/ruby/yml2vpb.rb -R _build/ruby -o $Particle.Output $Particle.Source
        if ($LASTEXITCODE -ne 0) {
            exit $LASTEXITCODE
        }
    }

    $DataList = Join-Path $RuntimeRoot 'data_list.txt'
    $RuntimeRootResolved = (Resolve-Path $RuntimeRoot).Path
    Get-ChildItem $RuntimeRoot -Recurse -File |
        Where-Object { $_.Name -notin @('nameDataHash.bin', 'data_list.txt') -and $_.Extension -ne '.d' } |
        ForEach-Object { $_.FullName.Substring($RuntimeRootResolved.Length + 1).Replace('\', '/') } |
        Set-Content -Path $DataList -Encoding ASCII

    ruby ./Tools/ruby/nameDataHashListGen.rb -R _build/ruby -o (Join-Path $RuntimeRoot 'nameDataHash.bin') -d $RuntimeRoot $DataList
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}
finally {
    Pop-Location
}

$SolutionDir = "$UsagiRoot\"
& $MSBuild $ParticleEditorProject /p:Configuration=Debug /p:Platform=x64 "/p:SolutionDir=$SolutionDir" /m /nologo
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

if (-not (Test-Path $ParticleEditorExe)) {
    throw "ParticleEditor.exe not found: $ParticleEditorExe"
}

if ($Launch) {
    $StdOutLog = Join-Path $BuildRoot 'ParticleEditor.stdout.log'
    $StdErrLog = Join-Path $BuildRoot 'ParticleEditor.stderr.log'
    Remove-Item -LiteralPath $StdOutLog, $StdErrLog -Force -ErrorAction SilentlyContinue

    $Process = Start-Process `
        -FilePath $ParticleEditorExe `
        -WorkingDirectory $RuntimeRoot `
        -PassThru `
        -WindowStyle Hidden `
        -RedirectStandardOutput $StdOutLog `
        -RedirectStandardError $StdErrLog
    Start-Sleep -Seconds $SmokeSeconds

    if ($Process.HasExited) {
        $StdErr = if (Test-Path $StdErrLog) { (Get-Content $StdErrLog -Tail 20) -join [Environment]::NewLine } else { '' }
        $StdOut = if (Test-Path $StdOutLog) { (Get-Content $StdOutLog -Tail 20) -join [Environment]::NewLine } else { '' }
        throw "ParticleEditor exited during smoke window with code $($Process.ExitCode).`nSTDERR:`n$StdErr`nSTDOUT:`n$StdOut"
    }

    Stop-Process -Id $Process.Id -Force
    $Process.WaitForExit()
}

Write-Host "Particle Editor smoke preflight passed: $RuntimeRoot"
if ($ValidationSdk) {
    Write-Host "Vulkan validation layer path: $env:VK_LAYER_PATH"
}
