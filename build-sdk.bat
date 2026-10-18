@echo off
rem Builds and installs the ReXGlue SDK in sdk\ (after tools\apply-sdk-patches.sh). One-time and slow.
setlocal
cd /d "%~dp0sdk"
for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -property installationPath`) do set "VS=%%i"
call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
set "PATH=%ProgramFiles%\LLVM\bin;%VS%\VC\Tools\Llvm\x64\bin;%PATH%"
dir /AL "thirdparty\libmspack\cabextract\mspack\lzxd.c" >nul 2>&1 || (
  for %%m in (libmspack o1heap) do (
    git -C thirdparty\%%m config core.symlinks true
    git -C thirdparty\%%m rm -r -q --cached .
    git -C thirdparty\%%m reset -q --hard
  )
)
cmake --preset win-amd64 || exit /b 1
cmake --build out\build\win-amd64 --config Release --target install
