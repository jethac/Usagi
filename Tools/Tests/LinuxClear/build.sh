#!/bin/sh
# Builds the LinuxClear bring-up test against the engine libraries.
# Run from the engine root after 'rake platform=linux build=<build> libs'.
# Usage: Tools/Tests/LinuxClear/build.sh [debug|release]
set -e
BUILD=${1:-debug}
NINJA_FILE=_build/linux/$BUILD/build.ninja
OUT_DIR=_build/linux/$BUILD/Tools/Tests/LinuxClear

# Compile with the same flags as the engine
flags() { sed -n "s/^$1 = //p" "$NINJA_FILE"; }
mkdir -p "$OUT_DIR"
eval "${CXX:-clang++} -c $(flags cxxflags) $(flags cppflags) Tools/Tests/LinuxClear/LinuxClear.cpp -o $OUT_DIR/LinuxClear.o"

${CXX:-clang++} $(flags ldflags) -o "$OUT_DIR/LinuxClear" "$OUT_DIR/LinuxClear.o" \
  -Wl,--start-group _libs/linux/$BUILD/*.a -Wl,--end-group \
  $(pkg-config --libs sdl3) -lvulkan -ldl -lpthread
echo "Built $OUT_DIR/LinuxClear"
