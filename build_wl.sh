#!/bin/sh

mkdir -p build
cd build

# XDG Shell headers
if [ ! -f xdg-shell-protocol.c ]; then
wayland-scanner private-code \
  < /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml \
  > xdg-shell-protocol.c
fi
if [ ! -f xdg-shell-client-protocol.h ]; then
wayland-scanner client-header \
  < /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml \
  > xdg-shell-client-protocol.h
fi

gcc $BUILD_OPTIONS -o raika -Wall ../src/wl_platform.cpp xdg-shell-protocol.c -lwayland-client -lm -lrt -lxkbcommon