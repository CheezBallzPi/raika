#!/bin/sh

mkdir -p build
cd build

gcc $BUILD_OPTIONS -o raika -Wall ../src/sdl_platform.cpp \
  -lm -lvulkan `sdl2-config --cflags --libs`