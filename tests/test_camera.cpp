#include "renderer/camera.h"

#include <cmath>
#include <gtest/gtest.h>

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
