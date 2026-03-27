#include <gtest/gtest.h>
#include "core/window.h"

TEST(WindowTest, CreatesWithValidDimensions) {
    gg::WindowConfig config{
        .title = "Test Window",
        .width = 800,
        .height = 600,
    };
    auto window = gg::Window::create(config);
    ASSERT_NE(window, nullptr);
    EXPECT_EQ(window->width(), 800);
    EXPECT_EQ(window->height(), 600);
    EXPECT_FALSE(window->should_close());
}

TEST(WindowTest, RejectsZeroDimensions) {
    gg::WindowConfig config{
        .title = "Bad Window",
        .width = 0,
        .height = 0,
    };
    auto window = gg::Window::create(config);
    EXPECT_EQ(window, nullptr);
}

TEST(WindowTest, ReturnsNativeHandle) {
    gg::WindowConfig config{
        .title = "Handle Test",
        .width = 640,
        .height = 480,
    };
    auto window = gg::Window::create(config);
    ASSERT_NE(window, nullptr);
    EXPECT_NE(window->native_handle(), nullptr);
}
