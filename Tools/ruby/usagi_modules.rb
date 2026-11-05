# Usagi Engine
# Discovery of add-on engine modules.
#
# An add-on module lives at Engine/<Name>/ (usually a git submodule) and has an
# Engine/<Name>/usagi_module.yml manifest with at least `name` and `guid`.
# Like the built-in modules it must also provide _exclude/stdafx.cpp (the
# precompiled-header stub, just `#include "Engine/Common/Common.h"`).
# The engine project generator uses this to create the module's Visual Studio
# project, and game project templates use it to reference those projects.
require 'yaml'

module UsagiModules
  MANIFEST = 'usagi_module.yml'

  # Returns the parsed manifests of every add-on module under <usagi_dir>/Engine,
  # sorted by name. Raises if a manifest is malformed, so mistakes are loud.
  def self.manifests(usagi_dir)
    root = usagi_dir.to_s.tr('\\', '/')	# Dir.glob only understands forward slashes
    Dir.glob(File.join(root, 'Engine', '*', MANIFEST)).sort.map do |path|
      m = YAML.load_file(path) || {}
      dir = File.basename(File.dirname(path))
      raise "#{path}: missing 'name'" unless m['name']
      raise "#{path}: 'name' (#{m['name']}) must match its directory (#{dir})" unless m['name'] == dir
      raise "#{path}: missing 'guid'" unless m['guid'] =~ /\A\h{8}-\h{4}-\h{4}-\h{4}-\h{12}\z/
      m
    end
  end

  # Extra preprocessor flags for compiling a module's own sources with ninja
  # (the counterpart of _msbuild/Module.props), from an optional manifest
  # section:
  #
  #   ninja:
  #     include_dirs: [_vendor/Foo/include]   # relative to the module
  #     defines: [FOO_ENABLE_VULKAN]
  #
  # Returns '' for modules without one.
  def self.ninja_cppflags(manifest, module_dir)
    ninja = manifest['ninja'] || {}
    flags = (ninja['include_dirs'] || []).map { |d| "-I#{File.join(module_dir, d)}" }
    flags += (ninja['defines'] || []).map { |d| "-D#{d}" }
    flags.join(' ')
  end

  # MSBuild ProjectReference items for every add-on module, for use in a game's
  # .vcxproj.erb template:  <%= UsagiModules.project_references %>
  def self.project_references(usagi_dir = ENV['USAGI_DIR'])
    manifests(usagi_dir).map do |m|
      "    <ProjectReference Include=\"$(USAGI_DIR)\\_build\\projects\\Engine\\#{m['name']}\\#{m['name']}.vcxproj\">\n" \
      "      <Project>{#{m['guid'].downcase}}</Project>\n" \
      "    </ProjectReference>\n"
    end.join
  end
end
