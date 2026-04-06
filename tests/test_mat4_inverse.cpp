#include "math/mat4.h"
#include "math/vec3.h"

#include <cmath>
#include <gtest/gtest.h>

TEST(Mat4InverseTest, IdentityInverseIsIdentity) {
    auto m = gg::Mat4::identity();
    auto inv = m.inverse();
    for (int i = 0; i < 16; ++i) {
        EXPECT_NEAR(inv.data[i], m.data[i], 1e-5f);
    }
}

TEST(Mat4InverseTest, TranslationInverse) {
    auto m = gg::Mat4::translation({3.0f, 4.0f, 5.0f});
    auto inv = m.inverse();
    auto result = m * inv;
    auto id = gg::Mat4::identity();
    for (int i = 0; i < 16; ++i) {
        EXPECT_NEAR(result.data[i], id.data[i], 1e-5f);
    }
}

TEST(Mat4InverseTest, PerspectiveInverse) {
    auto m = gg::Mat4::perspective(0.785f, 16.0f / 9.0f, 0.1f, 100.0f);
    auto inv = m.inverse();
    auto result = m * inv;
    auto id = gg::Mat4::identity();
    for (int i = 0; i < 16; ++i) {
        EXPECT_NEAR(result.data[i], id.data[i], 1e-4f);
    }
}

TEST(Mat4InverseTest, LookAtInverse) {
    auto m = gg::Mat4::look_at({0, 0, 5}, {0, 0, 0}, {0, 1, 0});
    auto inv = m.inverse();
    auto result = m * inv;
    auto id = gg::Mat4::identity();
    for (int i = 0; i < 16; ++i) {
        EXPECT_NEAR(result.data[i], id.data[i], 1e-4f);
    }
}

TEST(Mat4InverseTest, ViewProjectionInverse) {
    auto v = gg::Mat4::look_at({3, 4, 5}, {0, 0, 0}, {0, 1, 0});
    auto p = gg::Mat4::perspective(0.785f, 1.5f, 0.1f, 100.0f);
    auto vp = p * v;
    auto inv = vp.inverse();
    auto result = vp * inv;
    auto id = gg::Mat4::identity();
    for (int i = 0; i < 16; ++i) {
        EXPECT_NEAR(result.data[i], id.data[i], 1e-3f);
    }
}

TEST(Mat4TransformTest, TransformPointByTranslation) {
    auto m = gg::Mat4::translation({10.0f, 20.0f, 30.0f});
    auto p = m.transform_point({1.0f, 2.0f, 3.0f});
    EXPECT_NEAR(p.x, 11.0f, 1e-5f);
    EXPECT_NEAR(p.y, 22.0f, 1e-5f);
    EXPECT_NEAR(p.z, 33.0f, 1e-5f);
}

TEST(Mat4TransformTest, TransformDirectionIgnoresTranslation) {
    auto m = gg::Mat4::translation({10.0f, 20.0f, 30.0f});
    auto d = m.transform_direction({1.0f, 0.0f, 0.0f});
    EXPECT_NEAR(d.x, 1.0f, 1e-5f);
    EXPECT_NEAR(d.y, 0.0f, 1e-5f);
    EXPECT_NEAR(d.z, 0.0f, 1e-5f);
}
