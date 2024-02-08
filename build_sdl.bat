@echo off

where /q cl
if ERRORLEVEL 1 (
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
)
if NOT EXIST build mkdir build

if NOT DEFINED RAIKA_DEBUG (
  set debugFlags=
) else (
  echo Building in debug mode...
  set debugFlags=/Zi /DRAIKA_DEBUG 
)

pushd build

glslc ../shaders/shader.vert -o vert.spv
glslc ../shaders/shader.frag -o frag.spv

cl %debugFlags% ..\src\sdl_platform.cpp ^
  %VULKAN_SDK%\Lib\vulkan-1.lib %VULKAN_SDK%\Lib\SDL2.lib %VULKAN_SDK%\Lib\SDL2main.lib Shell32.lib ^
  /I%VULKAN_SDK%\Include /DWINDOWS /link /SUBSYSTEM:WINDOWS
popd