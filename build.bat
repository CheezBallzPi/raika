@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
pushd build
cl -Zi ..\src\main.cpp User32.lib
popd