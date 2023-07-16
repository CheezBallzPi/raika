#!/bin/sh

mkdir -p build
cd build
gcc $BUILD_OPTIONS -o raika -Wall ../src/xcb_platform.cpp -lxcb -lm