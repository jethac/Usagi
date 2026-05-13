param(
    [string]$RepoRoot = (Resolve-Path "$PSScriptRoot\..\..\..").Path
)

$ErrorActionPreference = 'Stop'

$ResolvedRepoRoot = [System.IO.Path]::GetFullPath($RepoRoot)
$RepoRoot = $ResolvedRepoRoot
$RepoParent = Split-Path -Parent $RepoRoot
$ToolsRoot = Join-Path $RepoParent 'tools'
if (-not (Test-Path (Join-Path $ToolsRoot 'usagi-dev-env.ps1'))) {
    $ToolsRoot = Join-Path (Split-Path -Parent $RepoParent) 'tools'
}
$ToolsRoot = [System.IO.Path]::GetFullPath($ToolsRoot)
. (Join-Path $ToolsRoot 'usagi-dev-env.ps1')

$RepoRoot = $ResolvedRepoRoot
$env:USAGI_DIR = $RepoRoot
$BuildRoot = Join-Path $ToolsRoot 'test-build\AudioToolBuildRules'
$ProjectRoot = Join-Path $BuildRoot 'Project'
$AudioDir = Join-Path $ProjectRoot 'Data\VPB\Audio'
$BuildConfig = Join-Path $RepoRoot 'Tools\build\build_config.rb'

Remove-Item -LiteralPath $BuildRoot -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path $AudioDir | Out-Null

$InputYaml = Join-Path $AudioDir 'TestAudio.yml'
@'
AudioBank:
  soundFiles:
    - enumName: TEST_SOUND
      filename: test_sound
      stream: false
      loop: false
      volume: 1.0
      minDistance: 1.0
      maxDistance: 1000.0
      eType: AUDIO_TYPE_SFX
      eFalloff: AUDIO_FALLOFF_LINEAR
      pitchRandomisation: 0.0
      dopplerFactor: 0.0
      basePitch: 1.0
      priority: 128
      crossfade: ""
      localized: false
      crc: 0
      filterCRC: 0
      effectCRCs: []
      roomNameCRC: 0
  filters: []
  reverbs: []
  rooms: []
'@ | Set-Content -Encoding ASCII -LiteralPath $InputYaml

function Get-AudioToolCommand {
    param([switch]$Legacy)

    $previousLegacy = $env:USAGI_USE_LEGACY_AUDIO_TOOL
    try {
        if ($Legacy) {
            $env:USAGI_USE_LEGACY_AUDIO_TOOL = '1'
        }
        else {
            Remove-Item Env:USAGI_USE_LEGACY_AUDIO_TOOL -ErrorAction SilentlyContinue
        }

        Push-Location $ProjectRoot
        try {
            $script = @"
require ARGV[0]
config = BuildConfig.new(ENV.fetch('USAGI_DIR'), ['debug', 'win', 'JP', 'TestProject', Dir.pwd])
puts config.vitei_audio_tool
"@
            $command = ruby -e $script $BuildConfig
            if ($LASTEXITCODE -ne 0) {
                throw "BuildConfig audio tool command generation failed."
            }

            return $command.Trim()
        }
        finally {
            Pop-Location
        }
    }
    finally {
        if ($null -eq $previousLegacy) {
            Remove-Item Env:USAGI_USE_LEGACY_AUDIO_TOOL -ErrorAction SilentlyContinue
        }
        else {
            $env:USAGI_USE_LEGACY_AUDIO_TOOL = $previousLegacy
        }
    }
}

$AudioToolCommand = Get-AudioToolCommand
if ($AudioToolCommand -notmatch 'Usagi\.AudioToolCli') {
    throw "Default audio build command did not select the managed audio CLI: $AudioToolCommand"
}

$LegacyAudioToolCommand = Get-AudioToolCommand -Legacy
if ($LegacyAudioToolCommand -notmatch 'FSIDBuilder\.exe') {
    throw "Legacy audio build command did not select FSIDBuilder.exe: $LegacyAudioToolCommand"
}

$OutputProto = Join-Path $ProjectRoot 'TestProject\audio_gen\TestAudio.proto'
$BuildRuleCommand = $AudioToolCommand +
    ' --proto' +
    ' -i="' + $InputYaml + '"' +
    ' -o="' + $OutputProto + '"' +
    ' -e=TestAudio' +
    ' -g=_CLR_TEST_AUDIO_FSID_'

Push-Location $ProjectRoot
try {
    cmd /c $BuildRuleCommand
    if ($LASTEXITCODE -ne 0) {
        throw "Audio build rule command failed with exit code $LASTEXITCODE"
    }
}
finally {
    Pop-Location
}

if ((Get-Content -Raw -LiteralPath $OutputProto) -notmatch 'TEST_SOUND\s*=') {
    throw 'Generated audio build-rule proto did not contain TEST_SOUND.'
}

Write-Host "Audio build rule passed: $OutputProto"
