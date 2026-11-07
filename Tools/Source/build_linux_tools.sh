#!/bin/sh
# Usagi Engine, Copyright (c) Vitei, Inc. 2013
#
# build_linux_tools.sh [tool...]
#
# Builds the data-cooking tools natively on Linux (x86_64) with clang:
#
#   fbx-to-vmdf   FBX -> .vmdf models and skeleton XML, effect .yml -> .cfx
#                 (the Tools/Source/fbx-to-vmdf submodule, built with its CMake)
#   PakFileGen    textures (Compressonator), audio and layout .paks
#   ShaderPackage effect .yml -> shader .pak (GLSL -> SPIR-V via libshaderc)
#   FontCreator   font .yml -> MSDF font texture + .vpb
#
# into <engine>/_build/tools/linux/bin/. The build is incremental: the script
# writes <engine>/_build/tools/linux/build.ninja and runs ninja on it. Pass
# tool names to build only those (default: all four).
#
# The tools other than fbx-to-vmdf link the engine's own libraries, so the
# engine must have been generated and built for Linux first, in the
# configuration given by ENGINE_BUILD (rake platform=linux build=release
# ninja && ninja ... libs). fbx-to-vmdf is self-contained.
#
# Environment:
#   JOBS               ninja parallelism (default 4); run the script itself
#                      under nice to keep it polite, the compilers inherit it
#   ENGINE_BUILD       engine library configuration to link (default release)
#   CC, CXX            compilers (default clang, clang++)
#   COMPRESSONATOR_DIR Compressonator source (default: the engine submodule if
#                      checked out, else fetched once at the pinned commit into
#                      <engine>/_build/tools/linux/src/compressonator)
#   CMAKE              cmake for Compressonator and fbx-to-vmdf (default: PATH, then
#                      ~/.local/opt/usagi-buildtools/bin/cmake)
#
# Host packages (Arch names): clang, ninja, cmake, freetype2, shaderc, sdl3,
# vulkan-icd-loader, git (for the one-off Compressonator fetch).
set -eu

ENGINE=$(cd "$(dirname "$0")/../.." && pwd)
JOBS=${JOBS:-4}
ENGINE_BUILD=${ENGINE_BUILD:-release}
CC=${CC:-clang}
CXX=${CXX:-clang++}
OUT=$ENGINE/_build/tools/linux
TOOLS=${*:-fbx-to-vmdf PakFileGen ShaderPackage FontCreator}

# The engine submodule pin (Engine/ThirdParty/compressonator)
CMP_COMMIT=f4b53d79ec5abbb50924f58aebb7bf2793200b94
CMP_URL=https://github.com/GPUOpen-Tools/compressonator.git

die() { echo "build_linux_tools: $*" >&2; exit 1; }

CMAKE=${CMAKE:-$(command -v cmake || echo "$HOME/.local/opt/usagi-buildtools/bin/cmake")}

#####################################################################
# fbx-to-vmdf (FBX -> .vmdf), the Tools/Source/fbx-to-vmdf submodule #
# built with its own CMake; it needs nothing from the engine         #
#####################################################################
case " $TOOLS " in
*" fbx-to-vmdf "*)
  F2V=$ENGINE/Tools/Source/fbx-to-vmdf
  [ -f "$F2V/CMakeLists.txt" ] || die "no fbx-to-vmdf source in $F2V: git submodule update --init Tools/Source/fbx-to-vmdf"
  [ -x "$CMAKE" ] || die "cmake not found (set CMAKE)"
  F2VB=$ENGINE/_build/tools/linux/fbx-to-vmdf
  mkdir -p "$F2VB" "$ENGINE/_build/tools/linux/bin"
  if [ ! -f "$F2VB/build.ninja" ]; then
    (cd "$F2VB" && CC=$CC CXX=$CXX "$CMAKE" -G Ninja -DCMAKE_MAKE_PROGRAM="$(command -v ninja)" \
      -DCMAKE_BUILD_TYPE=Release "$F2V" > cmake.log 2>&1) || die "fbx-to-vmdf cmake failed, see $F2VB/cmake.log"
  fi
  ninja -C "$F2VB" -j "$JOBS" fbx-to-vmdf > "$F2VB/build.log" 2>&1 || die "fbx-to-vmdf build failed, see $F2VB/build.log"
  cp "$F2VB/fbx-to-vmdf" "$ENGINE/_build/tools/linux/bin/fbx-to-vmdf.new"
  mv -f "$ENGINE/_build/tools/linux/bin/fbx-to-vmdf.new" "$ENGINE/_build/tools/linux/bin/fbx-to-vmdf"
  echo "build_linux_tools: built fbx-to-vmdf in $ENGINE/_build/tools/linux/bin"
  TOOLS=$(echo " $TOOLS " | sed 's/ fbx-to-vmdf / /g')
  ;;
esac
[ -n "$(echo $TOOLS)" ] || exit 0

LIBDIR=$ENGINE/_libs/linux/$ENGINE_BUILD
[ -f "$LIBDIR/Core.a" ] || die "no engine libraries in $LIBDIR: build the engine (rake platform=linux build=$ENGINE_BUILD ninja; ninja -f _build/linux/$ENGINE_BUILD/build.ninja libs) first"
[ -d "$ENGINE/_includes/Engine" ] || die "no generated headers in $ENGINE/_includes: run the engine codegen first"

mkdir -p "$OUT/bin" "$OUT/obj"

#####################################################################
# Compressonator (PakFileGen's texture compression), built with its #
# own CMake as static libraries                                     #
#####################################################################
need_cmp=
case " $TOOLS " in *" PakFileGen "*) need_cmp=1 ;; esac

if [ -n "$need_cmp" ]; then
  if [ -z "${COMPRESSONATOR_DIR:-}" ]; then
    if [ -f "$ENGINE/Engine/ThirdParty/compressonator/CMakeLists.txt" ]; then
      COMPRESSONATOR_DIR=$ENGINE/Engine/ThirdParty/compressonator
    else
      COMPRESSONATOR_DIR=$OUT/src/compressonator
      if [ ! -f "$COMPRESSONATOR_DIR/CMakeLists.txt" ]; then
        echo "build_linux_tools: fetching Compressonator $CMP_COMMIT"
        rm -rf "$COMPRESSONATOR_DIR"; mkdir -p "$COMPRESSONATOR_DIR"
        git -C "$COMPRESSONATOR_DIR" init -q
        git -C "$COMPRESSONATOR_DIR" fetch -q --depth 1 "$CMP_URL" "$CMP_COMMIT"
        git -C "$COMPRESSONATOR_DIR" checkout -q FETCH_HEAD
      fi
    fi
  fi
  [ -x "$CMAKE" ] || die "cmake not found (set CMAKE)"
  CMPB=$OUT/compressonator
  if [ ! -f "$CMPB/build.ninja" ]; then
    mkdir -p "$CMPB"
    # SDK libraries only. -include cstdint: some of its headers rely on
    # <cstdint> arriving transitively, which current libstdc++ doesn't do.
    (cd "$CMPB" && CC=$CC CXX=$CXX CXXFLAGS="-include cstdint" "$CMAKE" -G Ninja \
      -DCMAKE_MAKE_PROGRAM="$(command -v ninja)" -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
      -DOPTION_ENABLE_ALL_APPS=OFF -DOPTION_BUILD_CMP_SDK=ON \
      -DOPTION_CMP_QT=OFF -DOPTION_CMP_OPENCV=OFF -DOPTION_BUILD_KTX2=OFF \
      -DOPTION_BUILD_EXR=OFF "$COMPRESSONATOR_DIR" > cmake.log 2>&1) ||
      die "Compressonator cmake failed, see $CMPB/cmake.log"
  fi
  ninja -C "$CMPB" -j "$JOBS" CMP_Framework CMP_Core CMP_Core_SSE CMP_Core_AVX CMP_Core_AVX512 > "$CMPB/build.log" 2>&1 ||
    die "Compressonator build failed, see $CMPB/build.log"
fi

#####################################################################
# The tools                                                         #
#####################################################################
T=Engine/ThirdParty
S=Tools/Source

# Same defines and include paths as the engine libraries
# (Tools/build/platform.linux.rb), plus TOOL_BUILD
DEFINES="-DPLATFORM_LINUX -DEASTL_CUSTOM_FLOAT_CONSTANTS_REQUIRED -DEASTL_USER_DEFINED_ALLOCATOR
  -DPB_FIELD_32BIT '-DLUA_USER_H=<Engine/Framework/Script/LuaConf.h>' -DLUA_USE_POSIX -DTOOL_BUILD"
[ "$ENGINE_BUILD" = debug ] && DEFINES="$DEFINES -D_DEBUG -DDEBUG" || DEFINES="$DEFINES -DNDEBUG"

INCLUDES="-I. -I_build/linux/$ENGINE_BUILD -I_includes -I_build/proto -I$T/nanopb -I$T/EASTL/include
  -I$T/EASTL/test/packages/EABase/include/Common -I$T/lua-5.3.2/src -I$T/yaml-cpp/include
  -I$T/gli -I$T/gli/external -I$T/box3d/include"
[ -n "${VULKAN_SDK:-}" ] && INCLUDES="$INCLUDES -I$VULKAN_SDK/include"

WARN="-Wall -Wno-unknown-pragmas -Wno-unused-variable -Wno-unused-function -Wno-unused-private-field
  -Wno-missing-braces -Wno-reorder -Wno-reorder-ctor -Wno-sign-compare -Wno-switch -Wno-char-subscripts
  -Wno-unknown-warning-option -Wno-missing-template-arg-list-after-template-kw -Wno-unused-but-set-variable
  -Wno-format-security -Wno-deprecated-declarations -Wno-writable-strings -Wno-invalid-offsetof"
OPT="-O2"
[ "$ENGINE_BUILD" = debug ] && OPT="-O0 -g"
# ninja variables are one line each
DEFINES=$(echo $DEFINES); INCLUDES=$(echo $INCLUDES); WARN=$(echo $WARN)

FREETYPE_CFLAGS=$(pkg-config --cflags freetype2)
FREETYPE_LIBS=$(pkg-config --libs freetype2)
SYSLIBS="$(pkg-config --libs sdl3 2>/dev/null) $(pkg-config --libs vulkan 2>/dev/null || echo -lvulkan) -ldl -lm -lpthread"
ENGINE_LIBS="-Wl,--start-group $(ls "$LIBDIR"/*.a | tr '\n' ' ') -Wl,--end-group"

N=$OUT/build.ninja
NEW=$N.new
{
  echo "# Generated by Tools/Source/build_linux_tools.sh, do not edit"
  echo "ninja_required_version = 1.7"
  echo "builddir = $OUT"
  echo "cc = $CC"
  echo "cxx = $CXX"
  echo "base = -m64 -pthread -fPIC -fno-strict-aliasing $OPT $WARN $DEFINES"
  echo "cflags = \$base -std=gnu17"
  echo "cxxflags = \$base -std=c++17 -include $S/ToolsPosix.h"
  echo "includes = $INCLUDES"
  echo "rule cxx"
  echo "  command = \$cxx -MMD -MF \$out.d \$cxxflags \$includes \$extra -c \$in -o \$out"
  echo "  depfile = \$out.d"
  echo "  deps = gcc"
  echo "  description = CXX \$out"
  echo "rule cc"
  echo "  command = \$cc -MMD -MF \$out.d \$cflags \$includes \$extra -c \$in -o \$out"
  echo "  depfile = \$out.d"
  echo "  deps = gcc"
  echo "  description = CC \$out"
  echo "rule ar"
  echo "  command = rm -f \$out && ar rcs \$out \$in"
  echo "  description = AR \$out"
  echo "rule link"
  echo "  command = \$cxx -m64 -pthread -o \$out \$in \$libs"
  echo "  description = LINK \$out"
} > "$NEW"

# obj <source> [extra flags] -> prints the object path, adds a build edge
obj() {
  src=$1; shift
  o=$OUT/obj/$(echo "$src" | sed 's,\.[^.]*$,.o,')
  rule=cxx
  case $src in *.c) rule=cc ;; esac
  echo "build $o: $rule $src" >> "$NEW"
  [ $# -gt 0 ] && echo "  extra = $*" >> "$NEW"
  printf '%s ' "$o"
}

# msdfgen (FontCreator): the core plus the FreeType font importer
MSDF_OBJS=
for f in "$T"/msdfgen/core/*.cpp $T/msdfgen/ext/import-font.cpp; do
  MSDF_OBJS="$MSDF_OBJS $(obj "$f" -DMSDFGEN_USE_CPP11 "$FREETYPE_CFLAGS")"
done
echo "build $OUT/obj/libmsdfgen.a: ar $MSDF_OBJS" >> "$NEW"

# ResourceLib (material definitions, for ShaderPackage)
RES_OBJS=$(obj $S/ResourceLib/MaterialDefinition/MaterialDefinitionExporter.cpp -I$S/ResourceLib)

built=
for tool in $TOOLS; do
  case $tool in
  PakFileGen)
    CMPS=$COMPRESSONATOR_DIR
    CMPL=$OUT/compressonator/lib
    pak_flags="-I$S/ResourceLib -I$CMPS/cmp_compressonatorlib -I/usr/include/glslang/Public -D_LINUX"
    objs=
    for f in FileFactory FileFactoryWin main; do
      objs="$objs $(obj $S/PakFileGen/$f.cpp $pak_flags)"
    done
    libs="$CMPL/libCMP_Framework.a $CMPL/libCMP_Core.a $CMPL/libCMP_Core_SSE.a $CMPL/libCMP_Core_AVX.a $CMPL/libCMP_Core_AVX512.a $ENGINE_LIBS $SYSLIBS"
    ;;
  ShaderPackage)
    sp_flags="-I$S/ResourceLib -I/usr/include/shaderc"
    objs=
    for f in IShaderCompiler OpenGLShaderCompiler VulkanShaderCompiler main; do
      objs="$objs $(obj $S/ShaderPackage/$f.cpp $sp_flags)"
    done
    objs="$objs $RES_OBJS"
    libs="-lshaderc_shared $ENGINE_LIBS $SYSLIBS"
    ;;
  FontCreator)
    objs="$(obj $S/FontCreator/main.cpp -I$T/msdfgen "$FREETYPE_CFLAGS") $OUT/obj/libmsdfgen.a"
    libs="$FREETYPE_LIBS $ENGINE_LIBS $SYSLIBS"
    ;;
  *) die "unknown tool $tool" ;;
  esac
  echo "build $OUT/bin/$tool: link $objs | $(ls "$LIBDIR"/*.a | tr '\n' ' ')" >> "$NEW"
  echo "  libs = $libs" >> "$NEW"
  built="$built $OUT/bin/$tool"
done

# An object listed by more than one tool is emitted more than once; keep the first
awk '/^build /{ if (seen[$2]++) { skip=1; next } skip=0 } /^  /{ if (skip) next } { print }' "$NEW" > "$N"
rm -f "$NEW"

[ -n "$built" ] || die "nothing to build"
cd "$ENGINE"
ninja -f "$N" -j "$JOBS" $built
echo "build_linux_tools: built$(for b in $built; do printf ' %s' "$(basename "$b")"; done) in $OUT/bin"
