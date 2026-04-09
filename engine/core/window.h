#pragma once

#include <cstdint>
#include <memory>
#include <string>

struct GLFWwindow;

namespace gg {

struct WindowConfig {
    std::string title = "Game-Gym";
    uint32_t width = 1280;
    uint32_t height = 720;
    bool resizable = true;
};

class Window {
public:
    static std::unique_ptr<Window> create(const WindowConfig& config);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) noexcept;
    Window& operator=(Window&&) noexcept;

    [[nodiscard]] bool should_close() const;
    void poll_events() const;

    [[nodiscard]] uint32_t width() const;
    [[nodiscard]] uint32_t height() const;
    [[nodiscard]] uint32_t framebuffer_width() const;
    [[nodiscard]] uint32_t framebuffer_height() const;
    [[nodiscard]] GLFWwindow* native_handle() const;

    [[nodiscard]] float mouse_x() const;
    [[nodiscard]] float mouse_y() const;
    [[nodiscard]] bool mouse_button(int button) const;
    [[nodiscard]] bool key_down(int key) const;
    [[nodiscard]] float scroll_delta_y() const;
    void reset_scroll();
    void set_cursor_captured(bool captured) const;

private:
    explicit Window(GLFWwindow* handle, uint32_t w, uint32_t h);
    GLFWwindow* handle_ = nullptr;
    uint32_t width_ = 0;
    uint32_t height_ = 0;
    mutable float scroll_y_accum_ = 0.0f;
};

} // namespace gg
