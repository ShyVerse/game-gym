# Game-Gym Engine

A custom C++20 3D game engine built on WebGPU, designed for VR-scale applications.
The default boot path now loads `project.ggym`, opens the startup scene, and runs attached scripts from disk.

## Features

| Module | Description |
|--------|-------------|
| **Rendering** | WebGPU (wgpu-native) with glTF 2.0 model loading, orbit camera, depth testing |
| **ECS** | Flecs-based entity component system with Transform, Velocity, RigidBody |
| **Physics** | Jolt Physics integration -- Box/Sphere/Capsule, raycasting, contact events |
| **GPU Compute** | WGSL compute shader dispatch with buffer management |
| **Scripting** | V8 JavaScript engine with TypeScript support, hot reload, per-script lifecycle |
| **Editor** | ImGui overlay with entity hierarchy and property panels |
| **MCP** | Model Context Protocol server for AI tool integration |
| **Codegen** | `GG_SCRIPTABLE` macros + libclang-based code generation for type conversions |

## Requirements

| Tool | Version |
|------|---------|
| C++ compiler | Clang 15+ or GCC 12+ (C++20) |
| Meson | 1.0+ |
| Ninja | 1.10+ |
| Python 3 | 3.8+ (for codegen) |

**Platform:** macOS (Metal), Linux (Vulkan). Windows support is planned.

## Quick Start

```bash
# Clone
git clone https://github.com/ShyVerse/game-gym.git
cd game-gym

# Setup dependencies (prebuilt binaries)
bash scripts/setup-wgpu.sh
bash scripts/setup-imgui.sh

# Build
meson setup builddir
meson compile -C builddir

# Run tests
meson test -C builddir

# Run the engine
./builddir/app/game-gym
```

By default, the engine looks for `project.ggym` in the repo root and boots `scenes/start.scene.json`.
The sample project included here shows a file-backed world with a startup mesh and TypeScript script.

## Project Boot

The minimal Unity-style boot flow is file-based:

- `project.ggym` points to the startup scene
- `scenes/*.scene.json` describes named entities and file references
- `assets/models/*.gltf` or `.glb` provide renderable geometry
- `assets/scripts/*.ts` are compiled and loaded on startup when referenced by the scene

## Coverage Gate

Coverage is enforced on every push and in GitHub Actions.

- Threshold file: `scripts/coverage-threshold.json`
- Local hook installer: `bash scripts/install-git-hooks.sh`
- Manual run: `python3 scripts/check_coverage.py --builddir builddir-coverage --summary-out builddir-coverage/coverage-summary.json`

The coverage gate uses a separate `builddir-coverage` build with Meson's `b_coverage=true` option and evaluates total line coverage with `gcovr`.

## Versioning

Version management is tag-based.

- Release tags use the format `vMAJOR.MINOR.PATCH`
- Runtime build metadata is derived from `git describe --tags`
- `./builddir/app/game-gym --version` prints the current build version
- Pushing a release tag creates a GitHub Release automatically

Examples:

```bash
# create an annotated release tag on HEAD
bash scripts/create_release_tag.sh v0.2.0

# publish the tag
git push origin v0.2.0
```

Sample files included in the repo:

- `project.ggym`
- `scenes/start.scene.json`
- `assets/models/start-pyramid.gltf`
- `assets/scripts/spin.ts`

## 3D Model Rendering

Load a glTF model exported from Blender in direct debug mode:

```bash
./builddir/app/game-gym --model assets/models/your_model.glb
```

- **Left-click drag** -- orbit camera rotation
- **Scroll** -- zoom in/out

## V8 Scripting (Optional)

Enable TypeScript scripting with the V8 engine:

```bash
# Download prebuilt V8
bash scripts/setup-v8.sh

# Build with scripting enabled
meson setup builddir --reconfigure -Denable_scripts=true
meson compile -C builddir

# Run with scripts
./builddir/app/game-gym
```

Scene-referenced scripts in `assets/scripts/` are automatically compiled and loaded. Example:

```typescript
function onInit(): void {
    world.createEntity("my_entity");
    world.setTransform("my_entity", {
        position: { x: 0, y: 1, z: 0 },
    });
}

function onUpdate(dt: number): void {
    // Called every frame
}

function onDestroy(): void {
    world.destroyEntity("my_entity");
}
```

TypeScript typings are provided in `typings/engine.d.ts` for IDE autocompletion.

## Codegen

When adding new types to the scripting layer, annotate C++ structs and run codegen:

```cpp
// In your header:
struct GG_SCRIPTABLE MyType {
    float value = 0.0f;
};
```

```bash
pip install -r scripts/requirements-codegen.txt
python scripts/codegen.py
```

This generates:
- `generated/script_types_gen.h` -- C++ to JSON conversion
- `generated/engine_gen.d.ts` -- TypeScript type definitions
- `generated/codegen_bindings_check.h` -- link-time binding guards

## Project Layout

```
engine/
  core/           Window, Engine (main loop)
  renderer/       GpuContext, Renderer, MeshRenderer, Camera, GltfLoader
  ecs/            Flecs World, components (Vec3, Quat, Transform, Velocity)
  physics/        Jolt PhysicsWorld, collision shapes, raycasting
  compute/        GPU compute pipeline, buffer management
  editor/         ImGui panels (hierarchy, properties)
  script/         V8 ScriptEngine, bindings, hot-reload manager
  math/           Mat4 utility (perspective, look_at)
  mcp/            MCP server, JSON-RPC tools

generated/        Auto-generated type conversions and TypeScript types
typings/          TypeScript API declarations
shaders/          WGSL shaders (triangle, mesh, particle_sim)
scripts/          Build setup scripts, codegen tool
third_party/      cgltf, glfw3webgpu
tests/            Google Test suite
```

## CI

GitHub Actions runs on every push and PR:

| Job | Description |
|-----|-------------|
| **Format Check** | clang-format compliance |
| **Build & Test** | Meson build + full test suite |
| **Clang-Tidy** | Static analysis (bugprone, modernize, performance) |
| **Codegen Freshness** | Verifies generated files are up to date |
| **Release** | Publishes a GitHub Release when a `v*.*.*` tag is pushed |

## License

[MIT](LICENSE)
