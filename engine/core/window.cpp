#include "core/window.h"

#include <GLFW/glfw3.h>
#include <cstdio>

namespace gg {

namespace {
bool glfw_initialized = false;

bool ensure_glfw() {
    if (glfw_initialized) {
        return true;
    }
    if (glfwInit() == GLFW_FALSE) {
        std::fprintf(stderr, "[game-gym] Failed to initialize GLFW\n");
        return false;
    }
    glfw_initialized = true;
    return true;
}
} // namespace

std::unique_ptr<Window> Window::create(const WindowConfig& config) {
    if (config.width == 0 || config.height == 0) {
        return nullptr;
    }
    if (!ensure_glfw()) {
        return nullptr;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, config.resizable ? GLFW_TRUE : GLFW_FALSE);

    GLFWwindow* handle = glfwCreateWindow(static_cast<int>(config.width),
                                          static_cast<int>(config.height),
                                          config.title.c_str(),
                                          nullptr,
                                          nullptr);

    if (!handle) {
        std::fprintf(stderr, "[game-gym] Failed to create GLFW window\n");
        return nullptr;
    }

    auto window = std::unique_ptr<Window>(new Window(handle, config.width, config.height));
    glfwSetWindowUserPointer(handle, window.get());
    glfwSetScrollCallback(handle, [](GLFWwindow* w, double /*x*/, double y) {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(w));
        if (self) {
            self->scroll_y_accum_ += static_cast<float>(y);
        }
    });
    return window;
}

Window::Window(GLFWwindow* handle, uint32_t w, uint32_t h)
    : handle_(handle), width_(w), height_(h) {}

Window::~Window() {
    if (handle_) {
        glfwSetWindowUserPointer(handle_, nullptr);
        glfwDestroyWindow(handle_);
        handle_ = nullptr;
    }
}

Window::Window(Window&& other) noexcept
    : handle_(other.handle_)
    , width_(other.width_)
    , height_(other.height_)
    , scroll_y_accum_(other.scroll_y_accum_) {
    if (handle_) {
        glfwSetWindowUserPointer(handle_, this);
    }
    other.handle_ = nullptr;
    other.scroll_y_accum_ = 0.0f;
}

Window& Window::operator=(Window&& other) noexcept {
    if (this != &other) {
        if (handle_) {
            glfwDestroyWindow(handle_);
        }
        handle_ = other.handle_;
        width_ = other.width_;
        height_ = other.height_;
        scroll_y_accum_ = other.scroll_y_accum_;
        if (handle_) {
            glfwSetWindowUserPointer(handle_, this);
        }
        other.handle_ = nullptr;
        other.scroll_y_accum_ = 0.0f;
    }
    return *this;
}

bool Window::should_close() const {
    return handle_ && glfwWindowShouldClose(handle_);
}

void Window::poll_events() const {
    glfwPollEvents();
}

uint32_t Window::width() const {
    return width_;
}
uint32_t Window::height() const {
    return height_;
}
uint32_t Window::framebuffer_width() const {
    int w = 0;
    if (handle_) {
        glfwGetFramebufferSize(handle_, &w, nullptr);
    }
    return static_cast<uint32_t>(w);
}
uint32_t Window::framebuffer_height() const {
    int h = 0;
    if (handle_) {
        glfwGetFramebufferSize(handle_, nullptr, &h);
    }
    return static_cast<uint32_t>(h);
}
GLFWwindow* Window::native_handle() const {
    return handle_;
}

float Window::mouse_x() const {
    double x = 0.0;
    if (handle_) {
        glfwGetCursorPos(handle_, &x, nullptr);
    }
    return static_cast<float>(x);
}

float Window::mouse_y() const {
    double y = 0.0;
    if (handle_) {
        glfwGetCursorPos(handle_, nullptr, &y);
    }
    return static_cast<float>(y);
}

bool Window::mouse_button(int button) const {
    if (!handle_) {
        return false;
    }
    return glfwGetMouseButton(handle_, button) == GLFW_PRESS;
}

bool Window::key_down(int key) const {
    if (!handle_) {
        return false;
    }
    return glfwGetKey(handle_, key) == GLFW_PRESS;
}

float Window::scroll_delta_y() const {
    return scroll_y_accum_;
}

void Window::reset_scroll() {
    scroll_y_accum_ = 0.0f;
}

void Window::set_cursor_captured(bool captured) const {
    if (!handle_) {
        return;
    }
    glfwSetInputMode(handle_, GLFW_CURSOR, captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
}

} // namespace gg
