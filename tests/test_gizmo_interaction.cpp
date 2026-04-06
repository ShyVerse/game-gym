#include "editor/gizmo_interaction.h"
#include "math/mat4.h"
#include "math/vec3.h"
#include "renderer/camera.h"

#include <cmath>
#include <gtest/gtest.h>
#include <numbers>

class GizmoInteractionTest : public ::testing::Test {
protected:
    void SetUp() override {
        gi_ = gg::GizmoInteraction::create();
        cam_ = gg::Camera::create();
        cam_->set_aspect(float(FB_W) / float(FB_H));
    }

    void update(float mx, float my, bool down) {
        gi_->update(mx, my, down, FB_W, FB_H, *cam_, gizmo_pos_, gizmo_scale_);
    }

    /// Project a world point to screen coordinates using camera VP matrix.
    /// Returns screen pixel coordinates (0,0 = top-left).
    std::pair<float, float> world_to_screen(const gg::Vec3& p) {
        gg::Mat4 vp = cam_->view_projection_matrix();
        gg::Vec3 clip = vp.transform_point(p);
        float sx = (clip.x + 1.0f) * 0.5f * float(FB_W);
        float sy = (1.0f - clip.y) * 0.5f * float(FB_H);
        return {sx, sy};
    }

    static constexpr uint32_t FB_W = 800;
    static constexpr uint32_t FB_H = 600;

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
    update(0.0f, 0.0f, false);
    EXPECT_EQ(gi_->state().hovered_axis, -1);
}

TEST_F(GizmoInteractionTest, PositionDeltaIsZeroWhenNotDragging) {
    update(0.0f, 0.0f, false);
    auto d = gi_->position_delta();
    EXPECT_FLOAT_EQ(d.x, 0.0f);
    EXPECT_FLOAT_EQ(d.y, 0.0f);
    EXPECT_FLOAT_EQ(d.z, 0.0f);
}

TEST_F(GizmoInteractionTest, HoverDetectsXAxis) {
    // Project a point on the X axis to screen space
    auto [sx, sy] = world_to_screen({0.6f, 0.0f, 0.0f});
    update(sx, sy, false);
    EXPECT_EQ(gi_->state().hovered_axis, 0); // X axis
}

TEST_F(GizmoInteractionTest, HoverDetectsYAxis) {
    auto [sx, sy] = world_to_screen({0.0f, 0.6f, 0.0f});
    update(sx, sy, false);
    EXPECT_EQ(gi_->state().hovered_axis, 1); // Y axis
}

TEST_F(GizmoInteractionTest, ClickStartsDrag) {
    auto [sx, sy] = world_to_screen({0.6f, 0.0f, 0.0f});
    // Hover first
    update(sx, sy, false);
    ASSERT_EQ(gi_->state().hovered_axis, 0);
    // Click
    update(sx, sy, true);
    EXPECT_EQ(gi_->state().dragging_axis, 0);
}

TEST_F(GizmoInteractionTest, ReleaseEndsDrag) {
    auto [sx, sy] = world_to_screen({0.6f, 0.0f, 0.0f});
    update(sx, sy, false); // hover
    update(sx, sy, true);  // press
    ASSERT_EQ(gi_->state().dragging_axis, 0);
    update(sx, sy, false); // release
    EXPECT_EQ(gi_->state().dragging_axis, -1);
}

TEST_F(GizmoInteractionTest, DragProducesNonZeroDelta) {
    auto [sx, sy] = world_to_screen({0.6f, 0.0f, 0.0f});
    update(sx, sy, false); // hover X axis
    ASSERT_EQ(gi_->state().hovered_axis, 0);
    update(sx, sy, true); // start drag
    ASSERT_EQ(gi_->state().dragging_axis, 0);

    // Move mouse further along X direction
    auto [sx2, sy2] = world_to_screen({1.0f, 0.0f, 0.0f});
    update(sx2, sy2, true); // drag

    auto d = gi_->position_delta();
    // Delta should be positive along X, near-zero on Y and Z
    EXPECT_GT(d.x, 0.1f);
    EXPECT_NEAR(d.y, 0.0f, 0.05f);
    EXPECT_NEAR(d.z, 0.0f, 0.05f);
}

TEST_F(GizmoInteractionTest, DragDeltaIsZeroOnRelease) {
    auto [sx, sy] = world_to_screen({0.6f, 0.0f, 0.0f});
    update(sx, sy, false); // hover
    update(sx, sy, true);  // press

    auto [sx2, sy2] = world_to_screen({1.0f, 0.0f, 0.0f});
    update(sx2, sy2, true); // drag

    update(sx2, sy2, false); // release
    auto d = gi_->position_delta();
    EXPECT_FLOAT_EQ(d.x, 0.0f);
    EXPECT_FLOAT_EQ(d.y, 0.0f);
    EXPECT_FLOAT_EQ(d.z, 0.0f);
}
