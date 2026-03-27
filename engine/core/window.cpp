#include "core/window.h"

#include <GLFW/glfw3.h>
#include <cstdio>

namespace gg {

namespace {
    bool glfw_initialized = false;

    bool ensure_glfw() {
        if (glfw_initialized) return true;
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

    GLFWwindow* handle = glfwCreateWindow(
        static_cast<int>(config.width),
        static_cast<int>(config.height),
        config.title.c_str(),
        nullptr,
        nullptr
    );

    if (!handle) {
        std::fprintf(stderr, "[game-gym] Failed to create GLFW window\n");
        return nullptr;
    }

    return std::unique_ptr<Window>(
        new Window(handle, config.width, config.height)
    );
}

Window::Window(GLFWwindow* handle, uint32_t w, uint32_t h)
    : handle_(handle), width_(w), height_(h) {}

Window::~Window() {
    if (handle_) {
        glfwDestroyWindow(handle_);
        handle_ = nullptr;
    }
}

Window::Window(Window&& other) noexcept
    : handle_(other.handle_), width_(other.width_), height_(other.height_) {
    other.handle_ = nullptr;
}

Window& Window::operator=(Window&& other) noexcept {
    if (this != &other) {
        if (handle_) glfwDestroyWindow(handle_);
        handle_ = other.handle_;
        width_ = other.width_;
        height_ = other.height_;
        other.handle_ = nullptr;
    }
    return *this;
}

bool Window::should_close() const {
    return handle_ && glfwWindowShouldClose(handle_);
}

void Window::poll_events() const {
    glfwPollEvents();
}

uint32_t Window::width() const { return width_; }
uint32_t Window::height() const { return height_; }
GLFWwindow* Window::native_handle() const { return handle_; }

} // namespace gg
