@echo off
REM Build with MSVC directly, no CMake required.

REM Already inside a Developer Command Prompt, so cl is on PATH.
if not "%VSCMD_ARG_TGT_ARCH%"=="" goto :build

REM Each candidate is tested on its own line: inside a parenthesised block cmd
REM expands %VCVARS% when it parses the block, before the set above has run.
set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if exist "%VCVARS%" goto :vcvars
set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
if exist "%VCVARS%" goto :vcvars
set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
if exist "%VCVARS%" goto :vcvars
set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if exist "%VCVARS%" goto :vcvars

echo Could not find vcvars64.bat. Run this from a Developer Command Prompt.
exit /b 1

:vcvars
call "%VCVARS%" >nul

:build
if not exist "%~dp0build" mkdir "%~dp0build"
pushd "%~dp0build"

cl /nologo /std:c++20 /EHsc /W4 /O2 ..\src\regression.cpp ..\src\main.cpp /Fe:train.exe || goto :fail
cl /nologo /std:c++20 /EHsc /W4 /O2 ..\src\regression.cpp ..\tests\test_regression.cpp /Fe:tests.exe || goto :fail

echo.
echo Built build\train.exe and build\tests.exe
echo Run them from the ml-housing folder so data\ is found.
popd
exit /b 0

:fail
echo Build failed.
popd
exit /b 1
