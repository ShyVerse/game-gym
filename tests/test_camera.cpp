#include "math/vec3.h"
#include "renderer/camera.h"

#include <cmath>
#include <gtest/gtest.h>
#include <numbers>

TEST(CameraTest, CreatesSuccessfully) {
    auto cam = gg::Camera::create();
    ASSERT_NE(cam, nullptr);
}

TEST(CameraTest, ViewProjectionIsNotIdentity) {
    auto cam = gg::Camera::create();
    auto vp = cam->view_projection_matrix();
    auto id = gg::Mat4::identity();
    bool same = true;
    for (int i = 0; i < 16; ++i) {
        if (std::abs(vp.data[i] - id.data[i]) > 1e-5f) {
            same = false;
            break;
        }
    }
    EXPECT_FALSE(same);
}

TEST(CameraTest, OrbitChangesViewMatrix) {
    auto cam = gg::Camera::create();
    auto before = cam->view_matrix();
    cam->orbit(0.5f, 0.0f);
    auto after = cam->view_matrix();
    bool changed = false;
    for (int i = 0; i < 16; ++i) {
        if (std::abs(before.data[i] - after.data[i]) > 1e-6f) {
            changed = true;
            break;
        }
    }
    EXPECT_TRUE(changed);
}

TEST(CameraTest, ZoomChangesDistance) {
    auto cam = gg::Camera::create();
    auto before = cam->view_matrix();
    cam->zoom(1.0f);
    auto after = cam->view_matrix();
    bool changed = false;
    for (int i = 0; i < 16; ++i) {
        if (std::abs(before.data[i] - after.data[i]) > 1e-6f) {
            changed = true;
            break;
        }
    }
    EXPECT_TRUE(changed);
}

TEST(CameraTest, PitchIsClamped) {
    auto cam = gg::Camera::create();
    cam->orbit(0.0f, 1000.0f);
    auto vp = cam->view_projection_matrix();
    for (int i = 0; i < 16; ++i) {
        EXPECT_FALSE(std::isnan(vp.data[i]));
        EXPECT_FALSE(std::isinf(vp.data[i]));
    }
}

TEST(CameraTest, EyePositionAtDefaultIsAboveOrigin) {
    auto cam = gg::Camera::create();
    auto eye = cam->eye_position();
    // Default: yaw=0, pitch=0.3, distance=5, target=origin
    // eye.y should be positive (above origin)
    EXPECT_GT(eye.y, 0.0f);
    // eye.z should be positive (in front of origin)
    EXPECT_GT(eye.z, 0.0f);
    // Distance from origin should be ~5
    float dist = gg::vec3_length(eye);
    EXPECT_NEAR(dist, 5.0f, 0.1f);
}

TEST(CameraTest, EyePositionChangesAfterOrbit) {
    auto cam = gg::Camera::create();
    auto before = cam->eye_position();
    cam->orbit(50.0f, 0.0f); // yaw change
    auto after = cam->eye_position();
    // X should change due to yaw rotation
    EXPECT_NE(before.x, after.x);
}

TEST(CameraTest, FovReturnsDefault45Degrees) {
    auto cam = gg::Camera::create();
    float expected = 45.0f * (std::numbers::pi_v<float> / 180.0f);
    EXPECT_NEAR(cam->fov(), expected, 1e-5f);
}

TEST(CameraTest, SetAspectChangesProjection) {
    auto cam = gg::Camera::create();
    auto before = cam->projection_matrix();
    cam->set_aspect(2.0f);
    auto after = cam->projection_matrix();
    // data[0] changes with aspect ratio
    EXPECT_NE(before.data[0], after.data[0]);
}

TEST(CameraTest, ProjectionMatrixHasValidValues) {
    auto cam = gg::Camera::create();
    auto p = cam->projection_matrix();
    // Perspective matrix: data[0] and data[5] should be positive
    EXPECT_GT(p.data[0], 0.0f);
    EXPECT_GT(p.data[5], 0.0f);
    // data[11] should be -1 for perspective
    EXPECT_FLOAT_EQ(p.data[11], -1.0f);
}

TEST(CameraTest, ZoomClampsMinDistance) {
    auto cam = gg::Camera::create();
    // Zoom in a lot (negative delta zooms out in our impl, positive zooms in)
    for (int i = 0; i < 100; ++i) {
        cam->zoom(10.0f);
    }
    auto eye = cam->eye_position();
    float dist = gg::vec3_length(eye);
    // Should be clamped to minimum 0.5
    EXPECT_GE(dist, 0.4f);
}

TEST(CameraTest, ViewProjectionComposition) {
    auto cam = gg::Camera::create();
    auto v = cam->view_matrix();
    auto p = cam->projection_matrix();
    auto vp = cam->view_projection_matrix();
    auto expected = p * v;
    for (int i = 0; i < 16; ++i) {
        EXPECT_NEAR(vp.data[i], expected.data[i], 1e-5f);
    }
}

TEST(CameraTest, FlyForwardFollowsViewDirection) {
    auto cam = gg::Camera::create();
    cam->set_fly_mode(true);
    cam->look(0.0f, -40.0f);

    const auto before = cam->eye_position();
    cam->move_local(0.0f, 0.0f, -1.0f, 1.0f);
    const auto after = cam->eye_position();

    EXPECT_NE(after.y, before.y);
    EXPECT_LT(after.z, before.z);
}

TEST(CameraTest, FlyStrafeUsesCameraRightAxis) {
    auto cam = gg::Camera::create();
    cam->set_fly_mode(true);
    cam->look(-90.0f, 0.0f);

    const auto before = cam->eye_position();
    cam->move_local(1.0f, 0.0f, 0.0f, 1.0f);
    const auto after = cam->eye_position();

    EXPECT_NE(after.z, before.z);
}

TEST(CameraTest, FlyModeRoundTripsBackToOrbitWithoutInvalidMatrices) {
    auto cam = gg::Camera::create();
    cam->set_fly_mode(true);
    cam->move_local(0.0f, 0.0f, -2.0f, 0.5f);
    cam->set_fly_mode(false);

    const auto vp = cam->view_projection_matrix();
    for (float value : vp.data) {
        EXPECT_FALSE(std::isnan(value));
        EXPECT_FALSE(std::isinf(value));
    }
}

TEST(CameraTest, FlyStrafeRemainsFiniteNearPitchLimit) {
    auto cam = gg::Camera::create();
    cam->set_fly_mode(true);
    cam->look(0.0f, 1000.0f);

    const auto before = cam->eye_position();
    cam->move_local(1.0f, 0.0f, 0.0f, 1.0f);
    const auto after = cam->eye_position();

    EXPECT_FALSE(std::isnan(after.x));
    EXPECT_FALSE(std::isnan(after.y));
    EXPECT_FALSE(std::isnan(after.z));
    EXPECT_NE(after.x, before.x);
}
