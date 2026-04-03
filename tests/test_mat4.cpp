#include "math/mat4.h"

#include <cmath>
#include <gtest/gtest.h>
#include <numbers>

TEST(Mat4Test, IdentityIsIdentity) {
    auto m = gg::Mat4::identity();
    EXPECT_FLOAT_EQ(m.data[0], 1.0f);
    EXPECT_FLOAT_EQ(m.data[5], 1.0f);
    EXPECT_FLOAT_EQ(m.data[10], 1.0f);
    EXPECT_FLOAT_EQ(m.data[15], 1.0f);
    EXPECT_FLOAT_EQ(m.data[1], 0.0f);
    EXPECT_FLOAT_EQ(m.data[4], 0.0f);
}

TEST(Mat4Test, IdentityTimesIdentity) {
    auto a = gg::Mat4::identity();
    auto b = gg::Mat4::identity();
    auto c = a * b;
    for (int i = 0; i < 16; ++i) {
        EXPECT_FLOAT_EQ(c.data[i], a.data[i]);
    }
}

TEST(Mat4Test, PerspectiveHasCorrectAspect) {
    float fov = 45.0f * (std::numbers::pi_v<float> / 180.0f);
    auto p = gg::Mat4::perspective(fov, 16.0f / 9.0f, 0.1f, 1000.0f);
    float t = std::tan(fov / 2.0f);
    EXPECT_NEAR(p.data[0], 1.0f / ((16.0f / 9.0f) * t), 1e-5f);
    EXPECT_NEAR(p.data[5], 1.0f / t, 1e-5f);
}

TEST(Mat4Test, LookAtFromZAxis) {
    gg::Vec3 eye{0, 0, 5};
    gg::Vec3 target{0, 0, 0};
    gg::Vec3 up{0, 1, 0};
    auto v = gg::Mat4::look_at(eye, target, up);
    EXPECT_NEAR(v.data[14], -5.0f, 1e-5f);
}

TEST(Mat4Test, TranslationPlacesOffsetInLastColumn) {
    gg::Vec3 offset{1.0f, 2.0f, 3.0f};
    auto m = gg::Mat4::translation(offset);
    EXPECT_FLOAT_EQ(m.data[12], 1.0f);
    EXPECT_FLOAT_EQ(m.data[13], 2.0f);
    EXPECT_FLOAT_EQ(m.data[14], 3.0f);
}

TEST(Mat4Test, FromTransformAppliesTranslation) {
    gg::Transform t{};
    t.position = {4.0f, 5.0f, 6.0f};
    auto m = gg::Mat4::from_transform(t);
    EXPECT_FLOAT_EQ(m.data[12], 4.0f);
    EXPECT_FLOAT_EQ(m.data[13], 5.0f);
    EXPECT_FLOAT_EQ(m.data[14], 6.0f);
}
