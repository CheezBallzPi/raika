#!/bin/sh

mkdir -p build
cd build
gcc $BUILD_OPTIONS -o raika -Wall ../src/wl_platform.cpp -lwayland-client -lm