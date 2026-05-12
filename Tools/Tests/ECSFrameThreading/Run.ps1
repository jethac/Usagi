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

$BuildDir = Join-Path $ToolsRoot 'test-build\ECSFrameThreading'
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null

$Source = Join-Path $TestDir 'ECSFrameThreadingTests.cpp'
$Exe = Join-Path $BuildDir 'ECSFrameThreadingTests.exe'
$VcVars = 'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat'

if (-not (Test-Path $VcVars)) {
    throw "Unable to find Visual Studio environment script: $VcVars"
}

$Defines = @(
    '/DPLATFORM_PC',
    '/D_DEBUG',
    '/DFINAL_BUILD',
    '/DVK_USE_PLATFORM_WIN32_KHR',
    '/D"LUA_USER_H=<Engine/Framework/Script/LuaConf.h>"',
    '/DPB_FIELD_32BIT',
    '/DNN_SWITCH_ENABLE_HOST_IO',
    '/DEASTL_CUSTOM_FLOAT_CONSTANTS_REQUIRED=1',
    '/D_CRT_SECURE_NO_WARNINGS'
)

$Includes = @(
    "/I$UsagiRoot",
    "/I$UsagiRoot\_includes",
    "/I$UsagiRoot\_build\win\debug",
    "/I$UsagiRoot\Engine\ThirdParty\nanopb",
    "/I$UsagiRoot\Engine\ThirdParty\EASTL\include",
    "/I$UsagiRoot\Engine\ThirdParty\EASTL\test\packages\EABase\include\Common",
    "/I$UsagiRoot\Engine\ThirdParty\lua-5.3.2\src",
    "/I$UsagiRoot\Engine\ThirdParty\yaml-cpp\include",
    "/I$UsagiRoot\Engine\ThirdParty\gli",
    "/I$UsagiRoot\Engine\ThirdParty\gli\external",
    "/I$env:VK_SDK_PATH\Include"
)

$Sources = @(
    $Source,
    (Join-Path $UsagiRoot 'Engine\Framework\Component.cpp'),
    (Join-Path $UsagiRoot 'Engine\Framework\ComponentEntity.cpp'),
    (Join-Path $UsagiRoot 'Engine\Framework\ComponentStats.cpp'),
    (Join-Path $UsagiRoot 'Engine\Framework\ComponentSystemInputOutputs.cpp'),
    (Join-Path $UsagiRoot 'Engine\Framework\GameComponents.cpp'),
    (Join-Path $UsagiRoot 'Engine\Framework\NewEntities.cpp'),
    (Join-Path $UsagiRoot 'Engine\Framework\Signal.cpp'),
    (Join-Path $UsagiRoot 'Engine\Framework\SystemCoordinator.cpp'),
    (Join-Path $UsagiRoot 'Engine\Framework\SystemId.cpp'),
    (Join-Path $UsagiRoot 'Engine\Memory\UnTypesafeFastPool.cpp'),
    (Join-Path $UsagiRoot 'Engine\Memory\MemHeap.cpp'),
    (Join-Path $UsagiRoot 'Engine\Memory\_win\MemHeap_ps.cpp'),
    (Join-Path $UsagiRoot 'Engine\Core\Thread\_win\Thread_ps.cpp'),
    (Join-Path $UsagiRoot 'Engine\Core\Utility.cpp'),
    (Join-Path $UsagiRoot '_build\proto\Engine\Framework\Component.pb.cpp'),
    (Join-Path $UsagiRoot '_build\proto\Engine\Framework\Event.pb.cpp'),
    (Join-Path $UsagiRoot 'Engine\ThirdParty\nanopb\pb_common.c'),
    (Join-Path $UsagiRoot 'Engine\ThirdParty\nanopb\pb_decode.c')
)

$Libraries = @(
    (Join-Path $env:USAGI_STL_LIB 'x64\Debug\EASTL.lib')
)

$CommandParts = @(
    "`"$VcVars`" >nul &&",
    'cl /nologo /std:c++17 /EHsc /MTd /Zi /wd4291'
) + $Defines + $Includes + @(
    "/Fo$BuildDir\",
    "/Fe$Exe"
) + $Sources + $Libraries

$Command = $CommandParts -join ' '

cmd /c $Command
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

& $Exe
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}
