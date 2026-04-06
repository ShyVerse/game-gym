# Gizmo Drag Interaction Design

## Overview

기즈모 화살표를 마우스로 클릭 드래그하여 엔티티의 위치(Translate)를 이동하는 인터랙션 기능.

**범위**: 이동(Translate)만. 회전/스케일은 별도 피처로 추후 구현.

**인터랙션 모델**: 호버 하이라이트 + 클릭 드래그 (유니티 방식)

**기즈모 크기**: 화면 크기 고정 (Screen-space constant) — 카메라 거리에 관계없이 동일한 픽셀 크기 유지

**수학 접근**: Ray-Axis Closest Point 방식 — 스크린 좌표를 월드 레이로 변환하고, 레이와 축 직선 사이의 최근접점으로 히트 테스트 및 드래그 계산

---

## 1. Math Utilities

기존 Vec3/Mat4 구조체에 연산을 추가한다.

### Vec3 (`engine/math/vec3.h`)

Free function으로 구현:

- `vec3_add(Vec3 a, Vec3 b) -> Vec3`
- `vec3_sub(Vec3 a, Vec3 b) -> Vec3`
- `vec3_scale(Vec3 v, float s) -> Vec3`
- `vec3_dot(Vec3 a, Vec3 b) -> float`
- `vec3_cross(Vec3 a, Vec3 b) -> Vec3`
- `vec3_normalize(Vec3 v) -> Vec3`
- `vec3_length(Vec3 v) -> float`

### Mat4 (`engine/math/mat4.h`)

멤버 함수로 추가:

- `Mat4 inverse() const` — 4x4 일반 역행렬 (cofactor 방식)
- `Vec3 transform_point(Vec3 p) const` — 점 변환 (w=1, perspective divide)
- `Vec3 transform_direction(Vec3 d) const` — 방향 변환 (w=0)

### Ray (`engine/math/ray.h`)

새 파일:

```cpp
struct Ray {
    Vec3 origin;
    Vec3 direction;  // normalized
};

// 스크린 좌표 (픽셀) → 월드 레이
Ray ray_from_screen(float screen_x, float screen_y,
                    uint32_t fb_width, uint32_t fb_height,
                    const Mat4& inverse_view_proj);

// 레이와 직선 사이의 최근접점 거리 + 직선 위 최근접점 파라미터
// axis_origin: 축 시작점, axis_dir: 축 방향 (normalized)
// out_t: 축 위 최근접점 파라미터 (0=origin, 1=origin+axis_dir)
// returns: 레이와 축 사이의 최소 거리
float ray_axis_distance(const Ray& ray, const Vec3& axis_origin,
                        const Vec3& axis_dir, float& out_t);
```

---

## 2. GizmoInteraction (`engine/editor/gizmo_interaction.h/.cpp`)

렌더링과 분리된 순수 인터랙션 로직.

### 상태 머신

```
Idle → (호버 감지) → Hovering → (마우스 클릭) → Dragging → (마우스 릴리즈) → Idle
```

### 인터페이스

```cpp
struct GizmoState {
    int hovered_axis;     // -1=없음, 0=X, 1=Y, 2=Z
    int dragging_axis;    // -1=없음, 0=X, 1=Y, 2=Z
    Vec3 drag_start_pos;  // 드래그 시작 시 엔티티 위치
};

class GizmoInteraction {
public:
    static std::unique_ptr<GizmoInteraction> create();

    void update(float mouse_x, float mouse_y, bool mouse_down,
                uint32_t fb_width, uint32_t fb_height,
                const Camera& camera,
                const Vec3& gizmo_position, float gizmo_scale);

    GizmoState state() const;
    Vec3 position_delta() const;

private:
    GizmoState state_{};
    Vec3 last_closest_point_{};
    Vec3 frame_delta_{};
    bool was_mouse_down_ = false;  // 이전 프레임 마우스 상태 (클릭 엣지 감지)
};
```

### 히트 테스트

1. `ray_from_screen()` 으로 월드 레이 생성
2. 3개 축 각각에 대해 `ray_axis_distance()` 호출
3. 거리가 threshold 이내 (screen-space ~15px 상당)인 축 중 가장 가까운 것 선택
4. threshold는 `gizmo_scale * 0.15f` (월드 스페이스로 변환된 값)

### 드래그 계산

1. 드래그 시작: 선택된 축의 최근접점을 `last_closest_point_`에 저장
2. 매 프레임: 새 최근접점 계산, `frame_delta_ = new_point - last_closest_point_`
3. `last_closest_point_` 갱신
4. delta는 축 방향으로만 제한됨 (최근접점이 축 직선 위에 있으므로 자동)

### 엔티티 선택

여러 엔티티의 기즈모가 있을 때:
- 모든 엔티티의 기즈모에 대해 히트 테스트 수행
- 레이와의 최근접 거리가 가장 짧은 기즈모+축 조합 선택
- `update()` 호출을 루프에서 여러 번 수행하는 대신, 엔진 측에서 가장 가까운 기즈모를 찾아서 단일 호출

---

## 3. GizmoRenderer 확장 (`engine/renderer/gizmo_renderer.h/.cpp`)

### 시그니처 변경

```cpp
void draw(const Vec3& position, const Camera& camera, WGPURenderPassEncoder pass,
          float scale = 1.0f, int hovered_axis = -1, int dragging_axis = -1);
```

기본값으로 기존 호출 코드 호환성 유지.

### 색상 로직

- **기본**: X=빨강(0.9,0.2,0.2), Y=초록(0.2,0.9,0.2), Z=파랑(0.3,0.3,1.0) — 현재와 동일
- **호버**: 해당 축 색상을 밝게 (각 채널 +0.15, clamp 1.0)
- **드래그 중**: 드래그 축은 노란색(1.0,1.0,0.3), 나머지 축은 어둡게 (각 채널 *0.4)

### 화면 크기 고정

- `scale` 파라미터로 기즈모 전체 크기 조절
- `build_arrow()` 내부에서 SHAFT_LENGTH, CONE_LENGTH, SHAFT_RADIUS, CONE_RADIUS에 scale 곱하기
- 스케일 계산 공식: `scale = camera_distance * tan(fov/2) * SCREEN_RATIO`
  - `SCREEN_RATIO`: 기즈모가 화면 높이의 약 10%를 차지하도록 하는 상수 (0.12 정도)

---

## 4. Engine Integration (`engine/core/engine.h/.cpp`)

### 멤버 추가

```cpp
std::unique_ptr<GizmoInteraction> gizmo_interaction_;
```

생성: 카메라 + 기즈모 렌더러가 있을 때 함께 생성.

### 프레임 루프 순서

```
1. poll_events()
2. 기즈모 인터랙션 업데이트:
   - 모든 Renderable 엔티티 순회
   - 각 기즈모에 대해 레이 히트 테스트
   - 가장 가까운 기즈모+축 결정
   - gizmo_interaction_->update() 호출
3. if (dragging) {
     target_entity.Transform.position += delta
     if (has RigidBody) → sync_to_physics = true
     // 카메라 오빗 스킵
   } else {
     기존 카메라 오빗 처리
   }
4. physics step, editor, render...
5. gizmo draw 시 scale/hovered/dragging 파라미터 전달
```

---

## 5. Test Strategy

### 단위 테스트 대상

| 대상 | 테스트 내용 |
|------|-----------|
| Vec3 연산 | dot, cross, normalize, length 정확성 |
| Mat4::inverse() | M * M.inverse() ≈ Identity |
| transform_point/direction | 알려진 변환 행렬로 결과 검증 |
| ray_from_screen() | 스크린 중심 → 카메라 forward 방향 |
| ray_axis_distance() | 알려진 레이-축 조합의 거리/최근접점 |
| GizmoInteraction 상태 전이 | Idle→Hover→Drag→Idle 시퀀스 |
| GizmoInteraction 드래그 델타 | 알려진 입력으로 예상 이동량 검증 |
| 화면 크기 고정 스케일 | 거리 변화 시 스케일 계산 정확성 |

### 테스트 불가 (GPU 의존)

- GizmoRenderer 색상/스케일 시각적 결과
- 실제 렌더 패스 실행

### 커버리지 목표

수학 유틸리티 + GizmoInteraction에서 80% 이상. 현재 threshold 49%이므로 여유 있음.

---

## File Map

| 파일 | 작업 |
|------|------|
| `engine/math/vec3.h` | 새로 생성 — Vec3 연산 함수 |
| `engine/math/mat4.h` | 수정 — inverse, transform_point, transform_direction 추가 |
| `engine/math/ray.h` | 새로 생성 — Ray 구조체 + ray_from_screen + ray_axis_distance |
| `engine/editor/gizmo_interaction.h` | 새로 생성 — GizmoInteraction 클래스 |
| `engine/editor/gizmo_interaction.cpp` | 새로 생성 — 인터랙션 로직 구현 |
| `engine/renderer/gizmo_renderer.h` | 수정 — draw() 시그니처 확장 |
| `engine/renderer/gizmo_renderer.cpp` | 수정 — 하이라이트 색상, 스케일 적용 |
| `engine/core/engine.h` | 수정 — gizmo_interaction_ 멤버 추가 |
| `engine/core/engine.cpp` | 수정 — 루프에 인터랙션 삽입 |
| `engine/meson.build` | 수정 — 새 소스 파일 추가 |
| `tests/test_vec3.cpp` | 새로 생성 |
| `tests/test_mat4_inverse.cpp` | 새로 생성 |
| `tests/test_ray.cpp` | 새로 생성 |
| `tests/test_gizmo_interaction.cpp` | 새로 생성 |
