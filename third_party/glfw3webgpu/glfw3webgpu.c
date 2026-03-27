#include "glfw3webgpu.h"
#include <webgpu.h>

#ifdef __APPLE__
#  define GLFW_EXPOSE_NATIVE_COCOA
#elif defined(_WIN32)
#  define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__linux__)
#  ifdef GLFW_USE_WAYLAND
#    define GLFW_EXPOSE_NATIVE_WAYLAND
#  else
#    define GLFW_EXPOSE_NATIVE_X11
#  endif
#endif

#include <GLFW/glfw3native.h>

#ifdef __APPLE__
#  include <Foundation/Foundation.h>
#  include <QuartzCore/CAMetalLayer.h>

WGPUSurface glfwCreateWGPUSurface(WGPUInstance instance, GLFWwindow* window) {
    id metal_layer = [CAMetalLayer layer];
    NSWindow* ns_window = glfwGetCocoaWindow(window);
    [ns_window.contentView setWantsLayer:YES];
    [ns_window.contentView setLayer:metal_layer];

    WGPUSurfaceDescriptorFromMetalLayer from_metal = {
        .chain = { .sType = WGPUSType_SurfaceDescriptorFromMetalLayer },
        .layer = metal_layer,
    };
    WGPUSurfaceDescriptor descriptor = {
        .nextInChain = (const WGPUChainedStruct*)&from_metal,
    };
    return wgpuInstanceCreateSurface(instance, &descriptor);
}

#elif defined(_WIN32)

WGPUSurface glfwCreateWGPUSurface(WGPUInstance instance, GLFWwindow* window) {
    HWND hwnd = glfwGetWin32Window(window);
    HINSTANCE hinstance = GetModuleHandle(NULL);

    WGPUSurfaceDescriptorFromWindowsHWND from_hwnd = {
        .chain = { .sType = WGPUSType_SurfaceDescriptorFromWindowsHWND },
        .hinstance = hinstance,
        .hwnd = hwnd,
    };
    WGPUSurfaceDescriptor descriptor = {
        .nextInChain = (const WGPUChainedStruct*)&from_hwnd,
    };
    return wgpuInstanceCreateSurface(instance, &descriptor);
}

#elif defined(__linux__)

WGPUSurface glfwCreateWGPUSurface(WGPUInstance instance, GLFWwindow* window) {
#ifdef GLFW_USE_WAYLAND
    struct wl_display* display = glfwGetWaylandDisplay();
    struct wl_surface* surface = glfwGetWaylandWindow(window);

    WGPUSurfaceDescriptorFromWaylandSurface from_wayland = {
        .chain = { .sType = WGPUSType_SurfaceDescriptorFromWaylandSurface },
        .display = display,
        .surface = surface,
    };
    WGPUSurfaceDescriptor descriptor = {
        .nextInChain = (const WGPUChainedStruct*)&from_wayland,
    };
#else
    Display* x11_display = glfwGetX11Display();
    Window x11_window = glfwGetX11Window(window);

    WGPUSurfaceDescriptorFromXlibWindow from_xlib = {
        .chain = { .sType = WGPUSType_SurfaceDescriptorFromXlibWindow },
        .display = x11_display,
        .window = x11_window,
    };
    WGPUSurfaceDescriptor descriptor = {
        .nextInChain = (const WGPUChainedStruct*)&from_xlib,
    };
#endif
    return wgpuInstanceCreateSurface(instance, &descriptor);
}

#endif
