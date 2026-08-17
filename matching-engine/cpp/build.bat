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

cl /nologo /std:c++20 /EHsc /W4 /O2 ..\src\order_book.cpp ..\src\reference_trace.cpp /Fe:reference_trace.exe || goto :fail
cl /nologo /std:c++20 /EHsc /W4 /O2 ..\src\order_book.cpp ..\tests\test_order_book.cpp   /Fe:tests.exe           || goto :fail
cl /nologo /std:c++20 /EHsc /W4 /O2 ..\src\order_book.cpp ..\bench\benchmark.cpp          /Fe:benchmark.exe       || goto :fail

echo.
echo Built reference_trace.exe, tests.exe and benchmark.exe in build\
popd
exit /b 0

:fail
echo Build failed.
popd
exit /b 1
