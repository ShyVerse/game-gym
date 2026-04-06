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
