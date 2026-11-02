# leaf-framework

leaf-framework is a C++23 framework for application loops, RML scene support, Lua scripting hooks, mod/prototype loading, localization, audio, and Rutile graphics integration.

This directory is also a standalone CMake project, so Leaf can be built independently from the game.

## Kiln configuration

A parent project passes ordinary Kiln options into Leaf's configuration:

```rutile
kiln::Options leaf_options = options;
@leaf_options.set("storefront", "steam");
kiln::add_subdirectory("leaf-framework", leaf_options);
```

Leaf accepts `storefront=none` (the default) or `storefront=steam`. Steam requires
`STEAMWORKS_SDK_DIR`; Leaf declares its SDK headers, library, and runtime files.
The `cxx` and `linker` choices select the installed compiler and linker, initially MSVC
`cl` and `link`. Shared runtime and optimization policy comes from Kiln profiles.

Leaf exports its public headers and required dependency headers through target usage.
Its `modules` directory remains private. Leaf also owns application shader compilation
and the runtime files under `leaf/application`.

## Layout

- `include/leaf` contains the public headers and `modules` contains the framework implementation. Modules include `application`, `audio`, `core`, `framework`, `graphics`, `lockstep`, `math`, `network`, `platform`, `pmg`, `resource`, `script`, `store`, and `system`.
- `RutileGraphics` is the graphics API used by Leaf (see [RutileGraphics/README.md](RutileGraphics/README.md)).
- `tests` contains the CTest suite. Its targets and CTest registration are always configured; `leaf-tests` is excluded from the default build.

## How To Compile

### Windows

These commands assume Visual Studio 2022 with the C++ workload, CMake, Ninja, Git, and PowerShell.

Install the Vulkan SDK from LunarG if you want to build or run the Vulkan backend. The vcpkg command below installs Vulkan headers and loader libraries, but the SDK provides the local Vulkan development/runtime environment expected by Rutile's Vulkan backend.

Install vcpkg:

```powershell
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
```

Install all dependencies with one command:

```powershell
C:\vcpkg\vcpkg.exe install --triplet x64-windows-static
```

Configure and build Leaf:

```powershell
cmake -S . -B out/build/x64-Debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static
cmake --build out/build/x64-Debug --target leaf-framework
```

Build the test executable, then run the CTest suite:

```powershell
cmake --build out/build/x64-Debug --target leaf-tests
ctest --test-dir out/build/x64-Debug --output-on-failure
```

When leaf-framework is included from a parent CMake project, it is built through the parent target and does not need a separate configure step.

### Linux

Install a compiler, CMake, Ninja, Git, pkg-config, X11/Wayland development packages for GLFW, PulseAudio/ALSA development packages for miniaudio, and the Vulkan SDK or distribution Vulkan development packages.

On Ubuntu/Debian, the system package baseline is:

```bash
sudo apt update
sudo apt install build-essential cmake ninja-build git pkg-config zip unzip tar \
  libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev \
  libwayland-dev wayland-protocols libxkbcommon-dev libpulse-dev libasound2-dev \
  vulkan-tools libvulkan-dev
```

Install vcpkg:

```bash
git clone https://github.com/microsoft/vcpkg.git ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh
```

Install all dependencies with one command:

```bash
~/vcpkg/vcpkg install --triplet x64-linux
```

Configure and build Leaf:

```bash
cmake -S . -B out/build/linux-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-linux
cmake --build out/build/linux-debug --target leaf-framework
```

Build the test executable, then run the CTest suite:

```bash
cmake --build out/build/linux-debug --target leaf-tests
ctest --test-dir out/build/linux-debug --output-on-failure
```

## Notes

Leaf's audio module fetches a pinned miniaudio header during configuration; see the sound API below.

Rutile's Vulkan backend requires the Vulkan SDK, Vulkan headers, the Vulkan loader, SPIRV-Cross, and Vulkan Memory Allocator. The DirectX 12 backend uses the Windows SDK that ships with Visual Studio.

## Sound prototypes and RML Lua

Leaf downloads the pinned miniaudio 0.11.23 header during CMake configuration. Its URL and
SHA-256 are fixed in `modules/audio/CMakeLists.txt`. Playback decodes WAV, FLAC, and MP3
samples read through Leaf's virtual filesystem, including mapped mod directories and packages.

`lf::Init` registers `sound-group` and `sound` with the standard mod prototype loader:

```lua
data:extend({
    { type = "sound-group", name = "interface", volume = 0.8 },
    {
        type = "sound", name = "key-down", sound_group = "interface",
        path = "/my-mod/sounds/key-down.wav", volume = 0.6,
    },
})
```

`path` is required. `sound_group` is optional. Both prototype volumes default to 1 and
must be finite values between 0 and 1. Unknown sound-group references are rejected.

Every Scene owns an audio engine and installs this API into its RML Lua state:

```lua
local sound = prototypes.sound["key-down"]
local group = prototypes["sound-group"]["interface"]
audio.play(sound.name, 0.7) -- optional per-play volume, default 1
audio.set_volume(0.5)       -- scene master volume
audio.get_volume()
audio.stop_all()
audio.available()         -- opens output lazily; false when no device is available
audio.refresh_prototypes() -- refresh exports after loading/reloading mods
```

For example, an RML element can use `mousedown="audio.play('key-down', 0.7)"`.
Prototype exports are snapshots of the standard Database, indexed by name and ordinal.
Effective gain multiplies master, sound-group, sound, and per-play volumes. Runtime volume
arguments clamp to [0, 1]; nonfinite arguments produce an error. Unknown sounds, unreadable
files, and decoding failures produce descriptive Lua errors. Device absence disables playback
gracefully and logs one warning. Each call creates an independent overlapping voice; Scene
updates reclaim finished voices, and Scene destruction stops playback and releases the engine.

C++ callers use `scene.audio()` or a standalone `lf::Audio` from `leaf/audio/audio.hpp`.
Link `leaf::audio` when using it without `leaf::application`. `audio_output::offline` provides
stereo 48 kHz float mixing through `render`, without opening a device. The returned count is
produced frames; any unused output space is silence. The `leaf-audio-tests` target verifies
real decoded samples, overlapping playback, gain multiplication, cleanup, and Lua access.
