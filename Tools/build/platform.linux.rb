# Usagi Engine, Copyright (c) Vitei, Inc. 2013
require 'pathname'

require_relative 'platform.rb'

# Native Linux (x86_64) toolchain: clang/gcc driven directly by ninja.
#
# Unlike 'win', where ninja only generates code and MSBuild compiles,
# the Linux build compiles, archives and links from the ninja file.
#
# Environment overrides:
#   CC, CXX, AR    - compilers and archiver (default clang, clang++, ar)
#   VULKAN_SDK     - prefix containing include/vulkan (default: system)
#   PHYSX_DIR      - PhysX 'physx' directory (as on Windows)
#   PHYSX_BIN_DIR  - directory containing the PhysX static libraries
#                    (default: $PHYSX_DIR/bin/linux.clang/<config>)
class PlatformLinux < Platform

  def initialize(config)
    @config = config
  end

  def underscore_dirs_whitelist
    ['_linux', '_vulkan', '_fragment', '_dummy']
  end

  # The Oculus module is built on LibOVR, which is Windows-only
  def excluded_libs
    ['Oculus']
  end

  def cc
    ENV['CC'] || 'clang'
  end

  def cxx
    ENV['CXX'] || 'clang++'
  end

  def ar
    ENV['AR'] || 'ar'
  end

  def ld
    cxx
  end

  def debug_build?
    @config.build == 'debug'
  end

  def defines
    list = ['PLATFORM_LINUX',
            'EASTL_CUSTOM_FLOAT_CONSTANTS_REQUIRED',
            'EASTL_USER_DEFINED_ALLOCATOR',
            'PB_FIELD_32BIT',
            "'LUA_USER_H=<Engine/Framework/Script/LuaConf.h>'",
            'LUA_USE_POSIX',
            'PX_DEBUG=0',
            'PX_CHECKED=0']

    if debug_build?
      list += ['_DEBUG', 'DEBUG']
    else
      list += ['NDEBUG']
    end

    list << 'FINAL_BUILD' if @config.build == 'final'

    list
  end

  def physx_dir
    ENV['PHYSX_DIR'] || @config.build_path('Engine/ThirdParty/PhysX/physx')
  end

  def physx_config
    debug_build? ? 'debug' : 'release'
  end

  def physx_bin_dir
    ENV['PHYSX_BIN_DIR'] || "#{physx_dir}/bin/linux.clang/#{physx_config}"
  end

  def include_dirs
    dirs = []
    dirs << @config.project if @config.type == ConfigType::GAME
    dirs += ['.',
             @config.build_path('.'),
             @config.code_working_dir,
             @config.includes_output_dir,
             @config.build_path('Engine/ThirdParty/nanopb'),
             @config.build_path('Engine/ThirdParty/EASTL/include'),
             @config.build_path('Engine/ThirdParty/EASTL/test/packages/EABase/include/Common'),
             @config.build_path('Engine/ThirdParty/lua-5.3.2/src'),
             @config.build_path('Engine/ThirdParty/yaml-cpp/include'),
             @config.build_path('Engine/ThirdParty/gli'),
             @config.build_path('Engine/ThirdParty/gli/external'),
             "#{physx_dir}/include",
             "#{physx_dir}/../pxshared/include"]

    if @config.type == ConfigType::GAME
      dirs << "#{@config.includes_output_dir(false)}/#{@config.project}"
      dirs << @config.includes_output_dir(false)
    end

    dirs << "#{ENV['VULKAN_SDK']}/include" if ENV['VULKAN_SDK']

    dirs.uniq
  end

  def pkg_config(args)
    out = `pkg-config #{args} 2>/dev/null`.strip
    $?.success? ? out : ''
  end

  def cppflags
    flags = defines.map { |d| "-D#{d}" }
    flags += include_dirs.map { |d| "-I#{d}" }
    flags << pkg_config('--cflags sdl3')
    flags.reject(&:empty?)
  end

  def common_flags
    flags = ['-m64', '-pthread', '-fPIC', '-fno-strict-aliasing',
             '-Wall', '-Wno-unknown-pragmas', '-Wno-unused-variable',
             '-Wno-unused-function', '-Wno-unused-private-field',
             '-Wno-missing-braces', '-Wno-reorder', '-Wno-reorder-ctor',
             '-Wno-sign-compare', '-Wno-switch', '-Wno-char-subscripts',
             '-Wno-unknown-warning-option',
             # gli (third party) uses 'template' without an argument list,
             # which recent clang makes an error
             '-Wno-missing-template-arg-list-after-template-kw']

    if debug_build?
      flags += ['-g', '-O0']
    else
      flags += ['-g', '-O2']
    end

    flags
  end

  def cflags
    common_flags
  end

  def cxxflags
    common_flags + ['-std=c++17']
  end

  # Interpolated straight into the compile rules, so a string
  def c_cpp_dep_flags
    '-MMD -MF $out.d'
  end

  def arflags
    ['rcs']
  end

  def ldflags
    ['-m64', '-pthread', '-rdynamic']
  end

  def ld_response_file_arg
    '@$out.rsp'
  end

  def physx_libs
    # Static PhysX libraries, ordered so that a single-pass linker resolves
    # every dependency
    ['PhysXExtensions_static_64',
     'PhysXCharacterKinematic_static_64',
     'PhysXVehicle_static_64',
     'PhysXCooking_static_64',
     'PhysX_static_64',
     'PhysXPvdSDK_static_64',
     'PhysXCommon_static_64',
     'PhysXFoundation_static_64'].map { |l| "#{physx_bin_dir}/lib#{l}.a" }
  end

  def system_libs
    libs = [pkg_config('--libs sdl3'), pkg_config('--libs vulkan')]
    libs << '-lvulkan' if libs.last.empty?
    libs += ['-ldl', '-lm', '-lpthread']
    libs.reject(&:empty?)
  end
end
