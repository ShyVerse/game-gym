# Gizmo Drag Interaction Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Enable mouse-based click-drag on 3D arrow gizmos to translate entities along individual axes.

**Architecture:** Add Vec3/Mat4 math utilities, a Ray type for screen-to-world unprojection, and a GizmoInteraction class that manages hover/drag state. Extend GizmoRenderer for highlighting and screen-space constant sizing. Wire everything into the engine loop between input polling and camera orbit.

**Tech Stack:** C++20, WebGPU (wgpu-native), GTest, Meson

---

## File Map

| File | Action | Responsibility |
|------|--------|---------------|
| `engine/math/vec3.h` | Create | Vec3 free functions (add, sub, dot, cross, normalize, length, scale) |
| `engine/math/mat4.h` | Modify | Add inverse(), transform_point(), transform_direction() |
| `engine/math/ray.h` | Create | Ray struct + ray_from_screen() + ray_axis_distance() |
| `engine/editor/gizmo_interaction.h` | Create | GizmoInteraction class declaration |
| `engine/editor/gizmo_interaction.cpp` | Create | Hover/drag state machine + hit test + drag delta |
| `engine/renderer/camera.h` | Modify | Add eye_position(), fov(), distance() getters |
| `engine/renderer/camera.cpp` | Modify | Implement eye_position() |
| `engine/renderer/gizmo_renderer.h` | Modify | Extend draw() signature with scale/hovered/dragging |
| `engine/renderer/gizmo_renderer.cpp` | Modify | Highlight colors + scale support |
| `engine/core/engine.h` | Modify | Add gizmo_interaction_ member |
| `engine/core/engine.cpp` | Modify | Insert interaction into frame loop |
| `engine/meson.build` | Modify | Add gizmo_interaction.cpp to sources |
| `tests/test_vec3.cpp` | Create | Vec3 math tests |
| `tests/test_mat4_inverse.cpp` | Create | Mat4 inverse + transform tests |
| `tests/test_ray.cpp` | Create | Ray casting + axis distance tests |
| `tests/test_gizmo_interaction.cpp` | Create | Interaction state machine tests |
| `tests/meson.build` | Modify | Register new test executables |

---

### Task 1: Vec3 Math Utilities

**Files:**
- Create: `engine/math/vec3.h`
- Create: `tests/test_vec3.cpp`
- Modify: `tests/meson.build`

- [ ] **Step 1: Write the failing tests**

Create `tests/test_vec3.cpp`:

```cpp
#include "math/vec3.h"

#include <cmath>
#include <gtest/gtest.h>

TEST(Vec3Test, AddTwoVectors) {
    gg::Vec3 a{1.0f, 2.0f, 3.0f};
    gg::Vec3 b{4.0f, 5.0f, 6.0f};
    auto r = gg::vec3_add(a, b);
    EXPECT_FLOAT_EQ(r.x, 5.0f);
    EXPECT_FLOAT_EQ(r.y, 7.0f);
    EXPECT_FLOAT_EQ(r.z, 9.0f);
}

TEST(Vec3Test, SubTwoVectors) {
    gg::Vec3 a{4.0f, 5.0f, 6.0f};
    gg::Vec3 b{1.0f, 2.0f, 3.0f};
    auto r = gg::vec3_sub(a, b);
    EXPECT_FLOAT_EQ(r.x, 3.0f);
    EXPECT_FLOAT_EQ(r.y, 3.0f);
    EXPECT_FLOAT_EQ(r.z, 3.0f);
}

TEST(Vec3Test, ScaleVector) {
    gg::Vec3 v{1.0f, 2.0f, 3.0f};
    auto r = gg::vec3_scale(v, 2.0f);
    EXPECT_FLOAT_EQ(r.x, 2.0f);
    EXPECT_FLOAT_EQ(r.y, 4.0f);
    EXPECT_FLOAT_EQ(r.z, 6.0f);
}

TEST(Vec3Test, DotProduct) {
    gg::Vec3 a{1.0f, 0.0f, 0.0f};
    gg::Vec3 b{0.0f, 1.0f, 0.0f};
    EXPECT_FLOAT_EQ(gg::vec3_dot(a, b), 0.0f);

    gg::Vec3 c{1.0f, 2.0f, 3.0f};
    gg::Vec3 d{4.0f, 5.0f, 6.0f};
    EXPECT_FLOAT_EQ(gg::vec3_dot(c, d), 32.0f);
}

TEST(Vec3Test, CrossProduct) {
    gg::Vec3 x{1.0f, 0.0f, 0.0f};
    gg::Vec3 y{0.0f, 1.0f, 0.0f};
    auto z = gg::vec3_cross(x, y);
    EXPECT_FLOAT_EQ(z.x, 0.0f);
    EXPECT_FLOAT_EQ(z.y, 0.0f);
    EXPECT_FLOAT_EQ(z.z, 1.0f);
}

TEST(Vec3Test, Length) {
    gg::Vec3 v{3.0f, 4.0f, 0.0f};
    EXPECT_FLOAT_EQ(gg::vec3_length(v), 5.0f);
}

TEST(Vec3Test, NormalizeUnitVector) {
    gg::Vec3 v{0.0f, 3.0f, 4.0f};
    auto n = gg::vec3_normalize(v);
    EXPECT_NEAR(gg::vec3_length(n), 1.0f, 1e-6f);
    EXPECT_NEAR(n.y, 0.6f, 1e-6f);
    EXPECT_NEAR(n.z, 0.8f, 1e-6f);
}

TEST(Vec3Test, NormalizeZeroVectorReturnsZero) {
    gg::Vec3 v{0.0f, 0.0f, 0.0f};
    auto n = gg::vec3_normalize(v);
    EXPECT_FLOAT_EQ(n.x, 0.0f);
    EXPECT_FLOAT_EQ(n.y, 0.0f);
    EXPECT_FLOAT_EQ(n.z, 0.0f);
}
```

- [ ] **Step 2: Register test in build**

Append to `tests/meson.build` (before the `if enable_scripts` block):

```meson
  test_vec3 = executable('test_vec3',
    'test_vec3.cpp',
    dependencies: [engine_dep, gtest_dep, gtest_main_dep],
  )
  test('vec3', test_vec3)
```

- [ ] **Step 3: Run test to verify it fails**

Run: `meson compile -C builddir && meson test -C builddir test_vec3`
Expected: Compilation error — `math/vec3.h` not found / `vec3_add` not declared

- [ ] **Step 4: Write minimal implementation**

Create `engine/math/vec3.h`:

```cpp
#pragma once

#include "ecs/components.h"

#include <cmath>

namespace gg {

inline Vec3 vec3_add(const Vec3& a, const Vec3& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

inline Vec3 vec3_sub(const Vec3& a, const Vec3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

inline Vec3 vec3_scale(const Vec3& v, float s) {
    return {v.x * s, v.y * s, v.z * s};
}

inline float vec3_dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline Vec3 vec3_cross(const Vec3& a, const Vec3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

inline float vec3_length(const Vec3& v) {
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

inline Vec3 vec3_normalize(const Vec3& v) {
    float len = vec3_length(v);
    if (len < 1e-8f) {
        return {0.0f, 0.0f, 0.0f};
    }
    return vec3_scale(v, 1.0f / len);
}

} // namespace gg
```

- [ ] **Step 5: Run tests to verify they pass**

Run: `meson compile -C builddir && meson test -C builddir test_vec3`
Expected: All 8 tests PASS

- [ ] **Step 6: Commit**

```bash
git add engine/math/vec3.h tests/test_vec3.cpp tests/meson.build
git commit -m "feat: add Vec3 math utilities with tests"
```

---

### Task 2: Mat4 Inverse and Transform Methods

**Files:**
- Modify: `engine/math/mat4.h`
- Create: `tests/test_mat4_inverse.cpp`
- Modify: `tests/meson.build`

- [ ] **Step 1: Write the failing tests**

Create `tests/test_mat4_inverse.cpp`:

```cpp
#include "math/mat4.h"
#include "math/vec3.h"

#include <cmath>
#include <gtest/gtest.h>

TEST(Mat4InverseTest, IdentityInverseIsIdentity) {
    auto m = gg::Mat4::identity();
    auto inv = m.inverse();
    for (int i = 0; i < 16; ++i) {
        EXPECT_NEAR(inv.data[i], m.data[i], 1e-5f);
    }
}

TEST(Mat4InverseTest, TranslationInverse) {
    auto m = gg::Mat4::translation({3.0f, 4.0f, 5.0f});
    auto inv = m.inverse();
    auto result = m * inv;
    auto id = gg::Mat4::identity();
    for (int i = 0; i < 16; ++i) {
        EXPECT_NEAR(result.data[i], id.data[i], 1e-5f);
    }
}

TEST(Mat4InverseTest, PerspectiveInverse) {
    auto m = gg::Mat4::perspective(0.785f, 16.0f / 9.0f, 0.1f, 100.0f);
    auto inv = m.inverse();
    auto result = m * inv;
    auto id = gg::Mat4::identity();
    for (int i = 0; i < 16; ++i) {
        EXPECT_NEAR(result.data[i], id.data[i], 1e-4f);
    }
}

TEST(Mat4InverseTest, LookAtInverse) {
    auto m = gg::Mat4::look_at({0, 0, 5}, {0, 0, 0}, {0, 1, 0});
    auto inv = m.inverse();
    auto result = m * inv;
    auto id = gg::Mat4::identity();
    for (int i = 0; i < 16; ++i) {
        EXPECT_NEAR(result.data[i], id.data[i], 1e-4f);
    }
}

TEST(Mat4InverseTest, ViewProjectionInverse) {
    auto v = gg::Mat4::look_at({3, 4, 5}, {0, 0, 0}, {0, 1, 0});
    auto p = gg::Mat4::perspective(0.785f, 1.5f, 0.1f, 100.0f);
    auto vp = p * v;
    auto inv = vp.inverse();
    auto result = vp * inv;
    auto id = gg::Mat4::identity();
    for (int i = 0; i < 16; ++i) {
        EXPECT_NEAR(result.data[i], id.data[i], 1e-3f);
    }
}

TEST(Mat4TransformTest, TransformPointByTranslation) {
    auto m = gg::Mat4::translation({10.0f, 20.0f, 30.0f});
    auto p = m.transform_point({1.0f, 2.0f, 3.0f});
    EXPECT_NEAR(p.x, 11.0f, 1e-5f);
    EXPECT_NEAR(p.y, 22.0f, 1e-5f);
    EXPECT_NEAR(p.z, 33.0f, 1e-5f);
}

TEST(Mat4TransformTest, TransformDirectionIgnoresTranslation) {
    auto m = gg::Mat4::translation({10.0f, 20.0f, 30.0f});
    auto d = m.transform_direction({1.0f, 0.0f, 0.0f});
    EXPECT_NEAR(d.x, 1.0f, 1e-5f);
    EXPECT_NEAR(d.y, 0.0f, 1e-5f);
    EXPECT_NEAR(d.z, 0.0f, 1e-5f);
}
```

- [ ] **Step 2: Register test in build**

Append to `tests/meson.build` (after test_vec3 block):

```meson
  test_mat4_inverse = executable('test_mat4_inverse',
    'test_mat4_inverse.cpp',
    dependencies: [engine_dep, gtest_dep, gtest_main_dep],
  )
  test('mat4_inverse', test_mat4_inverse)
```

- [ ] **Step 3: Run test to verify it fails**

Run: `meson compile -C builddir && meson test -C builddir test_mat4_inverse`
Expected: Compilation error — `inverse()` not a member of `Mat4`

- [ ] **Step 4: Write minimal implementation**

Add to `engine/math/mat4.h` inside the `Mat4` struct, after `operator*`:

```cpp
    Mat4 inverse() const {
        // Cofactor expansion for general 4x4 inverse
        const float* m = data;
        float inv[16];

        inv[0] = m[5]*m[10]*m[15] - m[5]*m[11]*m[14] - m[9]*m[6]*m[15]
               + m[9]*m[7]*m[14] + m[13]*m[6]*m[11] - m[13]*m[7]*m[10];
        inv[4] = -m[4]*m[10]*m[15] + m[4]*m[11]*m[14] + m[8]*m[6]*m[15]
               - m[8]*m[7]*m[14] - m[12]*m[6]*m[11] + m[12]*m[7]*m[10];
        inv[8] = m[4]*m[9]*m[15] - m[4]*m[11]*m[13] - m[8]*m[5]*m[15]
               + m[8]*m[7]*m[13] + m[12]*m[5]*m[11] - m[12]*m[7]*m[9];
        inv[12] = -m[4]*m[9]*m[14] + m[4]*m[10]*m[13] + m[8]*m[5]*m[14]
                - m[8]*m[6]*m[13] - m[12]*m[5]*m[10] + m[12]*m[6]*m[9];

        float det = m[0]*inv[0] + m[1]*inv[4] + m[2]*inv[8] + m[3]*inv[12];
        if (std::abs(det) < 1e-10f) {
            return identity();
        }

        inv[1] = -m[1]*m[10]*m[15] + m[1]*m[11]*m[14] + m[9]*m[2]*m[15]
               - m[9]*m[3]*m[14] - m[13]*m[2]*m[11] + m[13]*m[3]*m[10];
        inv[5] = m[0]*m[10]*m[15] - m[0]*m[11]*m[14] - m[8]*m[2]*m[15]
               + m[8]*m[3]*m[14] + m[12]*m[2]*m[11] - m[12]*m[3]*m[10];
        inv[9] = -m[0]*m[9]*m[15] + m[0]*m[11]*m[13] + m[8]*m[1]*m[15]
               - m[8]*m[3]*m[13] - m[12]*m[1]*m[11] + m[12]*m[3]*m[9];
        inv[13] = m[0]*m[9]*m[14] - m[0]*m[10]*m[13] - m[8]*m[1]*m[14]
                + m[8]*m[2]*m[13] + m[12]*m[1]*m[10] - m[12]*m[2]*m[9];

        inv[2] = m[1]*m[6]*m[15] - m[1]*m[7]*m[14] - m[5]*m[2]*m[15]
               + m[5]*m[3]*m[14] + m[13]*m[2]*m[7] - m[13]*m[3]*m[6];
        inv[6] = -m[0]*m[6]*m[15] + m[0]*m[7]*m[14] + m[4]*m[2]*m[15]
               - m[4]*m[3]*m[14] - m[12]*m[2]*m[7] + m[12]*m[3]*m[6];
        inv[10] = m[0]*m[5]*m[15] - m[0]*m[7]*m[13] - m[4]*m[1]*m[15]
                + m[4]*m[3]*m[13] + m[12]*m[1]*m[7] - m[12]*m[3]*m[5];
        inv[14] = -m[0]*m[5]*m[14] + m[0]*m[6]*m[13] + m[4]*m[1]*m[14]
                - m[4]*m[2]*m[13] - m[12]*m[1]*m[6] + m[12]*m[2]*m[5];

        inv[3] = -m[1]*m[6]*m[11] + m[1]*m[7]*m[10] + m[5]*m[2]*m[11]
               - m[5]*m[3]*m[10] - m[9]*m[2]*m[7] + m[9]*m[3]*m[6];
        inv[7] = m[0]*m[6]*m[11] - m[0]*m[7]*m[10] - m[4]*m[2]*m[11]
               + m[4]*m[3]*m[10] + m[8]*m[2]*m[7] - m[8]*m[3]*m[6];
        inv[11] = -m[0]*m[5]*m[11] + m[0]*m[7]*m[9] + m[4]*m[1]*m[11]
                - m[4]*m[3]*m[9] - m[8]*m[1]*m[7] + m[8]*m[3]*m[5];
        inv[15] = m[0]*m[5]*m[10] - m[0]*m[6]*m[9] - m[4]*m[1]*m[10]
                + m[4]*m[2]*m[9] + m[8]*m[1]*m[6] - m[8]*m[2]*m[5];

        float inv_det = 1.0f / det;
        Mat4 result{};
        for (int i = 0; i < 16; ++i) {
            result.data[i] = inv[i] * inv_det;
        }
        return result;
    }

    Vec3 transform_point(const Vec3& p) const {
        float x = data[0]*p.x + data[4]*p.y + data[8]*p.z + data[12];
        float y = data[1]*p.x + data[5]*p.y + data[9]*p.z + data[13];
        float z = data[2]*p.x + data[6]*p.y + data[10]*p.z + data[14];
        float w = data[3]*p.x + data[7]*p.y + data[11]*p.z + data[15];
        if (std::abs(w) > 1e-8f) {
            x /= w;
            y /= w;
            z /= w;
        }
        return {x, y, z};
    }

    Vec3 transform_direction(const Vec3& d) const {
        float x = data[0]*d.x + data[4]*d.y + data[8]*d.z;
        float y = data[1]*d.x + data[5]*d.y + data[9]*d.z;
        float z = data[2]*d.x + data[6]*d.y + data[10]*d.z;
        return {x, y, z};
    }
```

- [ ] **Step 5: Run tests to verify they pass**

Run: `meson compile -C builddir && meson test -C builddir test_mat4_inverse`
Expected: All 7 tests PASS

- [ ] **Step 6: Commit**

```bash
git add engine/math/mat4.h tests/test_mat4_inverse.cpp tests/meson.build
git commit -m "feat: add Mat4 inverse and transform_point/direction"
```

---

### Task 3: Ray Utilities

**Files:**
- Create: `engine/math/ray.h`
- Create: `tests/test_ray.cpp`
- Modify: `tests/meson.build`

- [ ] **Step 1: Write the failing tests**

Create `tests/test_ray.cpp`:

```cpp
#include "math/ray.h"
#include "math/mat4.h"
#include "math/vec3.h"

#include <cmath>
#include <gtest/gtest.h>
#include <numbers>

TEST(RayTest, ScreenCenterPointsForward) {
    // Camera at (0,0,5) looking at origin
    auto v = gg::Mat4::look_at({0, 0, 5}, {0, 0, 0}, {0, 1, 0});
    auto p = gg::Mat4::perspective(
        45.0f * std::numbers::pi_v<float> / 180.0f, 1.0f, 0.1f, 100.0f);
    auto vp = p * v;
    auto inv_vp = vp.inverse();

    auto ray = gg::ray_from_screen(400.0f, 300.0f, 800, 600, inv_vp);

    // Origin should be near camera position (0, 0, 5)
    EXPECT_NEAR(ray.origin.x, 0.0f, 0.1f);
    EXPECT_NEAR(ray.origin.y, 0.0f, 0.1f);
    EXPECT_NEAR(ray.origin.z, 5.0f, 0.5f);

    // Direction should point toward -Z (toward origin from z=5)
    EXPECT_NEAR(ray.direction.z, -1.0f, 0.1f);
    EXPECT_NEAR(gg::vec3_length(ray.direction), 1.0f, 1e-5f);
}

TEST(RayTest, AxisDistanceParallelRayAndAxis) {
    // Ray along X at y=1, axis along X at origin
    gg::Ray ray{{0, 1, 0}, {1, 0, 0}};
    float t = 0.0f;
    float dist = gg::ray_axis_distance(ray, {0, 0, 0}, {1, 0, 0}, t);
    EXPECT_NEAR(dist, 1.0f, 1e-5f);
}

TEST(RayTest, AxisDistanceIntersectingRayAndAxis) {
    // Ray from (0, 1, 0) pointing down toward X-axis
    gg::Ray ray{{0, 1, 0}, gg::vec3_normalize({0, -1, 0})};
    float t = 0.0f;
    float dist = gg::ray_axis_distance(ray, {0, 0, 0}, {1, 0, 0}, t);
    EXPECT_NEAR(dist, 0.0f, 1e-4f);
    EXPECT_NEAR(t, 0.0f, 1e-4f);
}

TEST(RayTest, AxisDistanceSkewLines) {
    // Ray from (0, 2, 0) pointing along Z, axis along X at origin
    gg::Ray ray{{0, 2, 0}, {0, 0, 1}};
    float t = 0.0f;
    float dist = gg::ray_axis_distance(ray, {0, 0, 0}, {1, 0, 0}, t);
    EXPECT_NEAR(dist, 2.0f, 1e-5f);
    EXPECT_NEAR(t, 0.0f, 1e-4f);
}

TEST(RayTest, AxisClosestPointOnAxis) {
    // Ray from (3, 1, 0) pointing along -Y, axis along X at origin
    gg::Ray ray{{3, 1, 0}, {0, -1, 0}};
    float t = 0.0f;
    float dist = gg::ray_axis_distance(ray, {0, 0, 0}, {1, 0, 0}, t);
    EXPECT_NEAR(dist, 0.0f, 1e-4f);
    EXPECT_NEAR(t, 3.0f, 1e-4f);  // closest point is at x=3 on the axis
}
```

- [ ] **Step 2: Register test in build**

Append to `tests/meson.build` (after test_mat4_inverse block):

```meson
  test_ray = executable('test_ray',
    'test_ray.cpp',
    dependencies: [engine_dep, gtest_dep, gtest_main_dep],
  )
  test('ray', test_ray)
```

- [ ] **Step 3: Run test to verify it fails**

Run: `meson compile -C builddir && meson test -C builddir test_ray`
Expected: Compilation error — `math/ray.h` not found

- [ ] **Step 4: Write minimal implementation**

Create `engine/math/ray.h`:

```cpp
#pragma once

#include "ecs/components.h"
#include "math/mat4.h"
#include "math/vec3.h"

#include <cmath>
#include <cstdint>

namespace gg {

struct Ray {
    Vec3 origin;
    Vec3 direction; // normalized
};

/// Convert screen pixel coordinates to a world-space ray.
/// screen_x, screen_y: pixel coordinates (0,0 = top-left)
/// fb_width, fb_height: framebuffer dimensions
/// inverse_vp: inverse of (projection * view) matrix
inline Ray ray_from_screen(float screen_x, float screen_y,
                           uint32_t fb_width, uint32_t fb_height,
                           const Mat4& inverse_vp) {
    // Convert pixel to NDC [-1, 1]
    float ndc_x = (2.0f * screen_x / float(fb_width)) - 1.0f;
    float ndc_y = 1.0f - (2.0f * screen_y / float(fb_height)); // flip Y

    // Near and far points in NDC
    Vec3 near_ndc{ndc_x, ndc_y, 0.0f};
    Vec3 far_ndc{ndc_x, ndc_y, 1.0f};

    // Unproject to world space
    Vec3 near_world = inverse_vp.transform_point(near_ndc);
    Vec3 far_world = inverse_vp.transform_point(far_ndc);

    Vec3 dir = vec3_normalize(vec3_sub(far_world, near_world));
    return {near_world, dir};
}

/// Compute the minimum distance between a ray and an infinite line (axis).
/// axis_origin: a point on the line
/// axis_dir: direction of the line (must be normalized)
/// out_t: parameter along the axis line for the closest point
///         closest_point = axis_origin + axis_dir * out_t
/// Returns the minimum distance between the ray and the line.
inline float ray_axis_distance(const Ray& ray, const Vec3& axis_origin,
                               const Vec3& axis_dir, float& out_t) {
    // Using closest point between two lines formula:
    // Line 1 (ray):  P = ray.origin + ray.direction * s
    // Line 2 (axis): Q = axis_origin + axis_dir * t
    // w0 = ray.origin - axis_origin
    Vec3 w0 = vec3_sub(ray.origin, axis_origin);
    float a = vec3_dot(ray.direction, ray.direction);   // always 1 if normalized
    float b = vec3_dot(ray.direction, axis_dir);
    float c = vec3_dot(axis_dir, axis_dir);             // always 1 if normalized
    float d = vec3_dot(ray.direction, w0);
    float e = vec3_dot(axis_dir, w0);

    float denom = a * c - b * b;

    float s;
    if (std::abs(denom) < 1e-8f) {
        // Lines are parallel
        s = 0.0f;
        out_t = e / c;
    } else {
        s = (b * e - c * d) / denom;
        out_t = (a * e - b * d) / denom;
    }

    // Clamp s to >= 0 (ray, not line)
    if (s < 0.0f) {
        s = 0.0f;
        out_t = e / c;
    }

    // Points on each line at closest approach
    Vec3 p1 = vec3_add(ray.origin, vec3_scale(ray.direction, s));
    Vec3 p2 = vec3_add(axis_origin, vec3_scale(axis_dir, out_t));

    return vec3_length(vec3_sub(p1, p2));
}

} // namespace gg
```

- [ ] **Step 5: Run tests to verify they pass**

Run: `meson compile -C builddir && meson test -C builddir test_ray`
Expected: All 5 tests PASS

- [ ] **Step 6: Commit**

```bash
git add engine/math/ray.h tests/test_ray.cpp tests/meson.build
git commit -m "feat: add Ray type with screen-to-world and axis distance"
```

---

### Task 4: Camera Getters

**Files:**
- Modify: `engine/renderer/camera.h`
- Modify: `engine/renderer/camera.cpp`

Camera currently exposes only matrix getters. GizmoInteraction needs `eye_position()` for the ray origin check and `fov()` + camera distance for screen-space constant scaling.

- [ ] **Step 1: Add getters to camera.h**

In `engine/renderer/camera.h`, add to the public section after `view_projection_matrix()`:

```cpp
    [[nodiscard]] Vec3 eye_position() const;
    [[nodiscard]] float fov() const;
```

- [ ] **Step 2: Implement in camera.cpp**

Add to `engine/renderer/camera.cpp` after `view_projection_matrix()`:

```cpp
Vec3 Camera::eye_position() const {
    float cos_p = std::cos(pitch_);
    float sin_p = std::sin(pitch_);
    float cos_y = std::cos(yaw_);
    float sin_y = std::sin(yaw_);
    return {
        target_.x + distance_ * cos_p * sin_y,
        target_.y + distance_ * sin_p,
        target_.z + distance_ * cos_p * cos_y,
    };
}

float Camera::fov() const {
    return fov_;
}
```

- [ ] **Step 3: Verify build passes**

Run: `meson compile -C builddir && meson test -C builddir`
Expected: All existing tests still PASS

- [ ] **Step 4: Commit**

```bash
git add engine/renderer/camera.h engine/renderer/camera.cpp
git commit -m "feat: add eye_position() and fov() getters to Camera"
```

---

### Task 5: GizmoInteraction Class

**Files:**
- Create: `engine/editor/gizmo_interaction.h`
- Create: `engine/editor/gizmo_interaction.cpp`
- Create: `tests/test_gizmo_interaction.cpp`
- Modify: `engine/meson.build`
- Modify: `tests/meson.build`

- [ ] **Step 1: Write the failing tests**

Create `tests/test_gizmo_interaction.cpp`:

```cpp
#include "editor/gizmo_interaction.h"
#include "math/vec3.h"
#include "renderer/camera.h"

#include <gtest/gtest.h>

class GizmoInteractionTest : public ::testing::Test {
protected:
    void SetUp() override {
        gi_ = gg::GizmoInteraction::create();
        cam_ = gg::Camera::create();
        cam_->set_aspect(800.0f / 600.0f);
    }

    // Simulate a frame update at given screen coords
    void update(float mx, float my, bool down) {
        gi_->update(mx, my, down, 800, 600, *cam_, gizmo_pos_, gizmo_scale_);
    }

    std::unique_ptr<gg::GizmoInteraction> gi_;
    std::unique_ptr<gg::Camera> cam_;
    gg::Vec3 gizmo_pos_{0.0f, 0.0f, 0.0f};
    float gizmo_scale_ = 1.0f;
};

TEST_F(GizmoInteractionTest, InitialStateIsIdle) {
    auto s = gi_->state();
    EXPECT_EQ(s.hovered_axis, -1);
    EXPECT_EQ(s.dragging_axis, -1);
}

TEST_F(GizmoInteractionTest, NoHoverOnEmptySpace) {
    // Click far from gizmo — top-left corner
    update(0.0f, 0.0f, false);
    EXPECT_EQ(gi_->state().hovered_axis, -1);
}

TEST_F(GizmoInteractionTest, DragStartSetsAxis) {
    // Default camera is at approx (0, 1.5, 4.8) looking at origin.
    // The X axis arrow extends along +X from the origin.
    // Screen center (400, 300) should be near the gizmo origin.

    // Move mouse slightly right of center (along +X axis in screen space)
    // First hover pass
    update(440.0f, 300.0f, false);
    int hovered = gi_->state().hovered_axis;

    // We can't know the exact hover result without precise camera math,
    // but if hovered, a click should start dragging
    if (hovered >= 0) {
        update(440.0f, 300.0f, true);
        EXPECT_EQ(gi_->state().dragging_axis, hovered);
    }
}

TEST_F(GizmoInteractionTest, ReleaseEndsDrag) {
    // Simulate: hover → click → release
    update(440.0f, 300.0f, false);
    int hovered = gi_->state().hovered_axis;
    if (hovered >= 0) {
        update(440.0f, 300.0f, true);
        EXPECT_EQ(gi_->state().dragging_axis, hovered);
        update(440.0f, 300.0f, false);
        EXPECT_EQ(gi_->state().dragging_axis, -1);
    }
}

TEST_F(GizmoInteractionTest, PositionDeltaIsZeroWhenNotDragging) {
    update(0.0f, 0.0f, false);
    auto d = gi_->position_delta();
    EXPECT_FLOAT_EQ(d.x, 0.0f);
    EXPECT_FLOAT_EQ(d.y, 0.0f);
    EXPECT_FLOAT_EQ(d.z, 0.0f);
}
```

- [ ] **Step 2: Register in build files**

Append to `engine/meson.build` engine_sources (after `'renderer/gizmo_renderer.cpp'`):

```meson
  'editor/gizmo_interaction.cpp',
```

Append to `tests/meson.build` (after test_ray block):

```meson
  test_gizmo_interaction = executable('test_gizmo_interaction',
    'test_gizmo_interaction.cpp',
    dependencies: [engine_dep, gtest_dep, gtest_main_dep],
  )
  test('gizmo_interaction', test_gizmo_interaction)
```

- [ ] **Step 3: Run test to verify it fails**

Run: `meson compile -C builddir && meson test -C builddir test_gizmo_interaction`
Expected: Compilation error — `editor/gizmo_interaction.h` not found

- [ ] **Step 4: Write the header**

Create `engine/editor/gizmo_interaction.h`:

```cpp
#pragma once

#include "ecs/components.h"

#include <memory>

namespace gg {

class Camera;

struct GizmoState {
    int hovered_axis = -1;   // -1=none, 0=X, 1=Y, 2=Z
    int dragging_axis = -1;  // -1=none, 0=X, 1=Y, 2=Z
    Vec3 drag_start_pos{};   // entity position when drag started
};

class GizmoInteraction {
public:
    static std::unique_ptr<GizmoInteraction> create();
    ~GizmoInteraction();

    GizmoInteraction(const GizmoInteraction&) = delete;
    GizmoInteraction& operator=(const GizmoInteraction&) = delete;

    /// Call once per frame with current input + camera + gizmo position.
    void update(float mouse_x, float mouse_y, bool mouse_down,
                uint32_t fb_width, uint32_t fb_height,
                const Camera& camera,
                const Vec3& gizmo_position, float gizmo_scale);

    [[nodiscard]] GizmoState state() const;
    [[nodiscard]] Vec3 position_delta() const;

private:
    GizmoInteraction() = default;

    GizmoState state_{};
    Vec3 frame_delta_{};
    Vec3 last_closest_point_{};
    bool was_mouse_down_ = false;
};

} // namespace gg
```

- [ ] **Step 5: Write the implementation**

Create `engine/editor/gizmo_interaction.cpp`:

```cpp
#include "editor/gizmo_interaction.h"

#include "math/ray.h"
#include "math/vec3.h"
#include "renderer/camera.h"

#include <array>
#include <cmath>

namespace gg {

static constexpr float HIT_THRESHOLD_FACTOR = 0.15f;

// Axis directions (unit vectors)
static constexpr std::array<Vec3, 3> AXIS_DIRS = {{
    {1.0f, 0.0f, 0.0f},
    {0.0f, 1.0f, 0.0f},
    {0.0f, 0.0f, 1.0f},
}};

std::unique_ptr<GizmoInteraction> GizmoInteraction::create() {
    return std::unique_ptr<GizmoInteraction>(new GizmoInteraction());
}

GizmoInteraction::~GizmoInteraction() = default;

void GizmoInteraction::update(float mouse_x, float mouse_y, bool mouse_down,
                              uint32_t fb_width, uint32_t fb_height,
                              const Camera& camera,
                              const Vec3& gizmo_position, float gizmo_scale) {
    frame_delta_ = {0.0f, 0.0f, 0.0f};
    bool just_pressed = mouse_down && !was_mouse_down_;
    bool just_released = !mouse_down && was_mouse_down_;
    was_mouse_down_ = mouse_down;

    Mat4 vp = camera.view_projection_matrix();
    Mat4 inv_vp = vp.inverse();
    Ray ray = ray_from_screen(mouse_x, mouse_y, fb_width, fb_height, inv_vp);

    float threshold = gizmo_scale * HIT_THRESHOLD_FACTOR;

    if (state_.dragging_axis >= 0) {
        // Currently dragging
        if (just_released) {
            state_.dragging_axis = -1;
            state_.hovered_axis = -1;
            return;
        }

        int axis = state_.dragging_axis;
        const Vec3& axis_dir = AXIS_DIRS[axis];
        float t = 0.0f;
        ray_axis_distance(ray, gizmo_position, axis_dir, t);
        Vec3 closest = vec3_add(gizmo_position, vec3_scale(axis_dir, t));
        frame_delta_ = vec3_sub(closest, last_closest_point_);
        last_closest_point_ = closest;
        return;
    }

    // Not dragging — do hover detection
    int best_axis = -1;
    float best_dist = threshold;

    for (int i = 0; i < 3; ++i) {
        float t = 0.0f;
        float dist = ray_axis_distance(ray, gizmo_position, AXIS_DIRS[i], t);

        // Only consider hits within the arrow length (0 to shaft+cone scaled)
        float arrow_len = 1.5f * gizmo_scale;
        if (t < 0.0f || t > arrow_len) {
            continue;
        }

        if (dist < best_dist) {
            best_dist = dist;
            best_axis = i;
        }
    }

    state_.hovered_axis = best_axis;

    if (just_pressed && best_axis >= 0) {
        state_.dragging_axis = best_axis;
        state_.drag_start_pos = gizmo_position;

        // Compute initial closest point on axis
        float t = 0.0f;
        ray_axis_distance(ray, gizmo_position, AXIS_DIRS[best_axis], t);
        last_closest_point_ = vec3_add(gizmo_position, vec3_scale(AXIS_DIRS[best_axis], t));
    }
}

GizmoState GizmoInteraction::state() const {
    return state_;
}

Vec3 GizmoInteraction::position_delta() const {
    return frame_delta_;
}

} // namespace gg
```

- [ ] **Step 6: Run tests to verify they pass**

Run: `meson compile -C builddir && meson test -C builddir test_gizmo_interaction`
Expected: All 5 tests PASS

- [ ] **Step 7: Commit**

```bash
git add engine/editor/gizmo_interaction.h engine/editor/gizmo_interaction.cpp \
        tests/test_gizmo_interaction.cpp engine/meson.build tests/meson.build
git commit -m "feat: add GizmoInteraction with hover/drag state machine"
```

---

### Task 6: GizmoRenderer Extension (Highlight + Scale)

**Files:**
- Modify: `engine/renderer/gizmo_renderer.h`
- Modify: `engine/renderer/gizmo_renderer.cpp`

This task modifies GPU rendering code — no unit tests for visual output.

- [ ] **Step 1: Update the header**

In `engine/renderer/gizmo_renderer.h`, change the `draw` declaration:

```cpp
    void draw(const Vec3& position, const Camera& camera, WGPURenderPassEncoder pass,
              float scale = 1.0f, int hovered_axis = -1, int dragging_axis = -1);
```

- [ ] **Step 2: Update the implementation**

In `engine/renderer/gizmo_renderer.cpp`:

**a)** Change the `build_arrow` signature to accept a scale parameter. Replace the existing `build_arrow` function:

```cpp
static void build_arrow(
    std::vector<GizmoVertex>& out, float px, float py, float pz,
    int axis, const float color[3], float scale) {
    float shaft_length = SHAFT_LENGTH * scale;
    float cone_length = CONE_LENGTH * scale;
    float shaft_radius = SHAFT_RADIUS * scale;
    float cone_radius = CONE_RADIUS * scale;

    auto make_point = [&](float along, float r1, float r2) -> GizmoVertex {
        float pos[3] = {px, py, pz};
        int p1 = (axis + 1) % 3;
        int p2 = (axis + 2) % 3;
        pos[axis] += along;
        pos[p1] += r1;
        pos[p2] += r2;
        return {{pos[0], pos[1], pos[2]}, {color[0], color[1], color[2]}};
    };

    constexpr float tau = 2.0f * std::numbers::pi_v<float>;

    for (int i = 0; i < SEGMENTS; ++i) {
        float a0 = tau * float(i) / float(SEGMENTS);
        float a1 = tau * float(i + 1) / float(SEGMENTS);
        float c0 = std::cos(a0) * shaft_radius;
        float s0 = std::sin(a0) * shaft_radius;
        float c1 = std::cos(a1) * shaft_radius;
        float s1 = std::sin(a1) * shaft_radius;

        out.push_back(make_point(0.0f, c0, s0));
        out.push_back(make_point(shaft_length, c0, s0));
        out.push_back(make_point(shaft_length, c1, s1));

        out.push_back(make_point(0.0f, c0, s0));
        out.push_back(make_point(shaft_length, c1, s1));
        out.push_back(make_point(0.0f, c1, s1));
    }

    float tip = shaft_length + cone_length;
    for (int i = 0; i < SEGMENTS; ++i) {
        float a0 = tau * float(i) / float(SEGMENTS);
        float a1 = tau * float(i + 1) / float(SEGMENTS);
        float c0 = std::cos(a0) * cone_radius;
        float s0 = std::sin(a0) * cone_radius;
        float c1 = std::cos(a1) * cone_radius;
        float s1 = std::sin(a1) * cone_radius;

        out.push_back(make_point(shaft_length, c0, s0));
        out.push_back(make_point(tip, 0.0f, 0.0f));
        out.push_back(make_point(shaft_length, c1, s1));
    }
}
```

**b)** Update the `draw` method to use highlighting colors and scale:

```cpp
void GizmoRenderer::draw(const Vec3& position, const Camera& camera, WGPURenderPassEncoder pass,
                          float scale, int hovered_axis, int dragging_axis) {
    const float px = position.x;
    const float py = position.y;
    const float pz = position.z;

    std::vector<GizmoVertex> vertices;
    vertices.reserve(VERTEX_COUNT);

    // Base colors
    const float base_colors[3][3] = {
        {0.9f, 0.2f, 0.2f},  // X red
        {0.2f, 0.9f, 0.2f},  // Y green
        {0.3f, 0.3f, 1.0f},  // Z blue
    };
    const float drag_color[3] = {1.0f, 1.0f, 0.3f};
    const float dim_factor = 0.4f;
    const float hover_boost = 0.15f;

    for (int axis = 0; axis < 3; ++axis) {
        float color[3];
        if (dragging_axis >= 0) {
            if (axis == dragging_axis) {
                color[0] = drag_color[0];
                color[1] = drag_color[1];
                color[2] = drag_color[2];
            } else {
                color[0] = base_colors[axis][0] * dim_factor;
                color[1] = base_colors[axis][1] * dim_factor;
                color[2] = base_colors[axis][2] * dim_factor;
            }
        } else if (axis == hovered_axis) {
            color[0] = std::min(base_colors[axis][0] + hover_boost, 1.0f);
            color[1] = std::min(base_colors[axis][1] + hover_boost, 1.0f);
            color[2] = std::min(base_colors[axis][2] + hover_boost, 1.0f);
        } else {
            color[0] = base_colors[axis][0];
            color[1] = base_colors[axis][1];
            color[2] = base_colors[axis][2];
        }
        build_arrow(vertices, px, py, pz, axis, color, scale);
    }

    wgpuQueueWriteBuffer(
        ctx_.queue(), vertex_buffer_, 0, vertices.data(), vertices.size() * sizeof(GizmoVertex));

    Mat4 vp = camera.view_projection_matrix();
    wgpuQueueWriteBuffer(ctx_.queue(), uniform_buffer_, 0, &vp, sizeof(Mat4));

    wgpuRenderPassEncoderSetPipeline(pass, pipeline_);
    wgpuRenderPassEncoderSetBindGroup(pass, 0, bind_group_, 0, nullptr);
    wgpuRenderPassEncoderSetVertexBuffer(pass, 0, vertex_buffer_, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderDraw(pass, static_cast<uint32_t>(vertices.size()), 1, 0, 0);
}
```

- [ ] **Step 3: Verify build passes**

Run: `meson compile -C builddir && meson test -C builddir`
Expected: Build succeeds, all existing tests PASS

- [ ] **Step 4: Commit**

```bash
git add engine/renderer/gizmo_renderer.h engine/renderer/gizmo_renderer.cpp
git commit -m "feat: extend GizmoRenderer with highlight colors and scale"
```

---

### Task 7: Engine Loop Integration

**Files:**
- Modify: `engine/core/engine.h`
- Modify: `engine/core/engine.cpp`

- [ ] **Step 1: Add member to engine.h**

In `engine/core/engine.h`, add forward declaration alongside existing ones:

```cpp
class GizmoInteraction;
```

Add member alongside existing unique_ptrs:

```cpp
    std::unique_ptr<GizmoInteraction> gizmo_interaction_;
```

- [ ] **Step 2: Add include to engine.cpp**

At the top of `engine/core/engine.cpp`, add:

```cpp
#include "editor/gizmo_interaction.h"
#include "math/vec3.h"
```

- [ ] **Step 3: Create GizmoInteraction in Engine::create()**

In `engine/core/engine.cpp`, wherever `gizmo_renderer_` is created (two places — project boot and direct model boot), add immediately after:

```cpp
                engine->gizmo_interaction_ = GizmoInteraction::create();
```

- [ ] **Step 4: Add interaction logic to run() loop**

In `engine/core/engine.cpp`, replace the camera orbit block (lines ~239-251) with the following. The key change: gizmo interaction runs first, and camera orbit is skipped while dragging.

```cpp
        // Gizmo interaction + camera orbit
        float mx = window_->mouse_x();
        float my = window_->mouse_y();
        bool mouse_down = window_->mouse_button(0);

        if (gizmo_interaction_ && camera_ && gizmo_renderer_) {
            // Find closest gizmo across all renderable entities
            struct GizmoTarget {
                flecs::entity entity;
                Vec3 position;
                float distance;
            };
            GizmoTarget best_target{};
            best_target.distance = 1e30f;
            bool found_target = false;

            Vec3 eye = camera_->eye_position();

            // Screen-space constant scale
            float cam_dist_to_origin = vec3_length(vec3_sub(eye, {0, 0, 0}));
            float gizmo_scale = cam_dist_to_origin * std::tan(camera_->fov() / 2.0f) * 0.12f;

            if (!mesh_assets_.empty()) {
                world_->raw().each([&](flecs::entity e,
                                       const Transform& transform,
                                       const Renderable& /*r*/) {
                    float d = vec3_length(vec3_sub(eye, transform.position));
                    if (d < best_target.distance) {
                        best_target.entity = e;
                        best_target.position = transform.position;
                        best_target.distance = d;
                        found_target = true;
                    }
                });
            }

            if (found_target) {
                gizmo_scale = best_target.distance * std::tan(camera_->fov() / 2.0f) * 0.12f;
                gizmo_interaction_->update(mx, my, mouse_down,
                    window_->framebuffer_width(), window_->framebuffer_height(),
                    *camera_, best_target.position, gizmo_scale);

                // Apply drag delta to entity transform
                Vec3 delta = gizmo_interaction_->position_delta();
                if (gizmo_interaction_->state().dragging_axis >= 0) {
                    auto* t = best_target.entity.get_mut<Transform>();
                    if (t != nullptr) {
                        t->position = vec3_add(t->position, delta);

                        const auto* rb = best_target.entity.get<RigidBody>();
                        if (rb != nullptr) {
                            best_target.entity.set<RigidBody>({
                                .body_id = rb->body_id,
                                .sync_to_physics = true,
                            });
                        }
                    }
                }
            } else {
                // No target — reset interaction state
                gizmo_interaction_->update(mx, my, false, 0, 0, *camera_, {}, 1.0f);
            }
        }

        // Camera orbit (skip while dragging gizmo)
        bool is_dragging_gizmo = gizmo_interaction_ &&
                                  gizmo_interaction_->state().dragging_axis >= 0;
        if (camera_ && !is_dragging_gizmo) {
            static float last_mx = window_->mouse_x();
            static float last_my = window_->mouse_y();
            if (window_->mouse_button(0)) {
                camera_->orbit(mx - last_mx, my - last_my);
            }
            camera_->zoom(window_->scroll_delta_y());
            window_->reset_scroll();
            last_mx = mx;
            last_my = my;
        } else if (camera_) {
            // Still need to track mouse position and reset scroll
            static float last_mx = mx;
            static float last_my = my;
            last_mx = mx;
            last_my = my;
            camera_->zoom(window_->scroll_delta_y());
            window_->reset_scroll();
        }
```

- [ ] **Step 5: Pass gizmo state to renderer**

In `engine/core/engine.cpp`, update the gizmo rendering block (currently around lines 314-320). Replace:

```cpp
            if (gizmo_renderer_ && camera_ && !mesh_assets_.empty()) {
                world_->raw().each([&](flecs::entity /*e*/,
                                       const Transform& transform,
                                       const Renderable& /*r*/) {
                    gizmo_renderer_->draw(transform.position, *camera_, renderer_->render_pass());
                });
            }
```

With:

```cpp
            if (gizmo_renderer_ && camera_ && !mesh_assets_.empty()) {
                Vec3 eye = camera_->eye_position();
                int hovered = gizmo_interaction_ ? gizmo_interaction_->state().hovered_axis : -1;
                int dragging = gizmo_interaction_ ? gizmo_interaction_->state().dragging_axis : -1;

                world_->raw().each([&](flecs::entity /*e*/,
                                       const Transform& transform,
                                       const Renderable& /*r*/) {
                    float dist = vec3_length(vec3_sub(eye, transform.position));
                    float s = dist * std::tan(camera_->fov() / 2.0f) * 0.12f;
                    gizmo_renderer_->draw(transform.position, *camera_,
                                          renderer_->render_pass(), s, hovered, dragging);
                });
            }
```

- [ ] **Step 6: Verify build and tests**

Run: `meson compile -C builddir && meson test -C builddir`
Expected: Build succeeds, all tests PASS

- [ ] **Step 7: Manual verification**

Run: `./builddir/game-gym --scene scenes/cube-test.scene.json`
Expected:
- Gizmo arrows maintain constant screen size as camera zooms
- Hovering over an axis arrow brightens it
- Clicking and dragging an axis moves the cube along that axis
- Camera orbit is suppressed while dragging
- Releasing mouse stops the drag

- [ ] **Step 8: Commit**

```bash
git add engine/core/engine.h engine/core/engine.cpp
git commit -m "feat: integrate gizmo drag interaction into engine loop"
```

---

## Self-Review Checklist

### Spec Coverage

| Spec Section | Task |
|-------------|------|
| Vec3 utilities | Task 1 |
| Mat4 inverse, transform_point, transform_direction | Task 2 |
| Ray struct, ray_from_screen, ray_axis_distance | Task 3 |
| Camera getters | Task 4 |
| GizmoInteraction state machine | Task 5 |
| Hover/drag colors | Task 6 |
| Screen-space constant sizing | Task 6 + 7 |
| Engine loop integration | Task 7 |
| Camera orbit suppression | Task 7 |
| RigidBody sync | Task 7 |
| Entity selection (closest gizmo) | Task 7 |
| Test coverage (math + interaction) | Tasks 1-3, 5 |

### Type Consistency

- `Vec3` from `ecs/components.h` — used consistently everywhere
- `Mat4` from `math/mat4.h` — inverse/transform_point/direction added in Task 2, used in Tasks 3, 5, 7
- `Ray` from `math/ray.h` — created in Task 3, used in Task 5
- `GizmoState` from `editor/gizmo_interaction.h` — created in Task 5, queried in Task 7
- `Camera::eye_position()` and `fov()` — added in Task 4, used in Task 7
- `GizmoRenderer::draw()` signature — extended in Task 6, called with new args in Task 7
- `ray_from_screen()` — defined in Task 3 with `(float, float, uint32_t, uint32_t, const Mat4&)`, called identically in Task 5
- `ray_axis_distance()` — defined in Task 3 with `(const Ray&, const Vec3&, const Vec3&, float&)`, called identically in Task 5
