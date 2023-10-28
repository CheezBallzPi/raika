#!/bin/sh

set -e

mkdir -p build
cd build

glslc ../shaders/shader.vert -o vert.spv
glslc ../shaders/shader.frag -o frag.spv

if [ -z "${RAIKA_DEBUG}" ]
then
  set debugFlags=""
else
  echo "Building in debug mode..."
  set debugFlags="-DDEBUG"
fi

g++ $BUILD_OPTIONS $debugFlags -o raika -Wall ../src/sdl_platform.cpp \
  -lm -lvulkan `sdl2-config --cflags --libs`
