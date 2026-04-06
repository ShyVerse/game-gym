#include "editor/gizmo_interaction.h"

#include "math/ray.h"
#include "math/vec3.h"
#include "renderer/camera.h"
#include "renderer/gizmo_constants.h"

#include <array>
#include <cmath>

namespace gg {

static constexpr float HIT_THRESHOLD_FACTOR = 0.35f;
static constexpr float PLANE_QUAD_RATIO = 0.3f;  // plane quad at 30% of arrow length
static constexpr float CENTER_HIT_RATIO = 0.12f; // center cube hit radius

// Plane normals: XY→Z, XZ→Y, YZ→X
static constexpr std::array<Vec3, 3> PLANE_NORMALS = {{
    {0.0f, 0.0f, 1.0f}, // XY plane
    {0.0f, 1.0f, 0.0f}, // XZ plane
    {1.0f, 0.0f, 0.0f}, // YZ plane
}};

// For each plane, which two axes define it: XY={0,1}, XZ={0,2}, YZ={1,2}
static constexpr int PLANE_AXES[3][2] = {{0, 1}, {0, 2}, {1, 2}};

std::unique_ptr<GizmoInteraction> GizmoInteraction::create() {
    return std::unique_ptr<GizmoInteraction>(new GizmoInteraction());
}

/// Compute the ray-plane hit point for dragging on a plane or free movement.
static bool
compute_plane_hit(const Ray& ray, const Vec3& origin, const Vec3& normal, Vec3& out_point) {
    float t = 0.0f;
    if (!ray_plane_intersect(ray, origin, normal, t)) {
        return false;
    }
    out_point = vec3_add(ray.origin, vec3_scale(ray.direction, t));
    return true;
}

void GizmoInteraction::update(float mouse_x,
                              float mouse_y,
                              bool mouse_down,
                              uint32_t win_width,
                              uint32_t win_height,
                              const Camera& camera,
                              const Vec3& gizmo_position,
                              float gizmo_scale) {
    frame_delta_ = {0.0f, 0.0f, 0.0f};
    bool just_pressed = mouse_down && !was_mouse_down_;
    bool just_released = !mouse_down && was_mouse_down_;
    was_mouse_down_ = mouse_down;

    Mat4 vp = camera.view_projection_matrix();
    Mat4 inv_vp = vp.inverse();
    Ray ray = ray_from_screen(mouse_x, mouse_y, win_width, win_height, inv_vp);

    float threshold = gizmo_scale * HIT_THRESHOLD_FACTOR;

    // --- Active drag continuation ---
    if (state_.dragging_axis >= 0) {
        if (just_released) {
            state_.dragging_axis = -1;
            state_.hovered_axis = -1;
            return;
        }

        int drag = state_.dragging_axis;

        if (drag <= 2) {
            // Single axis drag
            const Vec3& axis_dir = gizmo::AXIS_DIRS[drag];
            float t = 0.0f;
            ray_axis_distance(ray, state_.drag_start_pos, axis_dir, t);
            Vec3 closest = vec3_add(state_.drag_start_pos, vec3_scale(axis_dir, t));
            frame_delta_ = vec3_sub(closest, last_hit_point_);
            last_hit_point_ = closest;
        } else if (drag <= 5) {
            // Plane drag (3=XY, 4=XZ, 5=YZ)
            int plane_idx = drag - 3;
            Vec3 hit{};
            if (compute_plane_hit(ray, state_.drag_start_pos, PLANE_NORMALS[plane_idx], hit)) {
                frame_delta_ = vec3_sub(hit, last_hit_point_);
                last_hit_point_ = hit;
            }
        } else {
            // Free movement (6) — camera-facing plane
            Vec3 cam_forward = vec3_normalize(vec3_sub(gizmo_position, camera.eye_position()));
            Vec3 hit{};
            if (compute_plane_hit(ray, state_.drag_start_pos, cam_forward, hit)) {
                frame_delta_ = vec3_sub(hit, last_hit_point_);
                last_hit_point_ = hit;
            }
        }
        return;
    }

    // --- Hover detection ---
    int best_target = -1;
    float best_score = 1e30f; // lower is better

    float arrow_len = gizmo::ARROW_TOTAL_LENGTH * gizmo_scale;
    float quad_size = PLANE_QUAD_RATIO * arrow_len;
    float center_radius = CENTER_HIT_RATIO * arrow_len;

    // Test single axes (0, 1, 2)
    for (int i = 0; i < 3; ++i) {
        float t = 0.0f;
        float dist = ray_axis_distance(ray, gizmo_position, gizmo::AXIS_DIRS[i], t);
        if (t >= 0.0f && t <= arrow_len && dist < threshold && dist < best_score) {
            best_score = dist;
            best_target = i;
        }
    }

    // Test plane quads (3=XY, 4=XZ, 5=YZ)
    // Each plane quad is a small square at (quad_size, quad_size) from origin
    for (int p = 0; p < 3; ++p) {
        int a1 = PLANE_AXES[p][0];
        int a2 = PLANE_AXES[p][1];
        Vec3 hit{};
        if (compute_plane_hit(ray, gizmo_position, PLANE_NORMALS[p], hit)) {
            Vec3 local = vec3_sub(hit, gizmo_position);
            float coord[3] = {local.x, local.y, local.z};
            float c1 = coord[a1];
            float c2 = coord[a2];
            // Hit inside the quad: both coords in (0, quad_size)
            if (c1 > 0.0f && c1 < quad_size && c2 > 0.0f && c2 < quad_size) {
                // Score by distance to quad center for priority
                float qcx = quad_size * 0.5f;
                float qcy = quad_size * 0.5f;
                float qdist = std::sqrt((c1 - qcx) * (c1 - qcx) + (c2 - qcy) * (c2 - qcy));
                // Plane quads should be prioritized over axis when mouse is clearly in quad
                float score = qdist * 0.5f; // weight down to prefer planes in overlap
                if (score < best_score) {
                    best_score = score;
                    best_target = 3 + p;
                }
            }
        }
    }

    // Test center cube (6=Free)
    {
        // Use camera-facing plane for center hit
        Vec3 cam_forward = vec3_normalize(vec3_sub(gizmo_position, camera.eye_position()));
        Vec3 hit{};
        if (compute_plane_hit(ray, gizmo_position, cam_forward, hit)) {
            float dist = vec3_length(vec3_sub(hit, gizmo_position));
            if (dist < center_radius && dist < best_score) {
                best_score = dist;
                best_target = 6;
            }
        }
    }

    state_.hovered_axis = best_target;

    // --- Start drag on click ---
    if (just_pressed && best_target >= 0) {
        state_.dragging_axis = best_target;
        state_.drag_start_pos = gizmo_position;

        if (best_target <= 2) {
            float t = 0.0f;
            ray_axis_distance(ray, gizmo_position, gizmo::AXIS_DIRS[best_target], t);
            last_hit_point_ =
                vec3_add(gizmo_position, vec3_scale(gizmo::AXIS_DIRS[best_target], t));
        } else if (best_target <= 5) {
            int plane_idx = best_target - 3;
            Vec3 hit{};
            if (compute_plane_hit(ray, gizmo_position, PLANE_NORMALS[plane_idx], hit)) {
                last_hit_point_ = hit;
            }
        } else {
            Vec3 cam_forward = vec3_normalize(vec3_sub(gizmo_position, camera.eye_position()));
            Vec3 hit{};
            if (compute_plane_hit(ray, gizmo_position, cam_forward, hit)) {
                last_hit_point_ = hit;
            }
        }
    }
}

GizmoState GizmoInteraction::state() const {
    return state_;
}

Vec3 GizmoInteraction::position_delta() const {
    return frame_delta_;
}

} // namespace gg
