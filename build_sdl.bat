@echo off

where /q cl
if ERRORLEVEL 1 (
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
)
if NOT EXIST build mkdir build

set ARG=%1
set debugFlags=
set rkdebugFlags=
set vkdebugFlags=

FOR %%A IN (%*) DO (
    if "%%A"=="/r" (
      echo Raika Debug
      set debugFlags=/Zi
      set rkdebugFlags=/DRAIKA_DEBUG
    )
    if "%%A"=="/v" (
      echo Vulkan Debug
      set debugFlags=/Zi
      set vkdebugFlags=/DVULKAN_DEBUG
    )
)

pushd build

glslc ../shaders/shader.vert -o vert.spv
glslc ../shaders/shader.frag -o frag.spv

cl %debugFlags% %rkdebugFlags% %vkdebugFlags% ..\src\sdl_platform.cpp ^
  %VULKAN_SDK%\Lib\vulkan-1.lib %VULKAN_SDK%\Lib\SDL2.lib %VULKAN_SDK%\Lib\SDL2main.lib Shell32.lib ^
  /I%VULKAN_SDK%\Include /I..\include /DWINDOWS /link /SUBSYSTEM:WINDOWS
popd