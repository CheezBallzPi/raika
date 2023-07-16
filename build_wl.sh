#!/bin/sh

mkdir -p build
cd build
gcc $BUILD_OPTIONS -Wall ../src/wl_platform.cpp -lwayland-client -lm