#pragma once

#include <memory>
#include <string>
#include <cstdint>

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
    [[nodiscard]] GLFWwindow* native_handle() const;

private:
    explicit Window(GLFWwindow* handle, uint32_t w, uint32_t h);
    GLFWwindow* handle_ = nullptr;
    uint32_t width_ = 0;
    uint32_t height_ = 0;
};

} // namespace gg
