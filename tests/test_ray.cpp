#include "math/ray.h"
#include "math/mat4.h"
#include "math/vec3.h"

#include <cmath>
#include <gtest/gtest.h>
#include <numbers>

TEST(RayTest, ScreenCenterPointsForward) {
    auto v = gg::Mat4::look_at({0, 0, 5}, {0, 0, 0}, {0, 1, 0});
    auto p = gg::Mat4::perspective(
        45.0f * std::numbers::pi_v<float> / 180.0f, 1.0f, 0.1f, 100.0f);
    auto vp = p * v;
    auto inv_vp = vp.inverse();

    auto ray = gg::ray_from_screen(400.0f, 300.0f, 800, 600, inv_vp);

    EXPECT_NEAR(ray.origin.x, 0.0f, 0.1f);
    EXPECT_NEAR(ray.origin.y, 0.0f, 0.1f);
    EXPECT_NEAR(ray.origin.z, 5.0f, 0.5f);

    EXPECT_NEAR(ray.direction.z, -1.0f, 0.1f);
    EXPECT_NEAR(gg::vec3_length(ray.direction), 1.0f, 1e-5f);
}

TEST(RayTest, AxisDistanceParallelRayAndAxis) {
    gg::Ray ray{{0, 1, 0}, {1, 0, 0}};
    float t = 0.0f;
    float dist = gg::ray_axis_distance(ray, {0, 0, 0}, {1, 0, 0}, t);
    EXPECT_NEAR(dist, 1.0f, 1e-5f);
}

TEST(RayTest, AxisDistanceIntersectingRayAndAxis) {
    gg::Ray ray{{0, 1, 0}, gg::vec3_normalize({0, -1, 0})};
    float t = 0.0f;
    float dist = gg::ray_axis_distance(ray, {0, 0, 0}, {1, 0, 0}, t);
    EXPECT_NEAR(dist, 0.0f, 1e-4f);
    EXPECT_NEAR(t, 0.0f, 1e-4f);
}

TEST(RayTest, AxisDistanceSkewLines) {
    gg::Ray ray{{0, 2, 0}, {0, 0, 1}};
    float t = 0.0f;
    float dist = gg::ray_axis_distance(ray, {0, 0, 0}, {1, 0, 0}, t);
    EXPECT_NEAR(dist, 2.0f, 1e-5f);
    EXPECT_NEAR(t, 0.0f, 1e-4f);
}

TEST(RayTest, AxisClosestPointOnAxis) {
    gg::Ray ray{{3, 1, 0}, {0, -1, 0}};
    float t = 0.0f;
    float dist = gg::ray_axis_distance(ray, {0, 0, 0}, {1, 0, 0}, t);
    EXPECT_NEAR(dist, 0.0f, 1e-4f);
    EXPECT_NEAR(t, 3.0f, 1e-4f);
}
