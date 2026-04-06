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
    update(0.0f, 0.0f, false);
    EXPECT_EQ(gi_->state().hovered_axis, -1);
}

TEST_F(GizmoInteractionTest, DragStartSetsAxis) {
    update(440.0f, 300.0f, false);
    int hovered = gi_->state().hovered_axis;
    if (hovered >= 0) {
        update(440.0f, 300.0f, true);
        EXPECT_EQ(gi_->state().dragging_axis, hovered);
    }
}

TEST_F(GizmoInteractionTest, ReleaseEndsDrag) {
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
