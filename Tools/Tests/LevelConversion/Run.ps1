param()

$ErrorActionPreference = 'Stop'

$ToolsRoot = Join-Path (Split-Path -Parent $PSScriptRoot) '..\..\..\tools'
$ToolsRoot = [System.IO.Path]::GetFullPath($ToolsRoot)
$EnvScript = Join-Path $ToolsRoot 'usagi-dev-env.ps1'
. $EnvScript

$RepoRoot = $env:USAGI_DIR
$BuildRoot = Join-Path $ToolsRoot 'test-build\LevelConversion'
$TempRoot = Join-Path $BuildRoot 'TempUsagi'
$OutRoot = Join-Path $BuildRoot 'out'

Remove-Item -LiteralPath $BuildRoot -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path $TempRoot, $OutRoot | Out-Null
New-Item -ItemType Directory -Path (Join-Path $TempRoot 'Data\Entities') | Out-Null

@'
Inherits:
  - PropBase
ModelComponent:
  name: PBRSample/PBRSample.vmdf
'@ | Set-Content -Encoding ASCII -Path (Join-Path $TempRoot 'Data\Entities\PBRSampleProp.yml')

$LevelPath = Join-Path $BuildRoot 'minimal.lvl'
@'
<game xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns="gap" name="Game">
  <gameObjectFolder name="GameObjects" visible="true" locked="false">
    <folder name="Props" visible="true" locked="false">
      <gameObject xsi:type="gameObjectType" name="DirectModel" translate="1 2 3" rotate="0 0 0" scale="1 1 1" visible="true">
        <resource xsi:type="resourceReferenceType" uri="Models/PBRSample/PBRSample.cmdl" />
      </gameObject>
    </folder>
    <folder name="Instances" visible="true" locked="false">
      <gameObject xsi:type="gameObjectType" name="InstancedModelA" translate="1 2 3" rotate="0 0 0" scale="1 1 1" visible="true">
        <resource xsi:type="resourceReferenceType" uri="Entities/PBRSampleProp.yml" />
      </gameObject>
      <gameObject xsi:type="gameObjectType" name="InstancedModelB" translate="2 2 3" rotate="0 0 0" scale="1 1 1" visible="true">
        <resource xsi:type="resourceReferenceType" uri="Entities/PBRSampleProp.yml" />
      </gameObject>
    </folder>
    <folder name="SpawnPoints" visible="true" locked="false">
      <gameObject xsi:type="spawnPointType" name="SpawnA" team="1" translate="0 0 0" rotate="0 0 0" scale="1 1 1" visible="true" />
    </folder>
  </gameObjectFolder>
</game>
'@ | Set-Content -Encoding ASCII -Path $LevelPath

$env:USAGI_DIR = $TempRoot
$LevelConverter = Join-Path $RepoRoot 'Tools\python\lvl2vhir\lvl2vhir.py'
$HierarchyOut = Join-Path $OutRoot 'minimal.yml'
$PositionsOut = Join-Path $OutRoot 'minimal_pos.yml'
$InstancesOut = Join-Path $OutRoot 'minimal_inst.yml'

& python $LevelConverter --dist 5 $LevelPath $HierarchyOut $PositionsOut $InstancesOut
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

foreach ($Path in @($HierarchyOut, $PositionsOut, $InstancesOut)) {
    if (-not (Test-Path $Path)) {
        throw "Expected level conversion output was not produced: $Path"
    }
}

$Hierarchy = Get-Content $HierarchyOut -Raw
$Instances = Get-Content $InstancesOut -Raw

if ($Hierarchy -notmatch 'PBRSample/PBRSample\.vmdf') {
    throw 'Hierarchy conversion did not emit the current .vmdf model resource extension.'
}
if ($Hierarchy -match '\.vmdc') {
    throw 'Hierarchy conversion still emitted obsolete .vmdc model resource extension.'
}
if ($Instances -notmatch 'PBRSample/PBRSample\.vmdf') {
    throw 'Instance conversion did not emit the current .vmdf model resource extension.'
}
if ($Instances -match '\.vmdc') {
    throw 'Instance conversion still emitted obsolete .vmdc model resource extension.'
}

Write-Host "Level conversion smoke passed: $OutRoot"
