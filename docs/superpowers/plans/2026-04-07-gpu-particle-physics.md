# GPU Particle Physics Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** GPU 컴퓨트 셰이더로 최대 100K 파티클의 물리 시뮬레이션 — 중력, 환경 충돌, 파티클 간 충돌(spatial hash).

**Architecture:** 3-pass 컴퓨트 파이프라인 (integrate → grid build → collide). ParticleSystem 클래스가 GPU 버퍼와 파이프라인을 관리하고, step()에서 순차 디스패치. 기존 GpuBuffer/ComputePipeline 인프라 위에 구축.

**Tech Stack:** C++20, WGSL compute shaders, wgpu-native, GoogleTest

**Spec:** `docs/superpowers/specs/2026-04-07-gpu-particle-physics-design.md`

---

## File Map

| Action | Path | Responsibility |
|--------|------|----------------|
| Create | `engine/compute/particle_system.h` | ParticleSystem 클래스 선언 |
| Create | `engine/compute/particle_system.cpp` | ParticleSystem 구현 |
| Create | `shaders/particle_integrate.wgsl` | Pass 1: 속도 적분 + 환경 충돌 |
| Create | `shaders/particle_grid_build.wgsl` | Pass 2: spatial hash 그리드 구축 |
| Create | `shaders/particle_collide.wgsl` | Pass 3: 파티클 간 충돌 |
| Create | `tests/test_gpu_particle.cpp` | ParticleSystem 테스트 |
| Modify | `engine/meson.build` | particle_system.cpp 소스 추가 |
| Modify | `tests/meson.build` | test_gpu_particle 등록 |

---

### Task 1: Pass 1 셰이더 — 속도 적분 + 환경 충돌

**Files:**
- Create: `shaders/particle_integrate.wgsl`
- Create: `tests/test_gpu_particle.cpp`
- Modify: `tests/meson.build`

- [ ] **Step 1: 셰이더 작성**

`shaders/particle_integrate.wgsl`:

```wgsl
struct Particle {
    pos: vec4f,  // xyz = position, w = radius
    vel: vec4f,  // xyz = velocity, w = inverse_mass
};

struct SimParams {
    dt: f32,
    gravity: f32,
    count: u32,
    grid_res: u32,
    bounds_min: vec3f,
    _pad0: f32,
    bounds_max: vec3f,
    damping: f32,
    cell_size: f32,
    _pad1: f32,
    _pad2: f32,
    _pad3: f32,
};

@group(0) @binding(0) var<storage, read_write> particles: array<Particle>;
@group(0) @binding(1) var<uniform> params: SimParams;

@compute @workgroup_size(64)
fn main(@builtin(global_invocation_id) gid: vec3u) {
    let idx = gid.x;
    if (idx >= params.count) { return; }

    var p = particles[idx];

    // Apply gravity
    p.vel.y = p.vel.y + params.gravity * params.dt;

    // Integrate position
    p.pos = vec4f(p.pos.xyz + p.vel.xyz * params.dt, p.pos.w);

    // Boundary collision (reflect + damp)
    let radius = p.pos.w;
    for (var axis = 0u; axis < 3u; axis = axis + 1u) {
        if (p.pos[axis] - radius < params.bounds_min[axis]) {
            p.pos[axis] = params.bounds_min[axis] + radius;
            p.vel[axis] = -p.vel[axis] * params.damping;
        } else if (p.pos[axis] + radius > params.bounds_max[axis]) {
            p.pos[axis] = params.bounds_max[axis] - radius;
            p.vel[axis] = -p.vel[axis] * params.damping;
        }
    }

    particles[idx] = p;
}
```

- [ ] **Step 2: C++ 데이터 레이아웃 정의 및 테스트 파일 작성**

`tests/test_gpu_particle.cpp`:

```cpp
#include "compute/compute_pipeline.h"
#include "compute/gpu_buffer.h"
#include "core/window.h"
#include "renderer/gpu_context.h"

#include <cmath>
#include <cstring>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
#include <vector>

// Must match WGSL struct layout exactly
struct GpuParticle {
    float pos[4]; // xyz = position, w = radius
    float vel[4]; // xyz = velocity, w = inverse_mass
};

struct SimParams {
    float dt;
    float gravity;
    uint32_t count;
    uint32_t grid_res;
    float bounds_min[3];
    float _pad0;
    float bounds_max[3];
    float damping;
    float cell_size;
    float _pad1;
    float _pad2;
    float _pad3;
};
static_assert(sizeof(SimParams) == 64, "SimParams must be 64 bytes");

namespace {

std::unique_ptr<gg::Window> make_window() {
    return gg::Window::create({.title = "particle-test", .width = 64, .height = 64});
}

std::string read_file(const char* path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        throw std::runtime_error(std::string("Cannot open: ") + path);
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

SimParams make_default_params(uint32_t count) {
    SimParams p{};
    p.dt = 1.0f / 60.0f;
    p.gravity = -9.81f;
    p.count = count;
    p.grid_res = 64;
    p.bounds_min[0] = -10.0f;
    p.bounds_min[1] = 0.0f;
    p.bounds_min[2] = -10.0f;
    p.bounds_max[0] = 10.0f;
    p.bounds_max[1] = 20.0f;
    p.bounds_max[2] = 10.0f;
    p.damping = 0.8f;
    p.cell_size = (20.0f) / 64.0f;
    return p;
}

uint32_t workgroups(uint32_t count) {
    return (count + 63) / 64;
}

} // namespace

// ---------------------------------------------------------------------------
// Pass 1: Integration + Boundary
// ---------------------------------------------------------------------------

TEST(GpuParticleTest, GravityFall) {
    auto window = make_window();
    auto ctx = gg::GpuContext::create(*window);
    auto shader = read_file("shaders/particle_integrate.wgsl");

    constexpr uint32_t kCount = 1;
    GpuParticle p{};
    p.pos[0] = 0.0f;
    p.pos[1] = 10.0f;
    p.pos[2] = 0.0f;
    p.pos[3] = 0.05f; // radius
    p.vel[0] = 0.0f;
    p.vel[1] = 0.0f;
    p.vel[2] = 0.0f;
    p.vel[3] = 1.0f; // inverse_mass

    auto params = make_default_params(kCount);

    gg::GpuBuffer pbuf = gg::GpuBuffer::create_storage(ctx->device(), sizeof(GpuParticle));
    pbuf.upload(ctx->queue(), &p, sizeof(GpuParticle));
    gg::GpuBuffer ubuf = gg::GpuBuffer::create_uniform(ctx->device(), sizeof(SimParams));
    ubuf.upload(ctx->queue(), &params, sizeof(SimParams));

    auto pipeline = gg::ComputePipeline::create(ctx->device(), shader);

    // Run 10 steps
    for (int i = 0; i < 10; ++i) {
        pipeline->dispatch(ctx->device(), ctx->queue(),
                           {pbuf.handle(), ubuf.handle()}, workgroups(kCount));
    }

    auto result = pbuf.readback(ctx->device(), ctx->queue());
    const auto* out = reinterpret_cast<const GpuParticle*>(result.data());

    // After 10 frames of gravity, y should decrease
    EXPECT_LT(out->pos[1], 10.0f) << "Particle should fall under gravity";
    EXPECT_LT(out->vel[1], 0.0f) << "Velocity should be negative (falling)";
}

TEST(GpuParticleTest, BoundaryBounce) {
    auto window = make_window();
    auto ctx = gg::GpuContext::create(*window);
    auto shader = read_file("shaders/particle_integrate.wgsl");

    constexpr uint32_t kCount = 1;
    GpuParticle p{};
    p.pos[0] = 0.0f;
    p.pos[1] = 0.5f; // near floor (bounds_min.y = 0)
    p.pos[2] = 0.0f;
    p.pos[3] = 0.05f;
    p.vel[0] = 0.0f;
    p.vel[1] = -20.0f; // fast downward
    p.vel[2] = 0.0f;
    p.vel[3] = 1.0f;

    auto params = make_default_params(kCount);
    params.gravity = 0.0f; // disable gravity to isolate bounce

    gg::GpuBuffer pbuf = gg::GpuBuffer::create_storage(ctx->device(), sizeof(GpuParticle));
    pbuf.upload(ctx->queue(), &p, sizeof(GpuParticle));
    gg::GpuBuffer ubuf = gg::GpuBuffer::create_uniform(ctx->device(), sizeof(SimParams));
    ubuf.upload(ctx->queue(), &params, sizeof(SimParams));

    auto pipeline = gg::ComputePipeline::create(ctx->device(), shader);
    pipeline->dispatch(ctx->device(), ctx->queue(),
                       {pbuf.handle(), ubuf.handle()}, workgroups(kCount));

    auto result = pbuf.readback(ctx->device(), ctx->queue());
    const auto* out = reinterpret_cast<const GpuParticle*>(result.data());

    // Should have bounced: pos.y >= bounds_min + radius, vel.y > 0
    EXPECT_GE(out->pos[1], 0.05f) << "Particle must stay inside boundary";
    EXPECT_GT(out->vel[1], 0.0f) << "Velocity should flip after bounce";
}

TEST(GpuParticleTest, WallBounce) {
    auto window = make_window();
    auto ctx = gg::GpuContext::create(*window);
    auto shader = read_file("shaders/particle_integrate.wgsl");

    constexpr uint32_t kCount = 1;
    GpuParticle p{};
    p.pos[0] = 9.9f; // near right wall (bounds_max.x = 10)
    p.pos[1] = 5.0f;
    p.pos[2] = 0.0f;
    p.pos[3] = 0.05f;
    p.vel[0] = 50.0f; // fast rightward
    p.vel[1] = 0.0f;
    p.vel[2] = 0.0f;
    p.vel[3] = 1.0f;

    auto params = make_default_params(kCount);
    params.gravity = 0.0f;

    gg::GpuBuffer pbuf = gg::GpuBuffer::create_storage(ctx->device(), sizeof(GpuParticle));
    pbuf.upload(ctx->queue(), &p, sizeof(GpuParticle));
    gg::GpuBuffer ubuf = gg::GpuBuffer::create_uniform(ctx->device(), sizeof(SimParams));
    ubuf.upload(ctx->queue(), &params, sizeof(SimParams));

    auto pipeline = gg::ComputePipeline::create(ctx->device(), shader);
    pipeline->dispatch(ctx->device(), ctx->queue(),
                       {pbuf.handle(), ubuf.handle()}, workgroups(kCount));

    auto result = pbuf.readback(ctx->device(), ctx->queue());
    const auto* out = reinterpret_cast<const GpuParticle*>(result.data());

    EXPECT_LE(out->pos[0], 10.0f - 0.05f) << "Particle must stay inside wall";
    EXPECT_LT(out->vel[0], 0.0f) << "Velocity should flip after wall bounce";
}
```

- [ ] **Step 3: tests/meson.build에 등록**

`tests/meson.build`에서 `test('shader_utils', test_shader_utils)` 뒤에 추가:

```meson
  test_gpu_particle = executable('test_gpu_particle',
    'test_gpu_particle.cpp',
    dependencies: [engine_dep, gtest_dep, gtest_main_dep],
  )
  test('gpu_particle', test_gpu_particle,
    workdir: meson.source_root(),
  )
```

- [ ] **Step 4: 빌드 및 테스트**

```bash
meson compile -C builddir test_gpu_particle
./builddir/tests/test_gpu_particle
```

Expected: GravityFall, BoundaryBounce, WallBounce 3개 PASS.

- [ ] **Step 5: Commit**

```bash
git add shaders/particle_integrate.wgsl tests/test_gpu_particle.cpp tests/meson.build
git commit -m "feat: add particle integrate shader with gravity and boundary collision"
```

---

### Task 2: Pass 2 셰이더 — Spatial Hash 그리드 구축

**Files:**
- Create: `shaders/particle_grid_build.wgsl`
- Modify: `tests/test_gpu_particle.cpp`

- [ ] **Step 1: 그리드 셰이더 작성**

`shaders/particle_grid_build.wgsl`:

```wgsl
struct Particle {
    pos: vec4f,
    vel: vec4f,
};

struct SimParams {
    dt: f32,
    gravity: f32,
    count: u32,
    grid_res: u32,
    bounds_min: vec3f,
    _pad0: f32,
    bounds_max: vec3f,
    damping: f32,
    cell_size: f32,
    _pad1: f32,
    _pad2: f32,
    _pad3: f32,
};

const MAX_PER_CELL: u32 = 8u;

@group(0) @binding(0) var<storage, read> particles: array<Particle>;
@group(0) @binding(1) var<uniform> params: SimParams;
@group(0) @binding(2) var<storage, read_write> grid_counts: array<atomic<u32>>;
@group(0) @binding(3) var<storage, read_write> grid_entries: array<u32>;

fn pos_to_cell(pos: vec3f) -> u32 {
    let rel = (pos - params.bounds_min) / params.cell_size;
    let cx = clamp(u32(rel.x), 0u, params.grid_res - 1u);
    let cy = clamp(u32(rel.y), 0u, params.grid_res - 1u);
    let cz = clamp(u32(rel.z), 0u, params.grid_res - 1u);
    return cx + cy * params.grid_res + cz * params.grid_res * params.grid_res;
}

@compute @workgroup_size(64)
fn clear(@builtin(global_invocation_id) gid: vec3u) {
    let idx = gid.x;
    let total_cells = params.grid_res * params.grid_res * params.grid_res;
    if (idx >= total_cells) { return; }
    atomicStore(&grid_counts[idx], 0u);
}

@compute @workgroup_size(64)
fn insert(@builtin(global_invocation_id) gid: vec3u) {
    let idx = gid.x;
    if (idx >= params.count) { return; }

    let cell = pos_to_cell(particles[idx].pos.xyz);
    let slot = atomicAdd(&grid_counts[cell], 1u);
    if (slot < MAX_PER_CELL) {
        grid_entries[cell * MAX_PER_CELL + slot] = idx;
    }
}
```

- [ ] **Step 2: 그리드 테스트 추가**

`tests/test_gpu_particle.cpp`에 추가:

```cpp
// ---------------------------------------------------------------------------
// Pass 2: Grid Build
// ---------------------------------------------------------------------------

TEST(GpuParticleTest, GridBuildCorrectness) {
    auto window = make_window();
    auto ctx = gg::GpuContext::create(*window);

    auto clear_shader = read_file("shaders/particle_grid_build.wgsl");
    auto insert_shader = clear_shader; // same file, different entry points

    constexpr uint32_t kCount = 4;
    constexpr uint32_t kGridRes = 4;
    constexpr uint32_t kTotalCells = kGridRes * kGridRes * kGridRes; // 64
    constexpr uint32_t kMaxPerCell = 8;

    // Place 4 particles in known positions within a small grid
    std::vector<GpuParticle> particles(kCount);
    // All in cell (0,0,0)
    for (auto& p : particles) {
        p.pos[0] = -9.0f;
        p.pos[1] = 0.5f;
        p.pos[2] = -9.0f;
        p.pos[3] = 0.05f;
        p.vel[0] = 0.0f;
        p.vel[1] = 0.0f;
        p.vel[2] = 0.0f;
        p.vel[3] = 1.0f;
    }

    SimParams params{};
    params.count = kCount;
    params.grid_res = kGridRes;
    params.bounds_min[0] = -10.0f;
    params.bounds_min[1] = 0.0f;
    params.bounds_min[2] = -10.0f;
    params.bounds_max[0] = 10.0f;
    params.bounds_max[1] = 20.0f;
    params.bounds_max[2] = 10.0f;
    params.cell_size = 20.0f / static_cast<float>(kGridRes); // 5.0

    uint64_t pbytes = kCount * sizeof(GpuParticle);
    gg::GpuBuffer pbuf = gg::GpuBuffer::create_storage(ctx->device(), pbytes);
    pbuf.upload(ctx->queue(), particles.data(), pbytes);

    gg::GpuBuffer ubuf = gg::GpuBuffer::create_uniform(ctx->device(), sizeof(SimParams));
    ubuf.upload(ctx->queue(), &params, sizeof(SimParams));

    uint64_t counts_bytes = kTotalCells * sizeof(uint32_t);
    gg::GpuBuffer counts_buf = gg::GpuBuffer::create_storage(ctx->device(), counts_bytes);

    uint64_t entries_bytes = kTotalCells * kMaxPerCell * sizeof(uint32_t);
    gg::GpuBuffer entries_buf = gg::GpuBuffer::create_storage(ctx->device(), entries_bytes);

    // Clear pass
    auto clear_pipeline = gg::ComputePipeline::create(ctx->device(), clear_shader, "clear");
    clear_pipeline->dispatch(ctx->device(), ctx->queue(),
                             {pbuf.handle(), ubuf.handle(),
                              counts_buf.handle(), entries_buf.handle()},
                             workgroups(kTotalCells));

    // Insert pass
    auto insert_pipeline = gg::ComputePipeline::create(ctx->device(), insert_shader, "insert");
    insert_pipeline->dispatch(ctx->device(), ctx->queue(),
                              {pbuf.handle(), ubuf.handle(),
                               counts_buf.handle(), entries_buf.handle()},
                              workgroups(kCount));

    // Readback counts
    auto counts_data = counts_buf.readback(ctx->device(), ctx->queue());
    const auto* counts = reinterpret_cast<const uint32_t*>(counts_data.data());

    // Cell (0,0,0) = index 0 should have 4 particles
    // pos (-9, 0.5, -9) → rel = (1, 0.5, 1)/5 = (0.2, 0.1, 0.2) → cell (0,0,0) = 0
    EXPECT_EQ(counts[0], 4u) << "Cell (0,0,0) should contain all 4 particles";

    // All other cells should be 0
    for (uint32_t i = 1; i < kTotalCells; ++i) {
        EXPECT_EQ(counts[i], 0u) << "Cell " << i << " should be empty";
    }
}
```

- [ ] **Step 3: 빌드 및 테스트**

```bash
meson compile -C builddir test_gpu_particle
./builddir/tests/test_gpu_particle
```

Expected: 4개 테스트 전부 PASS (기존 3 + GridBuildCorrectness 1).

- [ ] **Step 4: Commit**

```bash
git add shaders/particle_grid_build.wgsl tests/test_gpu_particle.cpp
git commit -m "feat: add spatial hash grid build shader with clear/insert passes"
```

---

### Task 3: Pass 3 셰이더 — 파티클 간 충돌

**Files:**
- Create: `shaders/particle_collide.wgsl`
- Modify: `tests/test_gpu_particle.cpp`

- [ ] **Step 1: 충돌 셰이더 작성**

`shaders/particle_collide.wgsl`:

```wgsl
struct Particle {
    pos: vec4f,
    vel: vec4f,
};

struct SimParams {
    dt: f32,
    gravity: f32,
    count: u32,
    grid_res: u32,
    bounds_min: vec3f,
    _pad0: f32,
    bounds_max: vec3f,
    damping: f32,
    cell_size: f32,
    _pad1: f32,
    _pad2: f32,
    _pad3: f32,
};

const MAX_PER_CELL: u32 = 8u;

@group(0) @binding(0) var<storage, read_write> particles: array<Particle>;
@group(0) @binding(1) var<uniform> params: SimParams;
@group(0) @binding(2) var<storage, read> grid_counts: array<u32>;
@group(0) @binding(3) var<storage, read> grid_entries: array<u32>;

fn pos_to_cell_3d(pos: vec3f) -> vec3i {
    let rel = (pos - params.bounds_min) / params.cell_size;
    return vec3i(
        clamp(i32(rel.x), 0, i32(params.grid_res) - 1),
        clamp(i32(rel.y), 0, i32(params.grid_res) - 1),
        clamp(i32(rel.z), 0, i32(params.grid_res) - 1),
    );
}

fn cell_index(cx: i32, cy: i32, cz: i32) -> u32 {
    let res = i32(params.grid_res);
    return u32(cx + cy * res + cz * res * res);
}

@compute @workgroup_size(64)
fn main(@builtin(global_invocation_id) gid: vec3u) {
    let idx = gid.x;
    if (idx >= params.count) { return; }

    var p = particles[idx];
    let my_cell = pos_to_cell_3d(p.pos.xyz);
    let my_radius = p.pos.w;
    let res = i32(params.grid_res);

    var force = vec3f(0.0, 0.0, 0.0);

    // Check 27 neighboring cells
    for (var dz = -1; dz <= 1; dz = dz + 1) {
        for (var dy = -1; dy <= 1; dy = dy + 1) {
            for (var dx = -1; dx <= 1; dx = dx + 1) {
                let nx = my_cell.x + dx;
                let ny = my_cell.y + dy;
                let nz = my_cell.z + dz;

                if (nx < 0 || nx >= res || ny < 0 || ny >= res || nz < 0 || nz >= res) {
                    continue;
                }

                let cell = cell_index(nx, ny, nz);
                let count = min(grid_counts[cell], MAX_PER_CELL);

                for (var s = 0u; s < count; s = s + 1u) {
                    let other_idx = grid_entries[cell * MAX_PER_CELL + s];
                    if (other_idx == idx) { continue; }

                    let other = particles[other_idx];
                    let diff = p.pos.xyz - other.pos.xyz;
                    let dist = length(diff);
                    let min_dist = my_radius + other.pos.w;

                    if (dist < min_dist && dist > 0.0001) {
                        let normal = diff / dist;
                        let overlap = min_dist - dist;

                        // Position correction (push apart)
                        p.pos = vec4f(p.pos.xyz + normal * overlap * 0.5, p.pos.w);

                        // Velocity response (elastic collision)
                        let rel_vel = p.vel.xyz - other.vel.xyz;
                        let vel_along_normal = dot(rel_vel, normal);
                        if (vel_along_normal < 0.0) {
                            p.vel = vec4f(
                                p.vel.xyz - normal * vel_along_normal,
                                p.vel.w
                            );
                        }
                    }
                }
            }
        }
    }

    particles[idx] = p;
}
```

- [ ] **Step 2: 충돌 테스트 추가**

`tests/test_gpu_particle.cpp`에 추가:

```cpp
// ---------------------------------------------------------------------------
// Pass 3: Particle Collision
// ---------------------------------------------------------------------------

TEST(GpuParticleTest, TwoParticleCollision) {
    auto window = make_window();
    auto ctx = gg::GpuContext::create(*window);

    auto integrate_src = read_file("shaders/particle_integrate.wgsl");
    auto grid_src = read_file("shaders/particle_grid_build.wgsl");
    auto collide_src = read_file("shaders/particle_collide.wgsl");

    constexpr uint32_t kCount = 2;
    constexpr uint32_t kGridRes = 4;
    constexpr uint32_t kTotalCells = kGridRes * kGridRes * kGridRes;
    constexpr uint32_t kMaxPerCell = 8;

    // Two particles approaching each other head-on
    std::vector<GpuParticle> particles(kCount);
    // Particle 0: moving right
    particles[0].pos[0] = -0.04f;
    particles[0].pos[1] = 5.0f;
    particles[0].pos[2] = 0.0f;
    particles[0].pos[3] = 0.05f; // radius
    particles[0].vel[0] = 2.0f;
    particles[0].vel[1] = 0.0f;
    particles[0].vel[2] = 0.0f;
    particles[0].vel[3] = 1.0f;

    // Particle 1: moving left
    particles[1].pos[0] = 0.04f;
    particles[1].pos[1] = 5.0f;
    particles[1].pos[2] = 0.0f;
    particles[1].pos[3] = 0.05f;
    particles[1].vel[0] = -2.0f;
    particles[1].vel[1] = 0.0f;
    particles[1].vel[2] = 0.0f;
    particles[1].vel[3] = 1.0f;

    SimParams params = make_default_params(kCount);
    params.gravity = 0.0f; // disable gravity
    params.grid_res = kGridRes;
    params.cell_size = 20.0f / static_cast<float>(kGridRes);

    uint64_t pbytes = kCount * sizeof(GpuParticle);
    gg::GpuBuffer pbuf = gg::GpuBuffer::create_storage(ctx->device(), pbytes);
    pbuf.upload(ctx->queue(), particles.data(), pbytes);

    gg::GpuBuffer ubuf = gg::GpuBuffer::create_uniform(ctx->device(), sizeof(SimParams));
    ubuf.upload(ctx->queue(), &params, sizeof(SimParams));

    uint64_t counts_bytes = kTotalCells * sizeof(uint32_t);
    gg::GpuBuffer counts_buf = gg::GpuBuffer::create_storage(ctx->device(), counts_bytes);

    uint64_t entries_bytes = kTotalCells * kMaxPerCell * sizeof(uint32_t);
    gg::GpuBuffer entries_buf = gg::GpuBuffer::create_storage(ctx->device(), entries_bytes);

    auto integrate_pipe = gg::ComputePipeline::create(ctx->device(), integrate_src);
    auto clear_pipe = gg::ComputePipeline::create(ctx->device(), grid_src, "clear");
    auto insert_pipe = gg::ComputePipeline::create(ctx->device(), grid_src, "insert");
    auto collide_pipe = gg::ComputePipeline::create(ctx->device(), collide_src);

    // Run full 3-pass pipeline
    integrate_pipe->dispatch(ctx->device(), ctx->queue(),
                             {pbuf.handle(), ubuf.handle()}, workgroups(kCount));
    clear_pipe->dispatch(ctx->device(), ctx->queue(),
                         {pbuf.handle(), ubuf.handle(),
                          counts_buf.handle(), entries_buf.handle()},
                         workgroups(kTotalCells));
    insert_pipe->dispatch(ctx->device(), ctx->queue(),
                          {pbuf.handle(), ubuf.handle(),
                           counts_buf.handle(), entries_buf.handle()},
                          workgroups(kCount));
    collide_pipe->dispatch(ctx->device(), ctx->queue(),
                           {pbuf.handle(), ubuf.handle(),
                            counts_buf.handle(), entries_buf.handle()},
                           workgroups(kCount));

    auto result = pbuf.readback(ctx->device(), ctx->queue());
    const auto* out = reinterpret_cast<const GpuParticle*>(result.data());

    // After collision: particles should be pushed apart
    // Particle 0 should be moving leftward (or at least slower rightward)
    // Particle 1 should be moving rightward (or at least slower leftward)
    EXPECT_GT(out[0].pos[0] - out[1].pos[0], 0.0f)
        << "Particles should not overlap after collision (p0 should be left of where it was heading)";

    // Velocities should have exchanged or reversed
    // The exact values depend on the collision response, but
    // particle 0 should no longer be moving rightward at full speed
    EXPECT_LT(out[0].vel[0], 2.0f) << "Particle 0 velocity should decrease after collision";
}
```

- [ ] **Step 3: 빌드 및 테스트**

```bash
meson compile -C builddir test_gpu_particle
./builddir/tests/test_gpu_particle
```

Expected: 5개 테스트 전부 PASS.

- [ ] **Step 4: Commit**

```bash
git add shaders/particle_collide.wgsl tests/test_gpu_particle.cpp
git commit -m "feat: add particle collision shader with grid-based neighbor search"
```

---

### Task 4: ParticleSystem 클래스

**Files:**
- Create: `engine/compute/particle_system.h`
- Create: `engine/compute/particle_system.cpp`
- Modify: `engine/meson.build`

- [ ] **Step 1: 헤더 작성**

`engine/compute/particle_system.h`:

```cpp
#pragma once
#include "math/vec3.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace gg {

class GpuContext;

struct ParticleData {
    float pos[4]; // xyz = position, w = radius
    float vel[4]; // xyz = velocity, w = inverse_mass
};

struct ParticleSystemConfig {
    uint32_t max_particles = 100000;
    float gravity = -9.81f;
    float damping = 0.8f;
    float particle_radius = 0.05f;
    Vec3 bounds_min = {-10.0f, 0.0f, -10.0f};
    Vec3 bounds_max = {10.0f, 20.0f, 10.0f};
    uint32_t grid_resolution = 64;
};

class ParticleSystem {
public:
    static std::unique_ptr<ParticleSystem> create(GpuContext& gpu,
                                                   const ParticleSystemConfig& config = {});
    ~ParticleSystem();

    ParticleSystem(const ParticleSystem&) = delete;
    ParticleSystem& operator=(const ParticleSystem&) = delete;

    /// Spawn particles at center with random spread
    void spawn(uint32_t count, Vec3 center, Vec3 extent, Vec3 velocity_range);

    /// Run one simulation step (3 compute passes)
    void step(float dt);

    /// Read particle data back to CPU (synchronous, for testing)
    [[nodiscard]] std::vector<ParticleData> readback();

    /// Current active particle count
    [[nodiscard]] uint32_t count() const;

private:
    ParticleSystem() = default;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace gg
```

- [ ] **Step 2: 구현 작성**

`engine/compute/particle_system.cpp`:

```cpp
#include "compute/particle_system.h"
#include "compute/compute_pipeline.h"
#include "compute/gpu_buffer.h"
#include "renderer/gpu_context.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <random>
#include <sstream>
#include <stdexcept>

namespace gg {

namespace {

std::string load_shader(const char* path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        throw std::runtime_error(std::string("Cannot open shader: ") + path);
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

struct GpuSimParams {
    float dt;
    float gravity;
    uint32_t count;
    uint32_t grid_res;
    float bounds_min[3];
    float _pad0;
    float bounds_max[3];
    float damping;
    float cell_size;
    float _pad1;
    float _pad2;
    float _pad3;
};
static_assert(sizeof(GpuSimParams) == 64, "GpuSimParams must be 64 bytes");

constexpr uint32_t WORKGROUP_SIZE = 64;
constexpr uint32_t MAX_PER_CELL = 8;

uint32_t div_ceil(uint32_t n, uint32_t d) {
    return (n + d - 1) / d;
}

} // namespace

struct ParticleSystem::Impl {
    GpuContext* gpu = nullptr;
    ParticleSystemConfig config;
    uint32_t active_count = 0;

    // GPU buffers
    GpuBuffer particle_buf{GpuBuffer::create_storage(nullptr, 0)};
    GpuBuffer params_buf{GpuBuffer::create_uniform(nullptr, 0)};
    GpuBuffer grid_counts_buf{GpuBuffer::create_storage(nullptr, 0)};
    GpuBuffer grid_entries_buf{GpuBuffer::create_storage(nullptr, 0)};

    // Pipelines
    std::unique_ptr<ComputePipeline> integrate_pipe;
    std::unique_ptr<ComputePipeline> grid_clear_pipe;
    std::unique_ptr<ComputePipeline> grid_insert_pipe;
    std::unique_ptr<ComputePipeline> collide_pipe;
};

ParticleSystem::~ParticleSystem() = default;

std::unique_ptr<ParticleSystem> ParticleSystem::create(GpuContext& gpu,
                                                       const ParticleSystemConfig& config) {
    auto sys = std::unique_ptr<ParticleSystem>(new ParticleSystem());
    sys->impl_ = std::make_unique<Impl>();
    auto& impl = *sys->impl_;
    impl.gpu = &gpu;
    impl.config = config;

    auto device = gpu.device();

    // Create buffers
    uint64_t particle_bytes = config.max_particles * sizeof(ParticleData);
    impl.particle_buf = GpuBuffer::create_storage(device, particle_bytes);

    impl.params_buf = GpuBuffer::create_uniform(device, sizeof(GpuSimParams));

    uint32_t total_cells = config.grid_resolution * config.grid_resolution * config.grid_resolution;
    impl.grid_counts_buf = GpuBuffer::create_storage(device, total_cells * sizeof(uint32_t));
    impl.grid_entries_buf =
        GpuBuffer::create_storage(device, total_cells * MAX_PER_CELL * sizeof(uint32_t));

    // Create pipelines
    auto integrate_src = load_shader("shaders/particle_integrate.wgsl");
    auto grid_src = load_shader("shaders/particle_grid_build.wgsl");
    auto collide_src = load_shader("shaders/particle_collide.wgsl");

    impl.integrate_pipe = ComputePipeline::create(device, integrate_src);
    impl.grid_clear_pipe = ComputePipeline::create(device, grid_src, "clear");
    impl.grid_insert_pipe = ComputePipeline::create(device, grid_src, "insert");
    impl.collide_pipe = ComputePipeline::create(device, collide_src);

    return sys;
}

void ParticleSystem::spawn(uint32_t count, Vec3 center, Vec3 extent, Vec3 velocity_range) {
    auto& impl = *impl_;
    uint32_t start = impl.active_count;
    uint32_t to_add = std::min(count, impl.config.max_particles - start);
    if (to_add == 0) {
        return;
    }

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

    std::vector<ParticleData> particles(to_add);
    for (auto& p : particles) {
        p.pos[0] = center.x + dist(rng) * extent.x;
        p.pos[1] = center.y + dist(rng) * extent.y;
        p.pos[2] = center.z + dist(rng) * extent.z;
        p.pos[3] = impl.config.particle_radius;
        p.vel[0] = dist(rng) * velocity_range.x;
        p.vel[1] = dist(rng) * velocity_range.y;
        p.vel[2] = dist(rng) * velocity_range.z;
        p.vel[3] = 1.0f; // inverse_mass
    }

    uint64_t offset = start * sizeof(ParticleData);
    impl.particle_buf.upload(impl.gpu->queue(), particles.data(),
                             to_add * sizeof(ParticleData), offset);
    impl.active_count = start + to_add;
}

void ParticleSystem::step(float dt) {
    auto& impl = *impl_;
    if (impl.active_count == 0) {
        return;
    }

    auto device = impl.gpu->device();
    auto queue = impl.gpu->queue();

    // Upload params
    auto& cfg = impl.config;
    float cell_size = (cfg.bounds_max.x - cfg.bounds_min.x) / static_cast<float>(cfg.grid_resolution);
    GpuSimParams params{};
    params.dt = dt;
    params.gravity = cfg.gravity;
    params.count = impl.active_count;
    params.grid_res = cfg.grid_resolution;
    params.bounds_min[0] = cfg.bounds_min.x;
    params.bounds_min[1] = cfg.bounds_min.y;
    params.bounds_min[2] = cfg.bounds_min.z;
    params.bounds_max[0] = cfg.bounds_max.x;
    params.bounds_max[1] = cfg.bounds_max.y;
    params.bounds_max[2] = cfg.bounds_max.z;
    params.damping = cfg.damping;
    params.cell_size = cell_size;
    impl.params_buf.upload(queue, &params, sizeof(GpuSimParams));

    uint32_t particle_wg = div_ceil(impl.active_count, WORKGROUP_SIZE);
    uint32_t total_cells =
        cfg.grid_resolution * cfg.grid_resolution * cfg.grid_resolution;
    uint32_t grid_wg = div_ceil(total_cells, WORKGROUP_SIZE);

    // Pass 1: Integrate + boundary
    impl.integrate_pipe->dispatch(device, queue,
                                  {impl.particle_buf.handle(), impl.params_buf.handle()},
                                  particle_wg);

    // Pass 2: Grid clear + insert
    impl.grid_clear_pipe->dispatch(
        device, queue,
        {impl.particle_buf.handle(), impl.params_buf.handle(),
         impl.grid_counts_buf.handle(), impl.grid_entries_buf.handle()},
        grid_wg);

    impl.grid_insert_pipe->dispatch(
        device, queue,
        {impl.particle_buf.handle(), impl.params_buf.handle(),
         impl.grid_counts_buf.handle(), impl.grid_entries_buf.handle()},
        particle_wg);

    // Pass 3: Collide
    impl.collide_pipe->dispatch(
        device, queue,
        {impl.particle_buf.handle(), impl.params_buf.handle(),
         impl.grid_counts_buf.handle(), impl.grid_entries_buf.handle()},
        particle_wg);
}

std::vector<ParticleData> ParticleSystem::readback() {
    auto& impl = *impl_;
    if (impl.active_count == 0) {
        return {};
    }

    auto raw = impl.particle_buf.readback(impl.gpu->device(), impl.gpu->queue());
    uint32_t count = impl.active_count;
    std::vector<ParticleData> result(count);
    std::memcpy(result.data(), raw.data(), count * sizeof(ParticleData));
    return result;
}

uint32_t ParticleSystem::count() const {
    return impl_ ? impl_->active_count : 0;
}

} // namespace gg
```

- [ ] **Step 3: engine/meson.build에 소스 추가**

`engine/meson.build`의 `engine_sources`에 추가:

```meson
'compute/particle_system.cpp',
```

- [ ] **Step 4: 빌드 확인**

```bash
meson compile -C builddir
```

Expected: 빌드 성공.

- [ ] **Step 5: Commit**

```bash
git add engine/compute/particle_system.h engine/compute/particle_system.cpp engine/meson.build
git commit -m "feat: add ParticleSystem class with 3-pass GPU compute pipeline"
```

---

### Task 5: ParticleSystem 통합 테스트

**Files:**
- Modify: `tests/test_gpu_particle.cpp`

- [ ] **Step 1: ParticleSystem 테스트 추가**

`tests/test_gpu_particle.cpp`에 추가:

```cpp
#include "compute/particle_system.h"

// ---------------------------------------------------------------------------
// ParticleSystem Integration Tests
// ---------------------------------------------------------------------------

TEST(ParticleSystemTest, CreateAndSpawn) {
    auto window = make_window();
    auto ctx = gg::GpuContext::create(*window);

    auto sys = gg::ParticleSystem::create(*ctx, {.max_particles = 1000});
    ASSERT_NE(sys, nullptr);
    EXPECT_EQ(sys->count(), 0u);

    sys->spawn(100, {0, 5, 0}, {1, 1, 1}, {1, 1, 1});
    EXPECT_EQ(sys->count(), 100u);

    auto data = sys->readback();
    ASSERT_EQ(data.size(), 100u);
}

TEST(ParticleSystemTest, StepAppliesGravity) {
    auto window = make_window();
    auto ctx = gg::GpuContext::create(*window);

    auto sys = gg::ParticleSystem::create(*ctx, {
        .max_particles = 1,
        .gravity = -9.81f,
        .grid_resolution = 4,
    });

    // Manually spawn one particle at known position
    sys->spawn(1, {0, 10, 0}, {0, 0, 0}, {0, 0, 0});

    for (int i = 0; i < 10; ++i) {
        sys->step(1.0f / 60.0f);
    }

    auto data = sys->readback();
    ASSERT_EQ(data.size(), 1u);
    EXPECT_LT(data[0].pos[1], 10.0f) << "Particle should fall under gravity";
}

TEST(ParticleSystemTest, LargeScaleStability) {
    auto window = make_window();
    auto ctx = gg::GpuContext::create(*window);

    auto sys = gg::ParticleSystem::create(*ctx, {
        .max_particles = 10000,
        .grid_resolution = 16,
    });

    sys->spawn(10000, {0, 10, 0}, {5, 5, 5}, {2, 2, 2});

    // Run 60 frames
    for (int i = 0; i < 60; ++i) {
        sys->step(1.0f / 60.0f);
    }

    auto data = sys->readback();
    ASSERT_EQ(data.size(), 10000u);

    // Check no NaN/Inf
    for (size_t i = 0; i < data.size(); ++i) {
        EXPECT_FALSE(std::isnan(data[i].pos[0])) << "NaN at particle " << i;
        EXPECT_FALSE(std::isnan(data[i].pos[1])) << "NaN at particle " << i;
        EXPECT_FALSE(std::isnan(data[i].pos[2])) << "NaN at particle " << i;
        EXPECT_FALSE(std::isinf(data[i].pos[0])) << "Inf at particle " << i;
        EXPECT_FALSE(std::isinf(data[i].pos[1])) << "Inf at particle " << i;
        EXPECT_FALSE(std::isinf(data[i].pos[2])) << "Inf at particle " << i;
    }

    // All particles should be within bounds
    for (size_t i = 0; i < data.size(); ++i) {
        EXPECT_GE(data[i].pos[0], -10.5f) << "Out of bounds particle " << i;
        EXPECT_LE(data[i].pos[0], 10.5f) << "Out of bounds particle " << i;
        EXPECT_GE(data[i].pos[1], -0.5f) << "Out of bounds particle " << i;
        EXPECT_LE(data[i].pos[1], 20.5f) << "Out of bounds particle " << i;
    }
}
```

- [ ] **Step 2: 빌드 및 테스트**

```bash
meson compile -C builddir test_gpu_particle
./builddir/tests/test_gpu_particle
```

Expected: 8개 테스트 전부 PASS.

- [ ] **Step 3: Commit**

```bash
git add tests/test_gpu_particle.cpp
git commit -m "test: add ParticleSystem integration tests with large-scale stability check"
```

---

### Task 6: 커버리지 확인 및 최종 검증

- [ ] **Step 1: 전체 테스트**

```bash
meson test -C builddir
```

Expected: 전체 테스트 스위트 PASS (기존 28 + gpu_particle 1 = 29).

- [ ] **Step 2: 커버리지 확인**

```bash
meson setup builddir-coverage --reconfigure -Denable_tests=true -Denable_scripts=true -Db_coverage=true
rm -f builddir-coverage/**/*.gcda
meson compile -C builddir-coverage test_gpu_particle
./builddir-coverage/tests/test_gpu_particle
gcovr --filter 'engine/compute/particle_system' --root . builddir-coverage/ --print-summary
```

Expected: `particle_system.cpp` 80%+.

- [ ] **Step 3: Commit (if any fixes needed)**

```bash
git add -A
git commit -m "chore: coverage and final verification for GPU particle physics"
```
