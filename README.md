# Game-Gym Engine

A minimal C++20 game engine demo built on WebGPU (wgpu-native v24) and GLFW.
Renders a coloured triangle using a Metal backend on macOS.

## Requirements

| Tool | Version |
|------|---------|
| Meson | 1.0+ |
| Ninja | 1.10+ |
| C++ compiler | Clang 15+ or GCC 12+ (C++20) |
| GLFW | 3.3+ (via system or Homebrew) |

macOS only for now (Metal backend).  The build system detects the host OS
and links the appropriate platform frameworks automatically.

## Build

```sh
# Configure (first time)
meson setup builddir

# Compile
meson compile -C builddir

# Run tests
meson test -C builddir -v
```

## Run

```sh
# From the project root so the shader path resolves correctly
./builddir/app/game-gym
```

The app reads `shaders/triangle.wgsl` relative to the current working
directory.  Pass an alternative path as the first argument:

```sh
./builddir/app/game-gym /path/to/triangle.wgsl
```

## Project Layout

```
engine/
  core/       window.h / window.cpp      GLFW-backed Window class
  renderer/   gpu_context.h/.cpp         wgpu instance → surface → adapter → device
              renderer.h/.cpp            render pipeline + per-frame draw loop
app/
  main.cpp                               entry point
shaders/
  triangle.wgsl                          WGSL vertex + fragment shader
tests/
  test_window.cpp                        Window unit tests
  test_gpu_context.cpp                   GpuContext integration tests
wgpu-native/                             pre-built wgpu-native v24 headers + dylib
third_party/glfw3webgpu/                 helper to create a WGPUSurface from a GLFWwindow
```

## Architecture Notes

- **wgpu-native v24** — uses the new callback-info structs
  (`WGPURequestAdapterCallbackInfo`, `WGPURequestDeviceCallbackInfo`) and
  `WGPUStringView` for all labels and string parameters.
- **AllowSpontaneous** callback mode is used so that adapter/device requests
  resolve synchronously on wgpu-native (no event-loop polling required).
- The `GpuContext` encapsulates the entire initialization chain and exposes
  only the handles needed by higher-level code.
- The `Renderer` is stateless between frames; per-frame objects (texture
  view, command encoder, render pass) are created and released each frame.
