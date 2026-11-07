# Usagi Engine: environment for building on a Linux host.
#
#   . Tools/linux/usagi_env.sh
#
# Points the build at the user-local toolchain that Tools/linux/bootstrap.sh
# installs (no root needed). Every variable can be set beforehand to use
# something else. USAGI_DIR (the engine checkout a game builds against) is
# left to the caller.
#
#   USAGI_TOOLCHAIN  where bootstrap.sh installs things (~/.local/opt)
#   USAGI_PROTOC     protoc 2.5.0; codegen needs exactly this version
#   USAGI_PYTHON     Python 3 with protobuf and the libclang bindings, for
#                    the nanopb plugins and the systems scanner
#   USAGI_LIBCLANG   optional: the libclang the systems scanner loads
#   FBXSDK_DIR       Autodesk FBX SDK, for building Ayataka
#   VULKAN_SDK       optional prefix with include/vulkan (else the system's)

: "${USAGI_TOOLCHAIN:=$HOME/.local/opt}"
export USAGI_TOOLCHAIN
export USAGI_PROTOC="${USAGI_PROTOC:-$USAGI_TOOLCHAIN/protoc-2.5/bin/protoc}"
export USAGI_PYTHON="${USAGI_PYTHON:-$USAGI_TOOLCHAIN/usagi-py/bin/python}"
export FBXSDK_DIR="${FBXSDK_DIR:-$USAGI_TOOLCHAIN/fbxsdk}"
# cmake (for Compressonator, in Tools/Source/build_linux_tools.sh): the
# system's, else the one bootstrap.sh puts in the venv
if [ -z "${CMAKE:-}" ] && ! command -v cmake >/dev/null 2>&1 &&
   [ -x "$(dirname "$USAGI_PYTHON")/cmake" ]; then
  export CMAKE="$(dirname "$USAGI_PYTHON")/cmake"
fi
if [ -z "${VULKAN_SDK:-}" ] && [ -d "$USAGI_TOOLCHAIN/vulkan-headers/include/vulkan" ]; then
  export VULKAN_SDK="$USAGI_TOOLCHAIN/vulkan-headers"
fi

# Ruby gems installed with --user-install, and ninja in ~/.local/bin
_usagi_gem_bin="$(ruby -e 'print Gem.user_dir' 2>/dev/null)/bin"
case ":$PATH:" in
  *":$_usagi_gem_bin:"*) ;;
  *) PATH="$_usagi_gem_bin:$PATH" ;;
esac
case ":$PATH:" in
  *":$HOME/.local/bin:"*) ;;
  *) PATH="$HOME/.local/bin:$PATH" ;;
esac
export PATH
unset _usagi_gem_bin
