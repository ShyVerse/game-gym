#include "editor/gizmo_interaction.h"

#include "math/ray.h"
#include "math/vec3.h"
#include "renderer/camera.h"

#include <array>
#include <cmath>

namespace gg {

static constexpr float HIT_THRESHOLD_FACTOR = 0.15f;
static constexpr float ARROW_TOTAL_LENGTH = 1.5f; // SHAFT_LENGTH(1.2) + CONE_LENGTH(0.3)

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

    int best_axis = -1;
    float best_dist = threshold;

    for (int i = 0; i < 3; ++i) {
        float t = 0.0f;
        float dist = ray_axis_distance(ray, gizmo_position, AXIS_DIRS[i], t);

        float arrow_len = ARROW_TOTAL_LENGTH * gizmo_scale;
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
