# Native Linux Port

This note is the plan for running Usagi games natively on Linux
(x86_64, Vulkan, Wayland or X11). The first target is UsagiGP on an
Arch/Hyprland machine with an NVIDIA GTX 1070. It covers what is
platform-specific in the engine today and the approach chosen for each
piece.

## Ground rules

- `linux` is a new target platform alongside `win`, `nx` and `nxe`.
  The Windows build must keep working unchanged.
- Platform code goes in `_linux/` subdirectories, picked by the existing
  `OS_HEADER` / underscore-directory machinery, in the same shape as
  `_win/`. The Vulkan backend stays in `_vulkan/` and is shared; the few
  places where it calls into Win32 are moved behind an OS-level interface.
- The preprocessor symbol is `PLATFORM_LINUX`. `PLATFORM_PC` keeps
  meaning "Windows desktop", since that is how the code uses it (Win32
  headers, `WINUTIL`, DirectInput, and so on).
- **Data and generated code** were produced on Windows and copied over
  in the first pass. A Linux host now generates the code and cooks the
  data itself (see "Generated code" and "Data tools" below).

## Layout after the shared platform layers

The port was first written in `_linux` directories. Since macOS, iOS and
Android use the same code, it has been split into shared layers, and the
paths below that say `_linux` now live here:

| Directory | Contents | Selected by |
| --- | --- | --- |
| `_sdl` | window and events (`Engine/Core/_sdl/SDLUtil`, namespace `SDLUTIL`), HID, the Vulkan surface (`Engine/Graphics/Device/_sdl/VulkanWindow_ps.cpp`), audio | `USE_SDL`, `HID_HEADER`, `AUDIO_HEADER` |
| `_posix` | files, threads, timers, memory, crash handling, modules, sockets, locale, profiling, `os_ps`, `VulkanIncludes.h`, the shared `Common_ps.h` | `OS_HEADER`, `PLATFORM_POSIX` |
| `_linux` | `Common_ps.h` (includes the `_posix` one), and anything Linux-only | `PLATFORM_LINUX` |

A game's entry point for every SDL platform is `main/_sdl/SDLMain.cpp`.
The Linux build whitelists `_linux`, `_posix`, `_sdl`, `_vulkan` and
`_fragment`. See `ApplePort.md` for macOS and iOS.

## Window, surface and events: SDL3

SDL3 is used for the window, the Vulkan surface, keyboard, mouse, text
input and gamepads.

- It is already installed on the target and handles Wayland natively
  (with X11 as a fallback), including fractional scaling, fullscreen and
  focus, which would be a lot of code to write against `wl_*` and
  `xdg-shell` directly.
- `SDL_Vulkan_GetInstanceExtensions` and `SDL_Vulkan_CreateSurface`
  choose `VK_KHR_wayland_surface` or `VK_KHR_xlib_surface` at run time, so
  the Vulkan backend does not need compile-time knowledge of the
  windowing system.
- SDL's gamepad API includes the community controller database, which
  covers more pads than XInput and DirectInput together.

SDL is used only from `_linux` platform files. It does not become an
engine-wide dependency, and nothing outside the platform layer includes
it.

`Engine/Core/_linux/LinuxUtil.{h,cpp}` is the counterpart of
`WINUTIL`. It creates the `SDL_Window`, owns the event pump, and
forwards events to `Input_ps` and to `usg::GameMessage` with the same
message IDs the Win32 `WindowProc` uses (`'ONSZ'`, `'WSZE'`, `'WMIN'`).
`WindHndl` is `SDL_Window*` on Linux. A game's `main/_linux/LinuxMain.cpp`
is as small as `WinMain.cpp`: it creates the window and calls `GameMain`.

## Vulkan backend

Most of `_vulkan/` is portable. The Win32 coupling is in three places:

| Code | Windows | Linux |
| --- | --- | --- |
| Instance extensions (`GFXDevice_ps::Init`) | `VK_KHR_win32_surface` | from `SDL_Vulkan_GetInstanceExtensions` |
| Surface creation (`Display_ps::Initialise`) | `vkCreateWin32SurfaceKHR` | `SDL_Vulkan_CreateSurface` |
| Client size, minimise wait (`Display_ps`) | `GetClientRect`, `Sleep` | `SDL_GetWindowSizeInPixels` |
| Display enumeration (`GFXDevice_ps::EnumerateDisplays`) | `EnumDisplayDevices` | `SDL_GetDisplays`, `SDL_GetFullscreenDisplayModes` |
| HDR probe (`Display_ps`) | DXGI output colour space | Vulkan surface formats only (see below) |
| Screenshot to clipboard | `SetClipboardData(CF_DIB)` | not supported yet (logs a warning) |

These calls move behind a small set of functions declared in the
existing OS-specific `VulkanIncludes.h` (`Engine/Graphics/Device/_win/`
and `_linux/`), implemented in a new `VulkanWindow_ps.cpp` per OS. The
shared Vulkan code calls those functions and no longer includes
`WinUtil.h`.

HDR: on Windows the DXGI probe decides whether to ask for an HDR10
swap chain. On Linux the decision comes from the formats and colour
spaces the surface reports (`VK_COLOR_SPACE_HDR10_ST2084_EXT`). The
target outputs SDR at 1080p, so the SDR path is what gets tested.

The Vulkan loader is linked dynamically (`-lvulkan`) as on Windows.

## Input / HID

`Engine/HID/_linux/`:

- `Input_ps` keeps the same public interface as the Windows version
  (`RegisterKeyStateChange`, `RegisterMouseButtonChange`,
  `RegisterInputChar`, and so on), so shared HID code is untouched.
- Keyboard: the engine indexes keys by Windows virtual-key code
  (`KEYBOARD_KEY_COUNT`, `VK_*` constants in game code). The SDL event
  handler translates `SDL_Scancode` to the same VK numbering, so key
  bindings in data and code keep working.
- Mouse: SDL mouse events feed `Mouse_ps`.
- Gamepads: `SDLGamepad` implements `IGamepad` over `SDL_Gamepad`,
  replacing `XInputPad` and `DirectInputJoystick`. It handles hot plug
  through `SDL_EVENT_GAMEPAD_ADDED/REMOVED`.
- `VirtualGamepad` (keyboard-as-pad) has a `_linux` copy that uses the
  engine's `KEYBOARD_KEY_*` codes. The Windows copy indexes the
  keyboard with `VK_*` codes, but the DirectInput keyboard fills it with
  engine codes, so its arrow and numpad bindings don't work.

## File I/O

`Engine/Core/File/_linux/file_ps.cpp` uses stdio, with `/`-separated
save and dump directories (`_savedata/`, `_dump/`) and `mkdir`/`opendir`
in place of `CreateDirectory`/`FindFirstFile`.

Two things differ from Windows and are handled in the platform layer:

- **Case sensitivity.** Cooked data and code refer to resources with
  whatever case the author typed. If an exact open fails, `File_ps`
  resolves the path one component at a time, case-insensitively, and
  caches the result.
- **Separators.** Paths are normalised from `\` to `/` before opening.

The user file dialogs (`UserFileOpenPath`, `UserFileSavePath`) are
editor features. On Linux they use `SDL_ShowOpenFileDialog` later; for
now they return `false`.

## Threads and synchronisation

`Engine/Core/Thread/_linux/` uses pthreads: `Thread_ps` wraps
`pthread_create`/`pthread_join`, with priorities mapped to `nice`-style
hints where the scheduler allows; `CriticalSection_ps` wraps a recursive
`pthread_mutex_t`, because Win32 critical sections are recursive and
engine code relies on that.

## Timers

`Engine/Core/Timer/_linux/TimeTracker` uses
`clock_gettime(CLOCK_MONOTONIC)` in place of `QueryPerformanceCounter`.

## Memory

`Engine/Memory/_linux/` mirrors `_win`. Aligned allocation uses
`posix_memalign`/`free` in place of `_aligned_malloc`/`_aligned_free`.

## Debug, logging, crash handling

`Engine/Debug/_linux/`: `DEBUG_PRINT` goes to stderr (in place of
`OutputDebugString`), `DEBUG_BREAK` is `__builtin_trap()` under a
debugger check, and the fatal-error stack dump that `Debug.cpp` builds
with `DbgHelp` uses `backtrace()`/`backtrace_symbols_fd()` from glibc.
The DbgHelp code in shared `Debug.cpp` moves to the Windows platform
file.

## OS, modules, profile, locale, network

- `OS_ps`: quit requests come from `SDL_EVENT_QUIT`.
- `ModuleManager`: `LoadLibrary`/`GetProcAddress`/`FreeLibrary` become
  `dlopen`/`dlsym`/`dlclose` behind a small `Module_ps` shim, and
  `HMODULE` becomes an opaque module handle. UsagiGP loads no modules,
  so this is compile-and-link only for now.
- `Profile_ps`: `QueryPerformanceCounter` becomes `clock_gettime`.
- `Locale_ps`: reads `LANG`/`LC_ALL`.
- `NetPlatform_ps`: BSD sockets (the Windows code is Winsock, which is
  close to BSD already).

## Audio

The engine already has an `_dummy` audio backend behind `AUDIO_HEADER`.
The Linux build selects it first, so the game runs silent. The real
backend later is SDL3 audio (it is already linked, and it uses PipeWire
on the target) behind a new `_sdl` audio directory. That keeps XAudio2
as the Windows backend.

## Types and compiler

- `Engine/Common/_linux/Common_ps.h` defines the basic types,
  `OS_HEADER` → `_linux`, `API_HEADER` → `_vulkan`,
  `AUDIO_HEADER` → `_dummy`, `ALIGNED_VAR` via `alignas`, `PACK` via
  `__attribute__((packed))`, and `DEPENDENT_TEMPLATE` as `template`.
- `char16` is `char16_t` on Linux. `wchar_t` is 32-bit there, and
  `char16` is used as UTF-16 (`Utf.h` typedefs `UTF16` from it).
- Compiler: clang (gcc should also work). C++17, no exceptions, no
  RTTI, as on Windows. `-Werror` is off until the warning baseline is
  clean.
- Code that compiled only because MSVC is permissive (missing
  `typename`/`template`, non-standard headers, Win32 types used in
  shared code) is fixed in place. Those fixes are standard C++ and keep
  MSVC building.

## Build system

`Tools/build/platform.linux.rb` adds a `PlatformLinux` toolchain:
`clang`/`clang++`, `ar`, link with `clang++`, and gcc-style depfiles.
The underscore whitelist is `_linux`, `_vulkan`, `_fragment`, `_dummy`.
`ThirdParty` and `PostFX` get `.sources.linux` lists matching their
`.sources.win`.

On Windows the ninja file only generates code, and MSBuild compiles. On
Linux the ninja file compiles, archives (`_libs/linux/<build>/*.a`) and
links directly. The game generator follows the same pattern to compile
the game sources and link the executable.

### Generated code

Code generation runs natively on a Linux host and produces the same files
as on Windows, byte for byte, apart from the generators' timestamp
comments (`BuildConfig#can_run_codegen?`):

- `.pb.h/.pb.cpp` (nanopb) and `.lua.h/.lua.cpp` (Lua bindings) come
  from protoc 2.5.0, built from Google's release tarball by
  `Tools/linux/bootstrap.sh` and found through `$USAGI_PROTOC`. A newer
  protoc isn't used: its built-in `--ruby_out` shadows the
  `protoc-gen-ruby` plugin, and the generated code was written against
  2.5.
- The two plugins run under Python 3 through
  `Tools/python/nanopb_py3/protoc-gen-nanopb[lua]`. protoc 2.5's Python
  output (nanopb's `proto/nanopb_pb2.py`) is Python 2 only, so
  `nanopb_py3/run_plugin.py` puts a `proto` package of its own first on
  the path: `nanopb_pb2` is built from a descriptor set of `nanopb.proto`
  that the build writes to `_build/proto/nanopb.desc`, and `plugin_pb2` is
  the protobuf runtime's. nanopb's generator itself runs unmodified (the
  one call it makes that Python 3 rejects, `zlib.crc32` on a `str`, is
  adapted in `run_plugin.py`). `Tools/python/nanopb_lua_generator.py` is
  ported to Python 3. Where its Python 2 version emitted dict or set
  iteration order, it reproduces CPython 2.7's order
  (`py2_table_order`), so the output doesn't change.
- `.pb.rb` (for the entity and data cookers) comes from the
  `ruby-protocol-buffers` gem's `protoc-gen-ruby`.
- Per-system `.yml` comes from `Tools/python/systems_scanner.py`, a
  Python replacement for `systems-scanner.exe` built on libclang's Python
  bindings, run by `Tools/ruby/boilerplate_scan.rb`.

The only differences from the Windows output are float defaults that
protoc formats through the C runtime: `1e+006` on Windows, `1e+06` on
Linux (`PhysicsStructs`), the same value.

A Linux game build should reach the engine through the game's `Usagi`
link (`USAGI_DIR=<game>/Usagi`), as on Windows, so game-side paths in the
generated code are spelled `Usagi/...`. The engine's own build recognises
itself through the link.

macOS hosts still need these outputs from Windows or Linux
(`rake platform=macos build=<b> codegen`); the generator stops with a
message naming that step if they're missing.

### Host requirements

- Linux: clang, ninja, Ruby, Python 3, pkg-config, the SDL3 and Vulkan
  loader development files, Vulkan headers and shaderc. Vulkan headers
  come from the distro package, or from `VULKAN_SDK` as on Windows.
- Everything else installs user-locally (no root) with
  `Tools/linux/bootstrap.sh`: the `ninja-gen`, `nokogiri`,
  `ruby-protocol-buffers` and `matrix` gems, protoc 2.5.0, and a Python
  venv with `protobuf` 3.20 and the `libclang` bindings. Then
  `. Tools/linux/usagi_env.sh` sets `USAGI_PROTOC`, `USAGI_PYTHON`,
  and `PATH`.
- Physics (Box3D) is vendored source, built with the engine; nothing to
  install.

## Shaders and data

Shaders are GLSL, compiled to SPIR-V by ShaderPackage at cook time, so
cooked effects are API-portable and run unchanged on the Linux Vulkan
driver. All cooked data (`_romfiles/win`) is little-endian and has no
OS-specific contents. Linux uses it as-is. Since the runtime opens
resources relative to the working directory, the game runs from the
romfiles directory, just as the Visual Studio debugger runs it from
`_romfiles\win`.

A Linux build therefore cooks the Windows data format, into
`_romfiles/win` (`BuildConfig#data_platform`), with the same rules as a
Windows build.

### Data tools

The tools in `Tools/Source` build natively with clang:
`Tools/Source/build_linux_tools.sh` puts fbx-to-vmdf, PakFileGen,
ShaderPackage and FontCreator in `_build/tools/linux/bin`, where the
generator looks for them on Linux (`BuildConfig#data_tool`). PakFileGen,
ShaderPackage and FontCreator are built from a small `build.ninja` and link
the engine's Linux libraries, so the engine is built first (`ENGINE_BUILD`,
release by default).

- `Tools/Source/ToolsPosix.h` is forced into the Linux builds only. It
  provides the Win32 and CRT calls the tools use (`fopen_s`,
  `sprintf_s`, `CreateDirectory`, `GetModuleFileName` and so on), so the
  sources keep their Windows spelling. Like the Debug runtime the Windows
  tools were built with, it fills the unused tail of fixed-size names
  with `0xFE`, which keeps the paks byte-identical.
- Compressonator (PakFileGen's texture compression) is fetched once at
  the submodule's pinned commit into `_build/tools/linux/src` when the
  submodule isn't checked out, and built with CMake.
- Models (FBX -> `.vmdf`, the `_build/skel` bone hierarchies and effect
  `.yml` -> `.cfx`) are converted by fbx-to-vmdf, the
  `Tools/Source/fbx-to-vmdf` submodule (https://github.com/jethac/fbx-to-vmdf).
  It replaced Ayataka: it reads FBX with ufbx, so neither the Autodesk FBX
  SDK nor libxml2 is needed on any platform. The script builds it with its
  own CMake (it doesn't link the engine) and copies the binary to
  `_build/tools/linux/bin/fbx-to-vmdf`; `USAGI_FBX_TO_VMDF` overrides the
  path the generator uses. Its output matches Ayataka's on every byte
  Ayataka initialised, apart from known, documented differences (see its
  README); it is byte-identical across hosts and compilers.
- On Windows, build fbx-to-vmdf with CMake and copy it to `Tools/bin`:

      cmake -S Tools\Source\fbx-to-vmdf -B _build\tools\win\fbx-to-vmdf -G "Visual Studio 17 2022" -A x64
      cmake --build _build\tools\win\fbx-to-vmdf --config Release --target fbx-to-vmdf
      copy _build\tools\win\fbx-to-vmdf\Release\fbx-to-vmdf.exe Tools\bin\

  (from a VS developer prompt, at the engine root).

Against the Windows cook of UsagiGP, the Linux tools give:

- PakFileGen: every texture pak (BC7 car paints included) and the audio
  pak are byte-identical.
- FontCreator: the font texture and `.vpb` are byte-identical.
- ShaderPackage: the SPIR-V is identical. Three effect paks differ in
  bytes that carry no data. Entries used to be sorted with a comparison
  that isn't a strict ordering, so their order depended on the C++
  library; they now follow a fixed dependency order (the game looks
  entries up by CRC). And uninitialised padding in attribute defaults,
  which the Windows Debug build left as `0xCC` or stack garbage, is now
  zeroed. (It also feeds the duplicate-entry check: left as garbage, a
  Linux build of `Model.pak` came out with 542 entries instead of 444.)


## Milestones

1. This plan.
2. `linux` platform in the build system.
3. Engine libraries compile and link on Linux (audio on `_dummy`).
4. A minimal app opens a Vulkan window via SDL3 and clears the screen.
5. UsagiGP builds and runs with Windows-cooked data; first fps
   measurement at 1920×1080.

## Status

All five milestones are done, and audio plays.

- The engine libraries build on Linux with clang 23 (debug and release)
  and still build on Windows with MSBuild.
- `Tools/Tests/LinuxClear` opens an SDL3/Wayland window on the GTX 1070
  and clears and presents at 1920×1080.
- UsagiGP (develop with the Cameraman, Effekseer, Wakuwaku and UXML
  modules) builds and renders correctly with `_romfiles/win` cooked on
  Windows: the scene, Effekseer, the UXML HUD and the debug overlays.
- The black 3D scene was Display_ps::TransferRect transitioning images
  from VK_IMAGE_LAYOUT_UNDEFINED, which allows drivers to discard their
  contents. NVIDIA's Windows driver keeps them; NVIDIA's Linux driver
  and RADV don't. Found by replaying a RenderDoc capture on the GTX 1070
  through renderdoccmd's remote server.
- Frame rate, release build, 1920×1080 fullscreen, mailbox present mode,
  150-second autopilot races (whole-run distribution, first 300 frames
  skipped):
  - couch mode: p50 7.30 ms, p99 8.58 ms, max 14.35 ms; GPU 3.9 ms
  - table mode with the VFX stress test (`USAGIGP_FXTEST`): p50 8.25 ms,
    p99 9.43 ms, max 12.52 ms; GPU 7.2 ms (the shadow cascade is 2.5 ms,
    GP_BenchWood alone 2.3 ms)
  - no frame over 16.7 ms in either mode
- Audio: the SDL3 backend (`Engine/Audio/_sdl`) plays the game's sounds,
  up to about 30 voices in a race. Reverb effects and rooms play dry.

RenderDoc on Linux: it doesn't support Wayland surfaces, so run the game
with `SDL_VIDEO_DRIVER=x11` (XWayland) under `renderdoccmd capture`.

### Building and running

Everything happens on the Linux machine. Once per machine:

    sh Tools/linux/bootstrap.sh             # gems, protoc 2.5, Python venv

Then, with the game checkout holding the engine as `Usagi` (a directory
or a link):

    . Usagi/Tools/linux/usagi_env.sh
    export USAGI_DIR=$PWD/Usagi
    rake platform=linux build=release ninja # generates the engine's and
                                            # the game's ninja files
    (cd Usagi && ninja -f _build/linux/release/build.ninja libs pb)
    (cd Usagi && sh Tools/Source/build_linux_tools.sh)  # the data tools
    ninja -f _build/linux/release/build.ninja  # codegen, the executable
                                               # and _romfiles/win
    cd _romfiles/win && ../../_rom/linux/release/UsagiGP [--fullscreen]

The game's `rake` only generates the engine's ninja file, so the engine
libraries are built first, and the data tools link them.
`Tools/Tests/LinuxClear/build.sh <build>` builds the bring-up test.

Add-on modules (Engine/<Name>) build like engine libraries. Modules that
need extra include paths or defines declare them in `usagi_module.yml`
under `ninja:` (`include_dirs`, `defines`). That's the counterpart of
`_msbuild/Module.props`.

Physics is Box3D, compiled from `Engine/ThirdParty/box3d` with the
ThirdParty library (C17; `wide8.c` gets AVX2 through a target pragma), so
there are no prebuilt physics libraries to build or point at.
