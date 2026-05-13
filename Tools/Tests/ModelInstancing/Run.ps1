param()

$ErrorActionPreference = 'Stop'

$WorktreeRoot = (& git -C $PSScriptRoot rev-parse --show-toplevel).Trim()
$RepoParent = Split-Path -Parent $WorktreeRoot
$ToolsRoot = Join-Path $RepoParent 'tools'
if (-not (Test-Path (Join-Path $ToolsRoot 'usagi-dev-env.ps1'))) {
    $ToolsRoot = Join-Path (Split-Path -Parent $RepoParent) 'tools'
}
$ToolsRoot = [System.IO.Path]::GetFullPath($ToolsRoot)
$EnvScript = Join-Path $ToolsRoot 'usagi-dev-env.ps1'
. $EnvScript

$BootstrapUsagiDir = $env:USAGI_DIR
$RepoRoot = $WorktreeRoot
$env:USAGI_DIR = $RepoRoot
$BuildRoot = Join-Path $ToolsRoot 'test-build\ModelInstancing'
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

$LevelPath = Join-Path $BuildRoot 'model_instances.lvl'
@'
<game xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns="gap" name="Game">
  <gameObjectFolder name="GameObjects" visible="true" locked="false">
    <folder name="Instances" visible="true" locked="false">
      <gameObject xsi:type="gameObjectType" name="InstancedModelA" translate="1 2 3" rotate="0 0 0" scale="1 1 1" visible="true">
        <resource xsi:type="resourceReferenceType" uri="Entities/PBRSampleProp.yml" />
      </gameObject>
      <gameObject xsi:type="gameObjectType" name="InstancedModelB" translate="4 5 6" rotate="0 0 0" scale="1 1 1" visible="true">
        <resource xsi:type="resourceReferenceType" uri="Entities/PBRSampleProp.yml" />
      </gameObject>
    </folder>
  </gameObjectFolder>
</game>
'@ | Set-Content -Encoding ASCII -Path $LevelPath

$env:USAGI_DIR = $TempRoot
$LevelConverter = Join-Path $RepoRoot 'Tools\python\lvl2vhir\lvl2vhir.py'
$HierarchyOut = Join-Path $OutRoot 'model_instances.yml'
$PositionsOut = Join-Path $OutRoot 'model_instances_pos.yml'
$InstancesOut = Join-Path $OutRoot 'model_instances_inst.yml'
$InstancePbOut = Join-Path $OutRoot 'model_instances_inst.pb'

& python $LevelConverter --dist 10 $LevelPath $HierarchyOut $PositionsOut $InstancesOut
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

foreach ($Path in @($HierarchyOut, $PositionsOut, $InstancesOut)) {
    if (-not (Test-Path $Path)) {
        throw "Expected model instancing conversion output was not produced: $Path"
    }
}

$Instances = Get-Content $InstancesOut -Raw

if ($Instances -notmatch 'Instance:\s+true') {
    throw 'Instance conversion did not mark the exported set as instanced.'
}
if ($Instances -notmatch 'Format:\s+transform') {
    throw 'Instance conversion did not emit transform instance data.'
}
if ($Instances -notmatch 'Length:\s+2') {
    throw 'Instance conversion did not emit the expected instance count.'
}
if ($Instances -notmatch 'ModelName:\s+PBRSample/PBRSample\.vmdf') {
    throw 'Instance conversion did not preserve the .vmdf model resource name.'
}
if ($Instances -match '\.vmdc') {
    throw 'Instance conversion still emitted obsolete .vmdc model resource extension.'
}

$NodeMatches = [regex]::Matches($Instances, 'name:\s+PBRSampleProp\.yml')
if ($NodeMatches.Count -ne 2) {
    throw "Instance conversion emitted $($NodeMatches.Count) nodes instead of 2."
}
foreach ($Expected in @(
    'translation:\s+\[-1\.0,\s+2\.0,\s+3\.0\]',
    'scale:\s+\[1\.0,\s+1\.0,\s+1\.0\]',
    'translation:\s+\[-4\.0,\s+5\.0,\s+6\.0\]'
)) {
    if ($Instances -notmatch $Expected) {
        throw "Instance conversion output was missing expected field pattern: $Expected"
    }
}

$RubyPbDir = Join-Path $RepoRoot '_build\ruby'
if ((-not (Test-Path (Join-Path $RubyPbDir 'Engine\Maths\Maths.pb.rb'))) -and $BootstrapUsagiDir) {
    $BootstrapRubyPbDir = Join-Path $BootstrapUsagiDir '_build\ruby'
    if (Test-Path (Join-Path $BootstrapRubyPbDir 'Engine\Maths\Maths.pb.rb')) {
        $RubyPbDir = $BootstrapRubyPbDir
    }
}
if (-not (Test-Path (Join-Path $RubyPbDir 'Engine\Maths\Maths.pb.rb'))) {
    Push-Location $RepoRoot
    try {
        rake platform=win build=debug,release projects
        if ($LASTEXITCODE -ne 0) {
            exit $LASTEXITCODE
        }
    }
    finally {
        Pop-Location
    }
    $RubyPbDir = Join-Path $RepoRoot '_build\ruby'
}

Push-Location $RepoRoot
try {
    ruby -I $RubyPbDir Tools/ruby/maya2pb.rb -o $InstancePbOut $InstancesOut
}
finally {
    Pop-Location
}
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}
if (-not (Test-Path $InstancePbOut)) {
    throw "maya2pb did not produce the expected InstanceSet binary: $InstancePbOut"
}
if ((Get-Item $InstancePbOut).Length -le 0) {
    throw "maya2pb produced an empty InstanceSet binary: $InstancePbOut"
}

$ModelResource = Get-Content (Join-Path $RepoRoot 'Engine\Resource\ModelResource.cpp') -Raw
if ($ModelResource -notmatch 'INSTANCE_TRANSFORM_ATTRIB_ID\s*=\s*11') {
    throw 'Runtime instance model resources no longer reserve the instance transform attribute binding.'
}
if ($ModelResource -notmatch 'bindings\[pipelineState\.uInputBindingCount\]\.Init\(g_instanceElements,\s*2,\s*VERTEX_INPUT_RATE_INSTANCE,\s*1\)') {
    throw 'Runtime instance model resources no longer declare an instance-rate transform stream.'
}

$ModelHeader = Get-Content (Join-Path $RepoRoot 'Engine\Scene\Model\Model.h') -Raw
if ($ModelHeader -notmatch 'LoadInstanced') {
    throw 'Model runtime no longer exposes an instanced load entry point.'
}

$ModelRenderNodes = Get-Content (Join-Path $RepoRoot 'Engine\Scene\Model\ModelRenderNodes.cpp') -Raw
if ($ModelRenderNodes -notmatch 'DrawIndexedEx\([^;]+m_uInstanceCount') {
    throw 'Model render nodes no longer draw with the loaded instance count.'
}

$ModelShader = Get-Content (Join-Path $RepoRoot 'Data\GLSL\shaders\includes\model_transform.inc') -Raw
if ($ModelShader -notmatch 'ao_instanceTransform') {
    throw 'Model shaders no longer consume per-instance transforms.'
}

Write-Host "Model instancing smoke passed: $InstancesOut -> $InstancePbOut"
