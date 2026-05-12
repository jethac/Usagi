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

$BuildDir = Join-Path $ToolsRoot 'test-build\SystemScheduler'
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null

$Source = Join-Path $TestDir 'SystemSchedulerTests.cpp'
$ThreadSource = Join-Path $UsagiRoot 'Engine\Core\Thread\_win\Thread_ps.cpp'
$Exe = Join-Path $BuildDir 'SystemSchedulerTests.exe'
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

$CommandParts = @(
    "`"$VcVars`" >nul &&",
    'cl /nologo /std:c++17 /EHsc /MTd /Zi /wd4291'
) + $Defines + $Includes + @(
    "/Fo$BuildDir\",
    "/Fe$Exe",
    $Source,
    $ThreadSource
)

$Command = $CommandParts -join ' '

cmd /c $Command
& $Exe
