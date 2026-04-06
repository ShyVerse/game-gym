#pragma once

#include "ecs/components.h"

#include <memory>

namespace gg {

class Camera;

// Interaction targets:
// -1 = none, 0=X, 1=Y, 2=Z, 3=XY plane, 4=XZ plane, 5=YZ plane, 6=Free (camera plane)
struct GizmoState {
    int hovered_axis = -1;
    int dragging_axis = -1;
    Vec3 drag_start_pos{}; // entity position when drag started
};

class GizmoInteraction {
public:
    static std::unique_ptr<GizmoInteraction> create();
    ~GizmoInteraction() = default;

    GizmoInteraction(const GizmoInteraction&) = delete;
    GizmoInteraction& operator=(const GizmoInteraction&) = delete;

    void update(float mouse_x,
                float mouse_y,
                bool mouse_down,
                uint32_t win_width,
                uint32_t win_height,
                const Camera& camera,
                const Vec3& gizmo_position,
                float gizmo_scale);

    [[nodiscard]] GizmoState state() const;
    [[nodiscard]] Vec3 position_delta() const;

private:
    GizmoInteraction() = default;

    GizmoState state_{};
    Vec3 frame_delta_{};
    Vec3 last_hit_point_{}; // last ray-plane/axis intersection point
    bool was_mouse_down_ = false;
};

} // namespace gg
