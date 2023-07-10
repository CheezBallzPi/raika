#!/bin/sh

mkdir -p build
cd build
gcc $BUILD_OPTIONS -Wall ../src/linux_platform.cpp -lxcb -lm