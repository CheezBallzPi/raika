# Raika

A game engine

# Building

Final executable will be created in the `build` folder. Set env variable `RAIKA_DEBUG` to enable debug prints.

## Windows

Install [Visual Studio](https://visualstudio.microsoft.com).

Install the [Vulkan SDK](https://vulkan.lunarg.com) and run `build_sdl.bat`. When installing the SDK make sure to install SDL2 and GLM with it as the build script looks for them in
the Vulkan SDK directory.

## Linux

Prerequisite libraries:
- SDL2
- Vulkan
- GLM
- `gcc`

Run `build_sdl.sh`.