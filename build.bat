@echo off

where /q cl
if ERRORLEVEL 1 (
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
)
pushd build
cl -Zi ..\src\win32_platform.cpp User32.lib Gdi32.lib
popd