#!/bin/sh

mkdir -p build
cd build
gcc $BUILD_OPTIONS -Wall ../src/xcb_platform.cpp -lxcb -lm