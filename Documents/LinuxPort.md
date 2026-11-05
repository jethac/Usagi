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
- **Data and generated code are produced on Windows** and copied over.
  The engine's tools (Ayataka, ShaderPackage, PakFileGen, FontCreator,
  `systems-scanner`, `protoc` 2.5 with the nanopb plugin batch files) are
  Windows binaries. Porting them is out of scope for the first pass.

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

Some generated code depends on Windows-only tools:

- `.pb.h/.pb.cpp` and `.lua.h/.lua.cpp` from `protoc.exe` 2.5 with the
  nanopb plugin `.bat` wrappers
- per-system `.yml` from `systems-scanner.exe`, a libclang tool whose
  source is not in this repo

These are produced on Windows with `rake platform=linux codegen` and
copied over with the data. When the generator runs on a Linux host it
treats those files as inputs and leaves out their rules. Everything else
(header copies into `_includes`, `BuildID.h`, YAML combining, and the
ERB boilerplate templates) is plain Ruby and runs on Linux.

The generator checks that the Windows-produced files exist and stops
with a message naming the missing step if they don't.

Making codegen native later means two changes. First, run the nanopb
generator under Python 3 against the system `protoc`. Second, build
`systems-scanner` against the system libclang, or replace it.

### Host requirements

- Linux: clang, ninja, Ruby with the `ninja-gen` gem, pkg-config, the
  SDL3 and Vulkan loader development files, and Vulkan headers. Ruby
  gems install with `--user-install`. Vulkan headers come from the
  distro package, or from `VULKAN_SDK` as on Windows.
- PhysX 4.1.2 is built from the submodule with its `linux` preset
  (cmake). `PHYSX_DIR` points at it, as on Windows.

## Shaders and data

Shaders are GLSL, compiled to SPIR-V by ShaderPackage at cook time, so
cooked effects are API-portable and run unchanged on the Linux Vulkan
driver. All cooked data (`_romfiles/win`) is little-endian and has no
OS-specific contents. Linux uses it as-is. Since the runtime opens
resources relative to the working directory, the game runs from the
copied romfiles directory, just as the Visual Studio debugger runs it
from `_romfiles\win`.

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
- UsagiGP (develop with the Cameraman, Effekseer, Livingroom and UXML
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

On Windows (pinned toolchain), in the engine checkout, then in the game:

    rake platform=linux build=debug codegen

Copy the tree, `_build/proto`, `_build/linux/<build>` and
`_romfiles/win` to the Linux machine. Then, with `USAGI_DIR`,
`VULKAN_SDK` (a prefix containing `include/vulkan`) and `PHYSX_DIR` set:

    rake platform=linux build=debug         # engine: generate + build libs
    Tools/Tests/LinuxClear/build.sh debug   # bring-up test
    rake platform=linux build=debug         # in the game directory
                                            # (after the engine libs: the
                                            # game rake only regenerates
                                            # the engine's ninja file)
    cd _romfiles/win && ../../_rom/linux/debug/UsagiGP [--fullscreen]

Add-on modules (Engine/<Name>) build like engine libraries. Modules that
need extra include paths or defines declare them in `usagi_module.yml`
under `ninja:` (`include_dirs`, `defines`). That's the counterpart of
`_msbuild/Module.props`.

PhysX 4.1.2 for Linux: generate with the `linux` preset (snippets and
samples off, static libraries), using CMake 3.x. Its CMake flags
include `-Werror`, which current clang fails, so remove it.
