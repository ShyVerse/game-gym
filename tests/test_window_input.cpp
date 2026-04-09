#include "core/window.h"

#include <GLFW/glfw3.h>
#include <cmath>
#include <gtest/gtest.h>

class WindowInputTest : public ::testing::Test {
protected:
    void SetUp() override {
        window_ = gg::Window::create({.title = "input-test", .width = 320, .height = 240});
        ASSERT_NE(window_, nullptr);
    }
    std::unique_ptr<gg::Window> window_;
};

TEST_F(WindowInputTest, MousePositionIsFinite) {
    float mx = window_->mouse_x();
    float my = window_->mouse_y();
    EXPECT_FALSE(std::isnan(mx));
    EXPECT_FALSE(std::isnan(my));
}

TEST_F(WindowInputTest, MouseButtonDefaultsToFalse) {
    EXPECT_FALSE(window_->mouse_button(0));
    EXPECT_FALSE(window_->mouse_button(1));
}

TEST_F(WindowInputTest, ScrollDeltaDefaultsToZero) {
    EXPECT_FLOAT_EQ(window_->scroll_delta_y(), 0.0f);
}

TEST_F(WindowInputTest, ResetScrollClearsDelta) {
    window_->reset_scroll();
    EXPECT_FLOAT_EQ(window_->scroll_delta_y(), 0.0f);
}

TEST_F(WindowInputTest, KeyDownDefaultsToFalse) {
    EXPECT_FALSE(window_->key_down(GLFW_KEY_W));
    EXPECT_FALSE(window_->key_down(GLFW_KEY_LEFT_SHIFT));
}

TEST_F(WindowInputTest, CursorCaptureTogglesWithoutCrash) {
    window_->set_cursor_captured(true);
    window_->set_cursor_captured(false);
    SUCCEED();
}
