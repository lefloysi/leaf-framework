# leaf-framework

leaf-framework is a C++23 framework for application loops, RML scene support, Lua scripting hooks, mod/prototype loading, localization, audio, and Rutile graphics integration.

This directory is also a standalone CMake project, so Leaf can be built independently from the game.

## Layout

- `include/leaf` contains the public headers and `modules` contains the framework implementation. Modules include `application`, `audio`, `core`, `framework`, `graphics`, `lockstep`, `math`, `network`, `platform`, `pmg`, `resource`, `script`, `store`, and `system`.
- `Rutile` is the graphics API used by Leaf (see [Rutile/README.md](Rutile/README.md)).
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

Leaf's audio and application modules locate `miniaudio.h` with CMake's `find_path`; provide a dependency installation that makes that header discoverable to CMake.

Rutile's Vulkan backend requires the Vulkan SDK, Vulkan headers, the Vulkan loader, SPIRV-Cross, and Vulkan Memory Allocator. The DirectX 12 backend uses the Windows SDK that ships with Visual Studio.
