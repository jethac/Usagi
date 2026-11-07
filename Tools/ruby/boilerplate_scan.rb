# Usagi Engine, Copyright © Vitei, Inc. 2013
#!ruby
#boilerplate.rb
#Uses the boilerplate tool to scan headers for Systems information.

require 'erb'
require 'zlib'
require 'yaml'
require 'open3'
require 'tempfile'
require 'pathname'

require 'optparse'

$submodule_dir = nil
$project_dir = nil

optparser = OptionParser.new do |opts|
  opts.banner = "Usage: #{$0} [options]"
  opts.on( '-o file', 'Specifies the filename to output' ) do |f|
    $out = f
  end

  opts.on( '-d file', 'Specifies the filename to put dependency information in' ) do |f|
    $depfile = f
  end

  opts.on( '-s dir', 'Specifies submodule directory prefix' ) do |s|
    $submodule_dir = s
  end

  opts.on( '-p dir', 'Specifies project directory' ) do |p|
    $project_dir = p
  end

  opts.on( '-h', '--help', 'Display this screen' ) do
    puts opts
    exit
  end
end

optparser.parse!

ALL_HEADERS = ARGV
INCLUDES = ALL_HEADERS.map { |h| "#include \"#{h}\"" }
includes = Tempfile.new(["systems", ".h"])

begin
  includes.write(INCLUDES.join("\n"))

  if Gem.win_platform?
    boilerplate_tool = "Tools\\bin\\systems-scanner.exe"

    boilerplateflags = ""
    boilerplateflags = "-I#{$project_dir} " + boilerplateflags if !$project_dir.nil?
    include_dirs = [".", "Engine/ThirdParty/nanopb", "_includes", "Engine/ThirdParty/pthread-w32/include"]

    if !$submodule_dir.nil?
      boilerplate_tool = "#{$submodule_dir}\\" + boilerplate_tool
      boilerplateflags = "-I. -I_includes " + boilerplateflags + "-I#{$submodule_dir} "
      boilerplateflags += include_dirs.select{|d| d != "."}.map{|d| "-I#{$submodule_dir}/#{d} "}.join("")
    else
      boilerplateflags += include_dirs.map{|d| "-I#{d} "}.join("")
    end

    boilerplateflags = boilerplateflags +
                       "-I\"#{ENV["VS120COMNTOOLS"]}../../VC/include\" " +
                       "-I\"C:/Program Files (x86)/Windows Kits/8.1/Include/um\" " +
                       "-I\"C:/Program Files (x86)/Windows Kits/8.1/Include/shared\" " +
                       "-D_M_IA64 -DPLATFORM_PC -Dpid_t=int"
  else
    # Tools/python/systems_scanner.py replaces systems-scanner.exe here. It
    # needs libclang's Python bindings ('clang' module), so USAGI_PYTHON
    # should name an interpreter that has them.
    python = ENV['USAGI_PYTHON'] || 'python3'
    scanner = "Tools/python/systems_scanner.py"
    scanner = "#{$submodule_dir}/#{scanner}" if !$submodule_dir.nil?
    boilerplate_tool = "#{python} #{scanner}"

    # Search the same directories, in the same order, as the Windows tool
    # does: sourceFile in the output is spelled the way the include was found.
    sub = $submodule_dir.nil? ? "" : "#{$submodule_dir}/"
    include_dirs = ["."]
    include_dirs += ["_includes", $project_dir] if !$submodule_dir.nil?
    include_dirs << $submodule_dir if !$submodule_dir.nil?
    include_dirs += ["#{sub}Engine/ThirdParty/nanopb", "#{sub}_includes"]
    # Then what the headers need to parse on this platform
    include_dirs += ["Engine/ThirdParty/EASTL/include",
                     "Engine/ThirdParty/EASTL/test/packages/EABase/include/Common",
                     "Engine/ThirdParty/lua-5.3.2/src",
                     "Engine/ThirdParty/yaml-cpp/include",
                     "Engine/ThirdParty/gli",
                     "Engine/ThirdParty/gli/external",
                     "Engine/ThirdParty/box3d/include"].map { |d| "#{sub}#{d}" }
    include_dirs << "#{ENV['VULKAN_SDK']}/include" if ENV['VULKAN_SDK']
    include_dirs = include_dirs.compact.uniq

    platform_define = RUBY_PLATFORM =~ /darwin/ ? "PLATFORM_OSX" : "PLATFORM_LINUX"
    defines = [platform_define, "EASTL_CUSTOM_FLOAT_CONSTANTS_REQUIRED",
               "EASTL_USER_DEFINED_ALLOCATOR", "PB_FIELD_32BIT",
               "'LUA_USER_H=<Engine/Framework/Script/LuaConf.h>'", "LUA_USE_POSIX"]

    # Every engine source starts with Common.h (the PCH); headers scanned on
    # their own need it too, for uint32 and friends.
    boilerplateflags = include_dirs.map { |d| "-I#{d} " }.join("") +
                       defines.map { |d| "-D#{d} " }.join("") +
                       "-include Engine/Common/Common.h"
  end
  includes.close

  deps = $depfile ? "-d#{$depfile}" : ""
  stdin, stdout, stderr = Open3.popen3("#{boilerplate_tool} #{boilerplateflags} #{deps} -o #{$out} \"#{includes.path}\"")

  boilerplate_error = stderr.readlines[0]

  if boilerplate_error != nil
    print "#{boilerplate_tool} #{boilerplateflags} -H -o #{$out} #{ALL_HEADERS.join(" ")}"
    raise "Error! Boilerplate error: #{boilerplate_error}"
  end
ensure
  includes.close
  includes.unlink
end
