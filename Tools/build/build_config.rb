# Usagi Engine, Copyright (c) Vitei, Inc. 2013
require 'pathname'
require 'rake'

module PathFormat
  ABSOLUTE = 0
  RELATIVE = 1
end

module ConfigType
  ENGINE = 0
  GAME = 1
end

class BuildConfig
  attr_accessor :usagi_dir, :current_dir
  attr_accessor :build, :target_platform, :region, :project
  attr_accessor :type

  # Here 'nxe' refers to the Windows platform using the Nintendo APIs,
  # while 'nx' refers to the Switch hardware using the same APIs.
  # 'linux' is native x86_64 Linux and 'macos' native arm64 macOS, both
  # compiled and linked by ninja.
  VALID_PLATFORMS = ["win", "nxe", "nx", "linux", "macos"]

  # maps file extensions to the command used to compile them
  COMPILERS = {".c" => "cc", ".cpp" => "cxx", ".m" => "cm", ".mm" => "cmm"}

  BUILD_DIR='_build'
  INCLUDES_DIR='_includes'
  ROM_DIR = '_rom'
  ROMFILES_DIR = '_romfiles'
  TOOLS_DIR='Tools'

  def self.normalize_path(path)
    path.tr('\\', '/').sub(/^([a-z])/){|c| c.upcase}
  end

  def self.to_windows_path(path)
    path.tr('/', '\\')
  end

  # Paths used as the first token of a ninja command are spawned directly
  # via CreateProcess, which rejects multi-component forward-slash relative
  # paths - they must be Windows paths when building on Windows
  def self.to_spawnable_path(path)
    Gem.win_platform? ? to_windows_path(path) : path
  end

  # http://stackoverflow.com/a/5471032/1004609
  def self.which(cmd)
    exts = ENV['PATHEXT'] ? ENV['PATHEXT'].split(';') : ['']

    ENV['PATH'].split(File::PATH_SEPARATOR).each do |path|
      exts.each do |ext|
        exe = File.join(path, "#{cmd}#{ext}")
        return exe if File.executable? exe
      end
    end

    return nil
  end

  def initialize(usagi_dir, argv)
    # absolute path
    @usagi_dir = Pathname.new(BuildConfig.normalize_path(usagi_dir))

    @build           = argv[0]
    @target_platform = argv[1]

    if ! VALID_PLATFORMS.include?(@target_platform)
      abort("Invalid platform setting: #{@target_platform}.  Valid settings are: #{BuildConfig::VALID_PLATFORMS.join(" ")}")
    end

    @region          = argv[2]
    @project         = argv[3]
    # absolute path
    @current_dir = Pathname.new(BuildConfig.normalize_path(argv[4]))

    @type = ConfigType::GAME
    # USAGI_DIR may reach the engine through a link in the game directory
    # (game/Usagi -> ../engine): game paths are then spelled 'Usagi/...', as
    # on Windows, while the engine's own build still recognises itself.
    @is_engine_build = @usagi_dir.to_path.downcase == @current_dir.to_path.downcase ||
      BuildConfig.same_directory?(@usagi_dir, @current_dir)

    if @is_engine_build
      @type = ConfigType::ENGINE
    end
  end

  #####################################################################
  # Helper methods                                                    #
  #####################################################################

  def build_path(suffix, should_use_relative=true)
    path = Pathname.new(suffix)

    if (@type == ConfigType::GAME) && should_use_relative
      path = @usagi_dir.join(suffix).relative_path_from(@current_dir)
    end

    path.to_path
  end

  # True when ninja, rather than MSBuild, compiles and links the code
  def compiles_with_ninja?
    ['linux', 'macos'].include?(@target_platform)
  end

  # A path argument for a tool: backslashes on Windows, as the tools there
  # expect, unchanged elsewhere
  def self.native_path(path)
    Gem.win_platform? ? to_windows_path(path) : path
  end

  def self.same_directory?(a, b)
    File.realpath(a.to_path) == File.realpath(b.to_path)
  rescue SystemCallError
    false
  end

  # True when the build runs on a Linux host
  def self.linux_host?
    RUBY_PLATFORM.include?('linux')
  end

  # Code generation needs protoc 2.5 with the nanopb plugins and the systems
  # scanner. Windows runs the binaries checked in to Tools/bin. Linux runs a
  # protoc 2.5 from $USAGI_PROTOC, the plugins under Python 3
  # (Tools/python/nanopb_py3) and Tools/python/systems_scanner.py; see
  # Documents/LinuxPort.md. On other hosts (macOS) these outputs (protocol
  # buffer sources and per-system .yml files) must be generated on Windows
  # ('rake platform=<p> codegen') and copied over.
  def can_run_codegen?
    Gem.win_platform? || BuildConfig.linux_host?
  end

  # Data is cooked by the tools in Tools/Source: the checked-in Windows
  # binaries, or on Linux the same tools built natively into
  # _build/tools/linux/bin by Tools/Source/build_linux_tools.sh.
  def can_cook_data?
    Gem.win_platform? || BuildConfig.linux_host?
  end

  # The data format a platform reads. The Linux runtime reads the data cooked
  # for Windows, so a Linux build cooks 'win' data (into _romfiles/win).
  def data_platform
    @target_platform == 'linux' ? 'win' : @target_platform
  end

  # A data tool from Tools/Source: Tools/bin/<name>.exe on Windows, the
  # natively built _build/tools/linux/bin/<name> elsewhere
  def data_tool(name)
    if Gem.win_platform?
      BuildConfig::to_spawnable_path("#{tools_bin_dir}/#{name}.exe")
    else
      build_path("#{BUILD_DIR}/tools/linux/bin/#{name}")
    end
  end

  #####################################################################
  # Fixed paths                                                       #
  #####################################################################

  def working_dir
    "#{BUILD_DIR}/#{@target_platform}"
  end

  def data_list_file
    "#{working_dir}/dataList.d"
  end

  def code_working_dir
    "#{working_dir}/#{@build}"
  end

  #####################################################################
  # Paths present in both the game and engine                         #
  #####################################################################
  def build_id_header
    header = build_path('Engine/Core/BuildID.h')
    header = "#{@project}/BuildID.h" if @type == ConfigType::GAME

    header
  end

  def pch_source_file(source_root)
    "#{source_root}/_exclude/stdafx.cpp"
  end

  def pch_file(source_root, basename)
    "#{code_working_dir}/#{source_root}/#{basename}.pch"
  end

  def tools_dir
    build_path(TOOLS_DIR)
  end

  def tools_bin_dir
    build_path("#{TOOLS_DIR}/bin")
  end

  def tools_ruby_dir
    build_path("#{TOOLS_DIR}/ruby")
  end

  def tools_python_dir(should_use_relative=true)
    build_path("#{TOOLS_DIR}/python", should_use_relative)
  end

  def yml_output_dir(should_use_relative=true)
    build_path("#{BUILD_DIR}/yml", should_use_relative)
  end

  def protocol_output_dir(should_use_relative=true)
    build_path("#{BUILD_DIR}/proto", should_use_relative)
  end

  def protocol_ruby_output_dir(should_use_relative=true)
    build_path("#{BUILD_DIR}/ruby", should_use_relative)
  end

  def protocol_csharp_output_dir(should_use_relative=true)
    build_path("#{BUILD_DIR}/csharp", should_use_relative)
  end

  def includes_output_dir(should_use_relative=true)
    build_path(INCLUDES_DIR, should_use_relative)
  end

  def vs_projects_dir(should_use_relative=true)
    build_path("#{BUILD_DIR}/projects", should_use_relative)
  end

  def csharp_engine_root(should_use_relative=true)
    "#{protocol_csharp_output_dir(should_use_relative)}/Engine"
  end

  def core_csharp_protos_proj(should_use_relative=true)
    "#{csharp_engine_root(should_use_relative)}/CorePBs.csproj"
  end

  def usagi_csharp_protos_proj(should_use_relative=true)
    "#{csharp_engine_root(should_use_relative)}/UsagiPBs.csproj"
  end

  def textures_dir(should_use_relative=true)
    build_path('Data/Textures', should_use_relative)
  end

  def effects_dir(should_use_relative=true)
    dir = 'Data/GLSL/effects'

    build_path(dir, should_use_relative)
  end

  def pc_shader_dir(should_use_relative=true)
    build_path('Data/GLSL/shaders', should_use_relative)
  end

  def pc_shader_include_dir
    build_path('Data/GLSL/shaders/includes', true)
  end

  def pre_includes
    list =
      [
       build_path('Engine/ThirdParty/nanopb'),
       build_path('Engine/ThirdParty/lua-5.3.2/src'),
       build_path('Engine/ThirdParty/EASTL/source'),
       '.'
      ]

    if @type == ConfigType::GAME
      list = [@project] + list
    end

    list
  end

  def post_includes
    list =
      [
       code_working_dir,
       includes_output_dir
      ]

    if @type == ConfigType::GAME
      list << "#{includes_output_dir(false)}/#{@project}"
      list << includes_output_dir(false)
    end

    list
  end

  def lua_conf_header
    build_path('Engine/Framework/Script/LuaConf.h')
  end

  def compiler(file)
    ext = File.extname(file)

    COMPILERS[ext]
  end

  # TODO: add option to include project .proto files
  def protocol_definitions(should_use_relative=true)
    # 'Engine/*/**' rather than 'Engine/**': Ruby's ** does not descend into
    # symlinks/junctions, which is how add-on modules are usually mounted
    list = FileList[build_path("Engine/*/**/*.proto")].exclude(build_path("Engine/ThirdParty/nanopb/**/*")).exclude(build_path("Engine/Core/usagipb*"))
    list = FileList["#{@project}/**/*.proto"] if ! should_use_relative

    list
  end

  def protocol_python_classes(should_use_relative=true)
    protocol_definitions(should_use_relative).map{|p| File.join(tools_python_dir(should_use_relative), p.sub(/\.proto$/, '_pb2.py')) }
  end

  def protocol_csharp_classes(should_use_relative=true)
    protocol_definitions(should_use_relative).map{|p| File.join(protocol_csharp_output_dir(should_use_relative), p.sub(/\.proto$/, '.pb.cs')) }
  end

  def protocol_ruby_classes(should_use_relative=true)
    protocol_definitions(should_use_relative).map{|p| File.join(protocol_ruby_output_dir(should_use_relative), p.sub(/\.proto$/, '.pb.rb'))}
  end

  def pb_headers(should_use_relative=true)
    protocol_definitions(should_use_relative).map{|p| File.join(includes_output_dir(should_use_relative), p.sub(/\.proto$/, '.pb.h')) }
  end

  def lua_headers(should_use_relative=true)
    protocol_definitions(should_use_relative).map{|p| File.join(includes_output_dir(should_use_relative), p.sub(/\.proto$/, '.lua.h')) }
  end

  def protocol_headers(should_use_relative=true)
    pb_headers(should_use_relative) + lua_headers(should_use_relative)
  end

  #####################################################################
  # Engine-specific paths                                             #
  #####################################################################

  def libs
    dirs = FileList["#{build_path('Engine')}/*"]

    dirs.exclude { |f| !File.directory?(f) }.map { |f| f.gsub(/^.*Engine\//, '') }
  end

  def lib_output_dir
    build_path("_libs/#{@target_platform}")
  end

  def compiled_libs_dir
    "#{lib_output_dir}/#{@build}"
  end

  def get_lib_filename(lib)
    "#{compiled_libs_dir}/#{lib}.a"
  end

  def get_lib_resname(res)
    "#{compiled_libs_dir}/#{lib}.res"
  end

  def compiled_libs
    FileList["#{compiled_libs_dir}/**/*.a"].to_a
  end

  def build_id_generator
    "#{tools_ruby_dir}/generate_build_id.rb"
  end

  def yaml_combiner
    "#{tools_ruby_dir}/combine_yaml.rb"
  end

  def boilerplate_script
    "#{tools_ruby_dir}/boilerplate.rb"
  end

  def boilerplate_scan
    "#{tools_ruby_dir}/boilerplate_scan.rb"
  end

  def boilerplate_scan_options
    options = ""
    options = "-p #{@project} -s #{build_path('')}" if @type == ConfigType::GAME

    options
  end

  def boilerplate_tool
    if Gem.win_platform?
      BuildConfig::to_spawnable_path("#{tools_bin_dir}/systems-scanner.exe")
    else
      "#{tools_python_dir}/systems_scanner.py"
    end
  end

  def register_systems_template
    build_path('Templates/RegisterSystems.cpp.erb')
  end

  def system_boilerplate_template
    build_path('Templates/SystemBoilerplate.cpp.erb')
  end

  def nanopb_dir
    build_path('Engine/ThirdParty/nanopb')
  end

  def nanopb_proto_dir
    "#{nanopb_dir}/generator/proto"
  end

  # What the nanopb plugins import to read nanopb.proto's options. On
  # Windows that's protoc 2.5's Python 2 output, written into the nanopb
  # tree. Under Python 3 (Tools/python/nanopb_py3) it's a descriptor set of
  # nanopb.proto, which nanopb_py3/proto/nanopb_pb2.py loads.
  def nanopb_protocols
    if Gem.win_platform?
      ['nanopb_pb2.py', 'plugin_pb2.py'].map{|p| "#{nanopb_proto_dir}/#{p}" }
    else
      [nanopb_descriptor_set]
    end
  end

  def nanopb_descriptor_set
    build_path("#{BUILD_DIR}/proto/nanopb.desc")
  end

  def usagi_protocols
    [build_path('Engine/Core/usagipb_pb2.py')]
  end

  def nanopb_generator
    "#{nanopb_dir}/generator/nanopb_generator.py"
  end

  def nanopb_lua_generator
    "#{tools_python_dir}/nanopb_lua_generator.py"
  end

  def font_converter
    data_tool('FontCreator')
  end

  def cmdl_converter
    data_tool('Ayataka')
  end

  def shader_pack
    data_tool('ShaderPackage')
  end

  def cmdl_converter_dep_opts
    '-d$out.d'
  end

  def resource_packer
    data_tool('PakFileGen')
  end

  def vitei_audio_tool
    mono = ''
    mono = 'mono' if ! Gem.win_platform?

    "#{mono} #{tools_dir}/AudioTool/FSIDBuilder.exe"
  end

  def behavior_tree_converter
    "#{tools_ruby_dir}/yml2vbt.rb"
  end

  def xml_behavior_tree_converter
    "#{tools_ruby_dir}/xml2vbt.rb"
  end

  def script_dep_opt
    "--MF '$out.d'"
  end

  def entity_converter
    "#{tools_ruby_dir}/process_hierarchy.rb"
  end

  def entity_expander
    "#{tools_ruby_dir}/expand_erb.rb"
  end

  def entity_opts
    "-I Data/Entities -d Data/Components/Defaults.yml -m #{model_out_dir} #{script_dep_opt}"
  end

  def vpb_converter
    "#{tools_ruby_dir}/yml2vpb.rb"
  end

  def vpb_converter_opts
    "-d Data/Components/Defaults.yml"
  end

  def maya_data_converter
    "#{tools_ruby_dir}/maya2pb.rb"
  end

  def level_converter
    "#{tools_python_dir}/lvl2vhir/lvl2vhir.py"
  end

  def name_data_hash_list_tool
    "#{tools_ruby_dir}/nameDataHashListGen.rb"
  end

  def text_converter
    "#{tools_ruby_dir}/mstxt2pb.rb"
  end

  def vs_project_generator
    "#{tools_ruby_dir}/generate_project.rb"
  end

  def msbuild
    if ! ENV.has_key?('MSBUILD_DIR')
      raise "Error! Environment variable 'MSBUILD_DIR' is not defined!"
    end

    "\"#{ENV['MSBUILD_DIR']}\\MSBuild.exe\""
  end

  # protoc 2.5. Its plugin protocol and output must match what the
  # generated code was written against, so other hosts don't fall back to
  # whatever protoc is on PATH: Linux takes it from $USAGI_PROTOC (built by
  # Tools/linux/bootstrap.sh).
  def protoc
    return BuildConfig::to_spawnable_path("#{tools_bin_dir}/protoc") unless BuildConfig.linux_host?

    protoc = ENV['USAGI_PROTOC']
    if protoc.nil? || protoc.empty?
      abort("USAGI_PROTOC isn't set: point it at a protoc 2.5.0 (Tools/linux/bootstrap.sh " \
            "builds one) or source Tools/linux/usagi_env.sh")
    end
    protoc
  end

  def luac
    # Elsewhere a Lua 5.3 luac comes from $USAGI_LUAC (no data uses it yet)
    return ENV['USAGI_LUAC'] || 'luac5.3' unless Gem.win_platform?

    # Bytecode compiled with the 32-bit Lua compiler is not
    # compatible with 64-bit Lua VM. So we have to choose the Lua
    # compiler based on the platform.
    compiler = "#{tools_bin_dir}/lua_x64/luac53.exe"
    platform = ENV.fetch('VISUAL_STUDIO_PLATFORM', '')
    compiler = "#{tools_bin_dir}/luac53.exe" if platform == 'Win32'

    BuildConfig::to_spawnable_path(compiler)
  end

  # these methods return paths for tools to be run from the protobuf
  # compiler, so they must be returned as Windows paths when building
  # on Windows
  def protoc_gen_c
    return "#{tools_python_dir}/nanopb_py3/protoc-gen-nanopb" unless Gem.win_platform?

    BuildConfig::to_windows_path(build_path('Engine/ThirdParty/nanopb/generator/protoc-gen-nanopb.bat'))
  end

  def protoc_gen_cs
    command = "#{tools_dir}/protobuf-net/protogen.exe"
    command = BuildConfig::to_windows_path(command) if Gem.win_platform?

    command
  end

  def protoc_gen_lua
    return "#{tools_python_dir}/nanopb_py3/protoc-gen-nanopblua" unless Gem.win_platform?

    BuildConfig::to_windows_path("#{tools_python_dir}/protoc-gen-nanopblua.bat")
  end

  def protoc_gen_ruby
    command = BuildConfig::which('protoc-gen-ruby')
    command = BuildConfig::to_windows_path(command) if Gem.win_platform?

    command
  end

  #####################################################################
  # Game-specific paths                                               #
  #####################################################################
  def romfiles_dir
    "#{ROMFILES_DIR}/#{data_platform}"
  end

  def effects_out_dir
    "#{romfiles_dir}/Effects"
  end

  def audio_out_dir
    "#{romfiles_dir}/Audio"
  end

  def entities_out
    "#{romfiles_dir}/Entities.pak"
  end  

  def skeleton_out_dir
    "#{BUILD_DIR}/skel"
  end

  def effect_build_dir
    "#{BUILD_DIR}/Effects"
  end

  def shader_out_dir
    "#{romfiles_dir}/shaders"
  end

  def textures_out_dir
    "#{romfiles_dir}/Textures"
  end

  def model_out_dir
    "#{romfiles_dir}/Models"
  end

  def layout_out_dir
    "#{romfiles_dir}/Layout"
  end

  def terrain_out_dir
    "#{romfiles_dir}/Terrain"
  end

  def custom_effect_out_dir
    "#{romfiles_dir}/CustomFX"
  end

  def model_working_dir
    "#{working_dir}/Data/Models"
  end

  def font_out_dir
    "#{romfiles_dir}/Fonts"
  end

  def particle_dir
    'Data/Particle'
  end  

  def particle_out_dir
    "#{romfiles_dir}/Particle"
  end  

  def particle_working_dir
    "#{working_dir}/Data/Particle"
  end

  def project_csharp_protos_proj
    "#{protocol_csharp_output_dir(false)}/#{@project}/#{@project}PBs.csproj"
  end

  def model_dir
    'Data/Models'
  end

  def font_dir
    'Data/Fonts'
  end

  def custom_effect_dir(should_use_relative=true)
    build_path('Data/CustomFX', should_use_relative)
  end

  def supported_languages
    ['JP_Japanese']
  end

  def name_data_hash_list
    "#{romfiles_dir}/nameDataHash.bin"
  end

  # The executable for platforms that ninja links
  def game_executable
    "#{ROM_DIR}/#{@target_platform}/#{@build}/#{@project}"
  end

end
