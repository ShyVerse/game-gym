# GPU Particle Physics MVP Design

**Date:** 2026-04-07
**Status:** Approved
**Author:** Seongmin + Claude
**Plan:** Plan 4 — GPU Physics

## Overview

GPU 컴퓨트 셰이더로 최대 100K 파티클의 물리 시뮬레이션을 구현한다. 중력, 환경 충돌(바닥/벽), 파티클 간 충돌(spatial hash 그리드)을 지원하며, 렌더링 없이 테스트로 correctness를 검증한다.

### 동기

- CPU 기반 파티클 물리는 10K 이상에서 프레임 드롭
- GPU 컴퓨트 인프라(GpuBuffer, ComputePipeline)가 이미 완성되어 있음
- SAO급 대규모 시뮬레이션의 기반 기술

## Architecture

```
Engine Main Loop (매 프레임)
│
├── 1. CPU: SimParams 업로드 (dt, gravity, bounds)
├── 2. GPU Compute Pass 1: 속도 적분 + 환경 충돌
├── 3. GPU Compute Pass 2: Spatial Hash 그리드 구축
├── 4. GPU Compute Pass 3: 파티클 간 충돌 검출 + 응답
└── 5. (선택) CPU Readback: 테스트 검증용
```

### 3-Pass 파이프라인 설계 근거

단일 패스로 적분+충돌을 합치면 파티클 간 충돌에서 이전 프레임의 위치를 참조하게 되어 부정확해진다. 3-pass 분리로:
- Pass 1: 속도/위치 업데이트 (독립적, 완전 병렬)
- Pass 2: 업데이트된 위치 기반으로 그리드 구축 (barrier 후 실행)
- Pass 3: 그리드 기반 이웃 탐색 + 충돌 응답 (barrier 후 실행)

## Components

### 1. ParticleSystem

**파일:** `engine/compute/particle_system.h`, `engine/compute/particle_system.cpp`

```cpp
struct ParticleSystemConfig {
    uint32_t max_particles = 100000;
    float gravity = -9.81f;
    float damping = 0.99f;
    float particle_radius = 0.05f;
    Vec3 bounds_min = {-10.0f, 0.0f, -10.0f};
    Vec3 bounds_max = {10.0f, 20.0f, 10.0f};
    uint32_t grid_resolution = 64; // cells per axis
};

class ParticleSystem {
public:
    static std::unique_ptr<ParticleSystem> create(GpuContext& gpu,
                                                   const ParticleSystemConfig& config);
    ~ParticleSystem();

    /// Spawn particles in a region with random velocities
    void spawn(uint32_t count, Vec3 center, Vec3 extent, Vec3 velocity_range);

    /// Run one simulation step (3 compute passes)
    void step(float dt);

    /// Read particle data back to CPU (synchronous, for testing)
    std::vector<Particle> readback();

    /// Current active particle count
    [[nodiscard]] uint32_t count() const;
};
```

**책임:**
- GPU 버퍼 생성/관리 (particles, grid, params)
- 3개 컴퓨트 파이프라인 생성 (integrate, grid_build, collide)
- step()에서 순차 디스패치 + GPU barrier
- readback()으로 테스트 검증 지원

### 2. GPU 데이터 레이아웃

```wgsl
struct Particle {
    pos: vec4f,    // xyz = position, w = radius
    vel: vec4f,    // xyz = velocity, w = inverse_mass (0 = static)
};

struct SimParams {
    dt: f32,
    gravity: f32,
    count: u32,
    grid_res: u32,       // cells per axis
    bounds_min: vec3f,
    _pad0: f32,
    bounds_max: vec3f,
    damping: f32,
    cell_size: f32,       // (bounds_max - bounds_min) / grid_res
    _pad1: f32,
    _pad2: f32,
    _pad3: f32,
};
```

- `Particle` 구조체는 32바이트 (vec4f x 2), GPU 캐시라인 친화적
- `w` 필드를 radius/inverse_mass로 활용하여 메모리 절약
- 100K 파티클 = 3.2MB GPU 메모리

### 3. 셰이더

#### `shaders/particle_integrate.wgsl` — Pass 1

```
@compute @workgroup_size(64)
fn main(@builtin(global_invocation_id) gid: vec3u) {
    // 1. vel += gravity * dt
    // 2. pos += vel * dt
    // 3. 환경 바운더리 충돌: pos를 bounds 내로 클램프, vel 반사 + damping
}
```

바인딩:
- `@binding(0)` particles: storage, read_write
- `@binding(1)` params: uniform

#### `shaders/particle_grid_build.wgsl` — Pass 2

```
@compute @workgroup_size(64)
fn clear(@builtin(global_invocation_id) gid: vec3u) {
    // grid_counts[cell] = 0
}

@compute @workgroup_size(64)
fn insert(@builtin(global_invocation_id) gid: vec3u) {
    // cell = hash(pos)
    // idx = atomicAdd(grid_counts[cell], 1)
    // grid_entries[cell * MAX_PER_CELL + idx] = particle_idx
}
```

바인딩:
- `@binding(0)` particles: storage, read
- `@binding(1)` params: uniform
- `@binding(2)` grid_counts: storage, read_write (atomic)
- `@binding(3)` grid_entries: storage, read_write

그리드 크기: 64^3 = 262,144 셀, MAX_PER_CELL = 8 (고정 버킷)

#### `shaders/particle_collide.wgsl` — Pass 3

```
@compute @workgroup_size(64)
fn main(@builtin(global_invocation_id) gid: vec3u) {
    // 자신의 셀 + 26개 이웃 셀 순회
    // 각 이웃 파티클과 거리 체크
    // 겹침 시: 위치 분리 + 속도 반사 (탄성 충돌)
}
```

바인딩:
- `@binding(0)` particles: storage, read_write
- `@binding(1)` params: uniform
- `@binding(2)` grid_counts: storage, read
- `@binding(3)` grid_entries: storage, read

충돌 응답: 단순 탄성 충돌 (운동량 보존, 반발 계수 적용)

### 4. Spatial Hash 그리드

- 시뮬레이션 공간을 균등 3D 그리드로 분할
- 셀 크기 = 파티클 직경 * 2 (이웃 탐색 효율)
- 해시: `cell = floor((pos - bounds_min) / cell_size)`
- 1D 인덱스: `cell.x + cell.y * res + cell.z * res * res`
- 고정 버킷 (MAX_PER_CELL = 8): 오버플로우 시 무시 (근사)
- 매 프레임 clear → insert → collide

### 5. GPU Buffer 구성

| Buffer | 크기 | 용도 |
|--------|------|------|
| `particles` | 32B * 100K = 3.2MB | 파티클 위치/속도 |
| `grid_counts` | 4B * 64^3 = 1MB | 셀당 파티클 수 (atomic) |
| `grid_entries` | 4B * 64^3 * 8 = 8MB | 셀당 파티클 인덱스 |
| `params` | 64B | 시뮬레이션 파라미터 |
| **Total** | ~12.2MB | |

## Engine Integration

- `Engine::create()`에서 `ParticleSystem` 생성 (MCP처럼 config 기반 opt-in)
- `Engine::run()` 루프에서 `particle_system_->step(FIXED_DT)` 호출
- MCP 도구 확장 가능: `spawn_particles`, `get_particle_count` (후속)

## Testing Strategy

### Unit Tests (`tests/test_gpu_particle.cpp`)

1. **GravityFall** — 단일 파티클, 중력으로 낙하, y좌표 감소 확인
2. **BoundaryBounce** — 바닥(y=0) 충돌 시 반사, 바운더리 내 유지
3. **TwoParticleCollision** — 정면 충돌, 속도 교환 검증
4. **WallBounce** — 4면 벽 충돌 반사
5. **GridBuildCorrectness** — 파티클 위치→셀 매핑 정확성
6. **LargeScaleStability** — 100K 파티클, 60스텝, NaN/Inf 없음
7. **EnergyConservation** — 100스텝 후 총 운동에너지가 damping 비율 내로 감소
8. **SpawnInRegion** — spawn 후 파티클이 지정 영역 내에 위치

### Coverage Target

- `particle_system.cpp`: 80%+
- 셰이더: 기능 테스트로 간접 검증

## Dependencies

- 기존 `GpuBuffer`, `ComputePipeline` (engine/compute/)
- 기존 `GpuContext` (engine/renderer/)
- wgpu compute shader 기능 (이미 검증됨)

## Future Work

- #19: Jolt 리지드바디 ↔ GPU 파티클 충돌
- #20: GPU 파티클 렌더링 (포인트/빌보드)
- 멀티 ParticleSystem 인스턴스 지원
- 파티클 이벤트 (충돌 콜백, 수명 관리)
