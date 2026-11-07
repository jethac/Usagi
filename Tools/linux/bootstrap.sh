#!/bin/sh
# Usagi Engine: one-time setup of a Linux host for code generation, data
# cooking and building. Installs everything user-locally (no root):
#
#   - Ruby gems (--user-install): ninja-gen, nokogiri, ruby-protocol-buffers
#     (protoc-gen-ruby and the entity cooker's runtime), matrix
#   - protoc 2.5.0, built from Google's release tarball, in
#     $USAGI_TOOLCHAIN/protoc-2.5
#   - a Python 3 venv in $USAGI_TOOLCHAIN/usagi-py with protobuf (for the
#     nanopb plugins), the libclang bindings (for the systems scanner) and
#     cmake (for Compressonator and fbx-to-vmdf, unless the system has one)
#
# Safe to rerun: each step is skipped when it's already done. Builds run
# under nice with $JOBS jobs (default 4). Afterwards:
#
#   . Tools/linux/usagi_env.sh
#   sh Tools/Source/build_linux_tools.sh        # the data tools
#
# System packages it expects (Arch names): base-devel (gcc, make), clang,
# ruby, python, pkgconf, sdl3, vulkan-icd-loader, vulkan-headers (or
# VULKAN_SDK), shaderc, freetype2, zlib, git, curl. ninja can be
# the distro's or in ~/.local/bin.
set -eu

HERE=$(cd "$(dirname "$0")" && pwd)
. "$HERE/usagi_env.sh"
JOBS=${JOBS:-4}
NICE="nice -n ${NICE_LEVEL:-10}"
mkdir -p "$USAGI_TOOLCHAIN"

missing=""
for tool in clang clang++ make ruby gem python3 pkg-config curl git ninja glslc; do
  command -v "$tool" >/dev/null 2>&1 || missing="$missing $tool"
done
pkg-config --exists sdl3 2>/dev/null || missing="$missing sdl3(pkg-config)"
if [ -n "$missing" ]; then
  echo "bootstrap: missing system tools:$missing" >&2
  echo "bootstrap: on Arch: pacman -S base-devel clang ruby python pkgconf sdl3 vulkan-icd-loader vulkan-headers shaderc freetype2 ninja git curl" >&2
  exit 1
fi

# --- Ruby gems -------------------------------------------------------------
gem_installed() { gem list -i "^$1\$" ${2:+-v "$2"} >/dev/null 2>&1; }
for spec in ninja-gen:0.1.10.0 nokogiri: ruby-protocol-buffers:1.6.1 matrix:; do
  name=${spec%%:*}; version=${spec#*:}
  if gem_installed "$name" "$version"; then
    echo "bootstrap: gem $name ok"
  else
    echo "bootstrap: gem install $name $version"
    gem install --user-install --no-document "$name" ${version:+-v "$version"}
  fi
done

# --- protoc 2.5.0 ----------------------------------------------------------
PROTOBUF_URL=https://github.com/protocolbuffers/protobuf/releases/download/v2.5.0/protobuf-2.5.0.tar.gz
PROTOBUF_SHA256=c55aa3dc538e6fd5eaf732f4eb6b98bdcb7cedb5b91d3b5bdcf29c98c293f58e
if [ -x "$USAGI_PROTOC" ] && "$USAGI_PROTOC" --version | grep -q 'libprotoc 2\.5\.0'; then
  echo "bootstrap: protoc 2.5.0 ok ($USAGI_PROTOC)"
else
  echo "bootstrap: building protoc 2.5.0"
  work=$(mktemp -d)
  curl -sSL -o "$work/protobuf.tar.gz" "$PROTOBUF_URL"
  echo "$PROTOBUF_SHA256  $work/protobuf.tar.gz" | sha256sum -c - >/dev/null
  tar -xzf "$work/protobuf.tar.gz" -C "$work"
  # 2.5.0 predates C++11; build it as C++98 with today's compilers
  (cd "$work/protobuf-2.5.0" &&
    $NICE ./configure --prefix="$USAGI_TOOLCHAIN/protoc-2.5" --disable-shared \
      CXXFLAGS="-O2 -std=gnu++98 -fpermissive -Wno-error" > "$work/configure.log" 2>&1 &&
    $NICE make -j"$JOBS" -C src protoc > "$work/make.log" 2>&1) ||
    { echo "bootstrap: protoc build failed, logs in $work" >&2; exit 1; }
  mkdir -p "$(dirname "$USAGI_PROTOC")"
  cp "$work/protobuf-2.5.0/src/protoc" "$USAGI_PROTOC"
  rm -rf "$work"
  "$USAGI_PROTOC" --version
fi

# --- Python 3 venv -----------------------------------------------------------
# protobuf 3.20.3 is the runtime the generated code was checked against;
# libclang provides clang.cindex (the scanner loads the system libclang.so
# through it, see Tools/python/systems_scanner.py); cmake is only for
# Compressonator, when the system has none.
PY_VENV=$(dirname "$(dirname "$USAGI_PYTHON")")
if [ ! -x "$USAGI_PYTHON" ]; then
  echo "bootstrap: creating Python venv $PY_VENV"
  python3 -m venv "$PY_VENV"
fi
if "$USAGI_PYTHON" -c 'import google.protobuf, clang.cindex' 2>/dev/null; then
  echo "bootstrap: Python packages ok"
else
  "$USAGI_PYTHON" -m pip install -q "protobuf==3.20.3" "libclang==18.1.1"
fi
if ! command -v cmake >/dev/null 2>&1 && [ ! -x "$PY_VENV/bin/cmake" ]; then
  "$USAGI_PYTHON" -m pip install -q cmake
fi


echo "bootstrap: done. Next: . $HERE/usagi_env.sh; sh $HERE/../Source/build_linux_tools.sh"
