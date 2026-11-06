# Usagi Engine, Copyright (c) Vitei, Inc. 2013
#
# Makes a self-contained macOS application bundle from a game executable
# built with 'rake platform=macos' and its cooked data:
#
#   <Name>.app/Contents/
#     Info.plist
#     MacOS/<Name>                         the executable
#     Frameworks/                          SDL3, the Vulkan loader, MoltenVK
#     Resources/romfiles/                  cooked data (SDLUTIL::UsePackagedData)
#     Resources/vulkan/icd.d/MoltenVK_icd.json
#
# The executable's non-system dylibs are copied into Frameworks and
# relinked to @rpath, which the macOS platform sets to
# @executable_path/../Frameworks. The Vulkan loader looks for driver
# manifests in Contents/Resources/vulkan/icd.d, so the bundle runs
# without the Vulkan SDK installed. The bundle is signed ad hoc.
#
# Usage:
#   ruby Tools/ruby/macos_app_bundle.rb -e _rom/macos/release/UsagiGP \
#     -r _romfiles/win -o _rom/macos/release/UsagiGP.app \
#     [-n UsagiGP] [-i com.example.usagigp] [--vulkan-sdk $VULKAN_SDK]

require 'optparse'
require 'fileutils'
require 'json'

options = {
  :vulkan_sdk => ENV['VULKAN_SDK'],
  :min_version => ENV['MACOS_MIN_VERSION'] || '13.0',
  :version => '1.0'
}

OptionParser.new do |opts|
  opts.banner = 'Usage: macos_app_bundle.rb -e EXE -r ROMFILES -o OUT.app [options]'
  opts.on('-e', '--exe FILE', 'Game executable') { |v| options[:exe] = v }
  opts.on('-r', '--romfiles DIR', 'Cooked data directory') { |v| options[:romfiles] = v }
  opts.on('-o', '--output DIR', 'Bundle to write (Name.app)') { |v| options[:output] = v }
  opts.on('-n', '--name NAME', 'Bundle and executable name') { |v| options[:name] = v }
  opts.on('-i', '--identifier ID', 'CFBundleIdentifier') { |v| options[:identifier] = v }
  opts.on('--version VER', 'CFBundleShortVersionString') { |v| options[:version] = v }
  opts.on('--vulkan-sdk DIR', 'LunarG SDK macOS directory (default $VULKAN_SDK)') { |v| options[:vulkan_sdk] = v }
  opts.on('-h', '--help', 'Show this message') { puts opts; exit }
end.parse!

%i[exe romfiles output].each do |k|
  raise "Missing --#{k} (see --help)" unless options[k]
end
raise "#{options[:exe]} not found" unless File.file?(options[:exe])
raise "#{options[:romfiles]} not found" unless File.directory?(options[:romfiles])

name = options[:name] || File.basename(options[:exe])
identifier = options[:identifier] || "com.usagi.#{name.downcase}"
app = options[:output]
contents = File.join(app, 'Contents')
macos_dir = File.join(contents, 'MacOS')
frameworks = File.join(contents, 'Frameworks')
resources = File.join(contents, 'Resources')

def run(*cmd)
  system(*cmd) or raise "Command failed: #{cmd.join(' ')}"
end

# Libraries outside the OS that a binary links against, as written in its
# load commands
def linked_dylibs(binary)
  `otool -L "#{binary}"`.lines.drop(1).map { |l| l.strip.split(' (').first }
    .reject { |p| p.start_with?('/usr/lib/', '/System/') }
end

# Resolves an install name (absolute, @rpath or @loader_path relative)
def resolve_dylib(path, search_dirs)
  return File.realpath(path) if path.start_with?('/') && File.exist?(path)
  base = File.basename(path)
  search_dirs.each do |dir|
    candidate = File.join(dir, base)
    return File.realpath(candidate) if File.exist?(candidate)
  end
  raise "Can't find #{path} (searched #{search_dirs.join(', ')})"
end

FileUtils.rm_rf(app)
FileUtils.mkdir_p([macos_dir, frameworks, resources])

exe = File.join(macos_dir, name)
FileUtils.cp(options[:exe], exe)
FileUtils.chmod(0755, exe)

search_dirs = []
search_dirs << File.join(options[:vulkan_sdk], 'lib') if options[:vulkan_sdk]
search_dirs += ['/opt/homebrew/lib', '/usr/local/lib']

# Copy each non-system dylib once, keeping its file name (which matches
# its install name's last component), and point everything at @rpath
pending = [exe]
copied = {}
until pending.empty?
  binary = pending.shift
  linked_dylibs(binary).each do |install_name|
    base = File.basename(install_name)
    unless copied[base]
      src = resolve_dylib(install_name, search_dirs)
      dst = File.join(frameworks, base)
      FileUtils.cp(src, dst)
      FileUtils.chmod(0755, dst)
      run('install_name_tool', '-id', "@rpath/#{base}", dst)
      copied[base] = dst
      pending << dst
    end
    run('install_name_tool', '-change', install_name, "@rpath/#{base}", binary) unless install_name == "@rpath/#{base}"
  end
end

# Dylibs in Frameworks find each other through the executable's rpath, but
# give them their own too so they also load on their own (e.g. via dlopen)
copied.each_value do |dylib|
  system('install_name_tool', '-add_rpath', '@loader_path', dylib, :err => File::NULL)
end

# The SDK's loader build has no rpath of its own pointing outside; drop the
# SDK lib directory rpath the macOS platform adds for running from _rom
system('install_name_tool', '-delete_rpath', File.join(options[:vulkan_sdk], 'lib'), exe, :err => File::NULL) if options[:vulkan_sdk]

# MoltenVK is loaded by the Vulkan loader through its ICD manifest, not
# linked, so it has to be added by hand
moltenvk = resolve_dylib('libMoltenVK.dylib', search_dirs)
FileUtils.cp(moltenvk, File.join(frameworks, 'libMoltenVK.dylib'))
run('install_name_tool', '-id', '@rpath/libMoltenVK.dylib', File.join(frameworks, 'libMoltenVK.dylib'))

icd_dir = File.join(resources, 'vulkan', 'icd.d')
FileUtils.mkdir_p(icd_dir)
File.write(File.join(icd_dir, 'MoltenVK_icd.json'), JSON.pretty_generate(
  'file_format_version' => '1.0.0',
  'ICD' => {
    # Relative to the manifest
    'library_path' => '../../../Frameworks/libMoltenVK.dylib',
    'api_version' => '1.2.0',
    'is_portability_driver' => true
  }
) + "\n")

# Cooked data. Copy rather than link, so the bundle is self-contained;
# skip run-time output directories
FileUtils.mkdir_p(File.join(resources, 'romfiles'))
Dir.children(options[:romfiles]).reject { |c| ['_dump', '_savedata'].include?(c) }.each do |child|
  FileUtils.cp_r(File.join(options[:romfiles], child), File.join(resources, 'romfiles'))
end

File.write(File.join(contents, 'Info.plist'), <<PLIST)
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleDevelopmentRegion</key>
	<string>en</string>
	<key>CFBundleExecutable</key>
	<string>#{name}</string>
	<key>CFBundleIdentifier</key>
	<string>#{identifier}</string>
	<key>CFBundleInfoDictionaryVersion</key>
	<string>6.0</string>
	<key>CFBundleName</key>
	<string>#{name}</string>
	<key>CFBundlePackageType</key>
	<string>APPL</string>
	<key>CFBundleShortVersionString</key>
	<string>#{options[:version]}</string>
	<key>CFBundleVersion</key>
	<string>#{options[:version]}</string>
	<key>LSMinimumSystemVersion</key>
	<string>#{options[:min_version]}</string>
	<key>LSApplicationCategoryType</key>
	<string>public.app-category.racing-games</string>
	<key>NSHighResolutionCapable</key>
	<true/>
	<key>GCSupportsControllerUserInteraction</key>
	<true/>
</dict>
</plist>
PLIST

# Ad-hoc signature: required on Apple silicon, and after install_name_tool
# has invalidated the original signatures
run('codesign', '--force', '--deep', '--sign', '-', app)

puts "Wrote #{app}"
