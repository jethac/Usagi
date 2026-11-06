REM %1 is the output dir where the executable exists
REM %2 is build platform, that is x64/Win32
REM %3 is build type, Debug/Release
REM %4 is the full path to the solution dir

copy %USAGI_DIR%\Engine\ThirdParty\glew\bin\release\%2\glew32.dll %1

