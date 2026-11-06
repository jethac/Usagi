# Native macOS and iOS Ports

This note is the plan for running Usagi games natively on macOS (arm64)
and iOS (arm64), through MoltenVK. It builds on the Linux port
(`LinuxPort.md`) and the shared platform layers it was split into.

## Ground rules

- `macos` and `ios` are target platforms alongside `win` and `linux`.
  The preprocessor symbols are the engine's existing `PLATFORM_OSX` and
  `PLATFORM_IOS`; both also get `PLATFORM_POSIX`, `USE_SDL` and
  `USE_VULKAN` from `Engine/Common/_posix/Common_ps.h`.
- Platform code is shared, not copied: `_posix` for OS services, `_sdl`
  for the window, input and audio, `_vulkan` for graphics. The few
  Darwin differences are `#ifdef __APPLE__` inside `_posix`.
  `Engine/Common/_osx` and `_ios` hold only each platform's
  `Common_ps.h`. (The old OpenGL-era `_osx` files were removed.)
- **Data is cooked on Windows** and shipped as is (`_romfiles/win`), as
  on Linux. Generated code comes from `rake platform=<p> codegen` on
  Windows.
- Performance is a deliverable: 60 fps minimum on every target, 120 on
  ProMotion displays where it's cheap, sustained over several minutes.

## Vulkan on Metal: MoltenVK

- The instance enables `VK_KHR_portability_enumeration` with
  `VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR`, and the device
  enables `VK_KHR_portability_subset`, whenever they are offered. Other
  platforms are unaffected.
- macOS links the LunarG SDK's loader (`VULKAN_SDK`), which finds
  MoltenVK through its ICD manifest; the app bundle carries the loader,
  MoltenVK and the manifest. iOS links MoltenVK statically.
- **Geometry shaders:** Metal has none, so MoltenVK reports
  `geometryShader = false`. The engine's point-sprite effects (text, UI,
  UXML boxes, debug text, particles) expand points to quads in a
  geometry shader. They move to vertex-shader expansion with instancing
  (one instance per sprite, four vertices each), which also suits the
  tile-based GPUs in phones better. Omni shadow cubemaps
  (`depthwriteomni`) need layered rendering without a geometry shader
  (one pass per face) or stay disabled on these platforms.
- **Texture formats:** cooked textures are BC. Apple silicon Macs sample
  BC; iPhones may not (MoltenVK reports `textureCompressionBC`). If not,
  the data is cooked to ASTC for iOS, still on Windows.

## Window, input, audio: SDL3

Unchanged from Linux: `SDLUTIL` creates the window with
`SDL_WINDOW_VULKAN | SDL_WINDOW_HIGH_PIXEL_DENSITY` and
`SDL_Vulkan_CreateSurface` makes a Metal-layer surface. Audio is the
`_sdl` backend. The entry point is the game's `main/_sdl/SDLMain.cpp`,
which includes `SDL_main.h` so SDL owns the real entry point on iOS.
`SDLUTIL::UsePackagedData()` makes a bundle's `romfiles` directory the
working directory.

iOS adds:

- **Touch:** SDL finger events, mapped to gameplay. Table mode's phone
  controls map naturally to touch; couch mode gets a simple on-screen
  scheme.
- **Lifecycle:** `SDL_EVENT_WILL_ENTER_BACKGROUND` /
  `DID_ENTER_FOREGROUND` pause and resume the game, stop presenting while
  in the background, and recreate the swap chain on surface loss.

## Build system

`Tools/build/platform.macos.rb` (`PlatformMacOS`, derived from
`PlatformLinux`) compiles and links with Apple clang from the ninja file:
arm64, `MACOS_MIN_VERSION` (default 13.0), SDL3 through pkg-config.
Physics (Box3D) builds from source with the engine, using its NEON path.
ld64 needs no `--start-group`.
iOS follows as `PlatformIOS` (device and simulator SDKs), producing a
signed `.app` for the simulator without Xcode and an Xcode project for
device signing and debugging.

### Host requirements (Mac)

Xcode (command line tools for macOS, full Xcode for iOS), Homebrew
(`ninja`, `pkg-config`, `sdl3`, `cmake`, Ruby with the `ninja-gen` gem),
and the LunarG Vulkan SDK for macOS.

## Milestones

1. Shared SDL and POSIX platform layers (done; Linux and Windows
   unchanged).
2. `macos` platform in the build system; portability extensions.
3. Point sprites without geometry shaders.
4. UsagiGP runs on macOS from an app bundle; frame-time distribution and
   GPU profile at the display's resolution, sustained.
5. iOS simulator, then device: touch, lifecycle, thermals.
