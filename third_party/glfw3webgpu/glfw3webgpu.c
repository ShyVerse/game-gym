#include "glfw3webgpu.h"
#include <webgpu.h>

#if defined(_WIN32)
#  define GLFW_EXPOSE_NATIVE_WIN32
#  include <GLFW/glfw3native.h>

WGPUSurface glfwCreateWGPUSurface(WGPUInstance instance, GLFWwindow* window) {
    HWND hwnd = glfwGetWin32Window(window);
    HINSTANCE hinstance = GetModuleHandle(NULL);

    WGPUSurfaceSourceWindowsHWND source = {
        .chain = { .sType = WGPUSType_SurfaceSourceWindowsHWND },
        .hinstance = hinstance,
        .hwnd = hwnd,
    };
    WGPUSurfaceDescriptor descriptor = {
        .nextInChain = (const WGPUChainedStruct*)&source,
    };
    return wgpuInstanceCreateSurface(instance, &descriptor);
}

#elif defined(__linux__)
#  ifdef GLFW_USE_WAYLAND
#    define GLFW_EXPOSE_NATIVE_WAYLAND
#    include <GLFW/glfw3native.h>

WGPUSurface glfwCreateWGPUSurface(WGPUInstance instance, GLFWwindow* window) {
    struct wl_display* display = glfwGetWaylandDisplay();
    struct wl_surface* surface = glfwGetWaylandWindow(window);

    WGPUSurfaceSourceWaylandSurface source = {
        .chain = { .sType = WGPUSType_SurfaceSourceWaylandSurface },
        .display = display,
        .surface = surface,
    };
    WGPUSurfaceDescriptor descriptor = {
        .nextInChain = (const WGPUChainedStruct*)&source,
    };
    return wgpuInstanceCreateSurface(instance, &descriptor);
}

#  else
#    define GLFW_EXPOSE_NATIVE_X11
#    include <GLFW/glfw3native.h>

WGPUSurface glfwCreateWGPUSurface(WGPUInstance instance, GLFWwindow* window) {
    Display* x11_display = glfwGetX11Display();
    Window x11_window = glfwGetX11Window(window);

    WGPUSurfaceSourceXlibWindow source = {
        .chain = { .sType = WGPUSType_SurfaceSourceXlibWindow },
        .display = x11_display,
        .window = (uint64_t)x11_window,
    };
    WGPUSurfaceDescriptor descriptor = {
        .nextInChain = (const WGPUChainedStruct*)&source,
    };
    return wgpuInstanceCreateSurface(instance, &descriptor);
}

#  endif
#endif
