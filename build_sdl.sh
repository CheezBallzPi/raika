#!/bin/sh

set -e

mkdir -p build
cd build

glslc ../shaders/shader.vert -o vert.spv
glslc ../shaders/shader.frag -o frag.spv

g++ $BUILD_OPTIONS -o raika -Wall ../src/sdl_platform.cpp \
  -lm -lvulkan `sdl2-config --cflags --libs`
