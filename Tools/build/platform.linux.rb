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
class PlatformLinux < Platform

  def initialize(config)
    @config = config
  end

  def underscore_dirs_whitelist
    ['_linux', '_posix', '_vulkan', '_fragment', '_sdl']
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

  # The PLATFORM_* symbols that select this platform's Common_ps.h
  def platform_defines
    ['PLATFORM_LINUX']
  end

  def defines
    list = platform_defines + [
            'EASTL_CUSTOM_FLOAT_CONSTANTS_REQUIRED',
            'EASTL_USER_DEFINED_ALLOCATOR',
            'PB_FIELD_32BIT',
            "'LUA_USER_H=<Engine/Framework/Script/LuaConf.h>'",
            'LUA_USE_POSIX']

    if debug_build?
      list += ['_DEBUG', 'DEBUG']
    else
      list += ['NDEBUG']
    end

    list << 'FINAL_BUILD' if @config.build == 'final'

    list
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
             @config.build_path('Engine/ThirdParty/box3d/include')]

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

  # Target architecture flags, used when compiling and linking
  def arch_flags
    ['-m64']
  end

  def common_flags
    flags = arch_flags + ['-pthread', '-fPIC', '-fno-strict-aliasing',
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
    # Box3D is C17
    common_flags + ['-std=gnu17']
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
    arch_flags + ['-pthread', '-rdynamic']
  end

  # The engine libraries depend on each other in both directions, so a
  # single-pass linker needs them grouped
  def link_group(libs)
    ['-Wl,--start-group'] + libs + ['-Wl,--end-group']
  end

  def ld_response_file_arg
    '@$out.rsp'
  end

  def system_libs
    libs = [pkg_config('--libs sdl3'), pkg_config('--libs vulkan')]
    libs << '-lvulkan' if libs.last.empty?
    libs += ['-ldl', '-lm', '-lpthread']
    libs.reject(&:empty?)
  end
end
