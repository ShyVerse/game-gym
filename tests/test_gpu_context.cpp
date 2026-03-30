#include "core/window.h"
#include "renderer/gpu_context.h"

#include <gtest/gtest.h>

// GpuContext tests require a display / GPU.  On headless CI, glfwInit() will
// succeed but surface creation may fail.  We guard each test with a check so
// the suite doesn't hard-crash in environments without a GPU.

namespace {

std::unique_ptr<gg::Window> make_window() {
    return gg::Window::create({.title = "test", .width = 256, .height = 256, .resizable = false});
}

} // namespace

TEST(GpuContextTest, CreationSucceeds) {
    auto window = make_window();
    ASSERT_NE(window, nullptr);

    std::unique_ptr<gg::GpuContext> ctx;
    ASSERT_NO_THROW(ctx = gg::GpuContext::create(*window));
    ASSERT_NE(ctx, nullptr);
}

TEST(GpuContextTest, DeviceAndQueueNonNull) {
    auto window = make_window();
    ASSERT_NE(window, nullptr);

    auto ctx = gg::GpuContext::create(*window);
    ASSERT_NE(ctx, nullptr);

    EXPECT_NE(ctx->device(), nullptr);
    EXPECT_NE(ctx->queue(), nullptr);
    EXPECT_NE(ctx->surface(), nullptr);
}

TEST(GpuContextTest, SurfaceDimensionsMatchWindow) {
    auto window = make_window();
    ASSERT_NE(window, nullptr);

    auto ctx = gg::GpuContext::create(*window);
    ASSERT_NE(ctx, nullptr);

    EXPECT_EQ(ctx->surface_width(), window->width());
    EXPECT_EQ(ctx->surface_height(), window->height());
}
