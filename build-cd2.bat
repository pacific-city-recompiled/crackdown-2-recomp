@echo off
rem Builds Crackdown 2 against an installed (patched) ReXGlue SDK. Usage: build-cd2.bat [sdk-install-dir]
setlocal
cd /d "%~dp0crackdown2"
for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath`) do set "VS=%%i"
call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
set "PATH=%ProgramFiles%\LLVM\bin;%VS%\VC\Tools\Llvm\x64\bin;%PATH%"
if "%~1"=="" (set "SDKI=%~dp0sdk\out\install\win-amd64") else (set "SDKI=%~1")
cmake --preset win-amd64-release -DCMAKE_PREFIX_PATH="%SDKI%" || exit /b 1
cmake --build --preset win-amd64-release
