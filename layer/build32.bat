@echo off
rem 32-bit build of the layer, for 32-bit OpenXR games (e.g. Fallout New Vegas via TTWVR)
call "G:\Claude\Tools\VSBuildTools\Common7\Tools\VsDevCmd.bat" -arch=x86 >nul
set CMAKE="G:\Claude\Tools\VSBuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
%CMAKE% -S "%~dp0." -B "%~dp0build32" -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release >nul || exit /b 1
%CMAKE% --build "%~dp0build32" --target OpenXRCompanion 2>&1
