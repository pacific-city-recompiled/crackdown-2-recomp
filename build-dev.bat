@echo off
rem Like build-cd2.bat, but into out\build\dev so a running crackdown2.exe in the release folder is never touched.
setlocal
cd /d "%~dp0crackdown2"
for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath`) do set "VS=%%i"
call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
set "PATH=%ProgramFiles%\LLVM\bin;%VS%\VC\Tools\Llvm\x64\bin;%PATH%"
if "%~1"=="" (set "SDKI=%~dp0sdk\out\install\win-amd64") else (set "SDKI=%~1")
if not exist out\build\dev\CMakeCache.txt cmake -S . -B out\build\dev -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_PREFIX_PATH="%SDKI%" || exit /b 1
cmake --build out\build\dev
