# Usagi Engine, Copyright (c) Vitei, Inc. 2013
require_relative 'platform.linux.rb'

# Native macOS (arm64) toolchain: Apple clang driven directly by ninja, as
# on Linux. Vulkan runs on Metal through MoltenVK; the window, input and
# audio come from SDL3.
#
# Environment overrides (as well as those of PlatformLinux):
#   VULKAN_SDK         - the LunarG SDK's macOS directory (containing
#                        include/vulkan and lib/libvulkan.dylib)
#   MACOS_MIN_VERSION  - deployment target (default 13.0)
#   PHYSX_BIN_DIR      - directory containing the PhysX static libraries
#                        (default: $PHYSX_DIR/bin/mac.arm64/<config>)
#
# SDL3 is found with pkg-config (Homebrew's sdl3, or a local build with
# PKG_CONFIG_PATH pointing at it).
class PlatformMacOS < PlatformLinux

  def underscore_dirs_whitelist
    ['_osx', '_posix', '_vulkan', '_fragment', '_sdl']
  end

  def platform_defines
    ['PLATFORM_OSX']
  end

  def min_version
    ENV['MACOS_MIN_VERSION'] || '13.0'
  end

  def arch_flags
    ['-arch', 'arm64', "-mmacosx-version-min=#{min_version}"]
  end

  def ldflags
    # The app bundle keeps its dylibs in Contents/Frameworks
    arch_flags + ['-Wl,-rpath,@executable_path/../Frameworks']
  end

  # ld64 searches every archive until nothing is left unresolved, so the
  # engine libraries need no grouping (and it has no --start-group)
  def link_group(libs)
    libs
  end

  def physx_bin_dir
    ENV['PHYSX_BIN_DIR'] || "#{physx_dir}/bin/mac.arm64/#{physx_config}"
  end

  def vulkan_libs
    if ENV['VULKAN_SDK']
      # The loader finds MoltenVK through the SDK's ICD manifest; the app
      # bundle carries its own copy (see Tools/ruby/macos_app_bundle.rb)
      ["-L#{ENV['VULKAN_SDK']}/lib", '-lvulkan', "-Wl,-rpath,#{ENV['VULKAN_SDK']}/lib"]
    else
      [pkg_config('--libs vulkan')]
    end
  end

  def system_libs
    libs = [pkg_config('--libs sdl3')] + vulkan_libs
    libs += ['-lm', '-lpthread']
    libs.reject(&:empty?)
  end
end
