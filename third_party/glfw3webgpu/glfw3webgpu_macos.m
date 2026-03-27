#include "glfw3webgpu.h"
#include <webgpu.h>

#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3native.h>

#include <Foundation/Foundation.h>
#include <QuartzCore/CAMetalLayer.h>

WGPUSurface glfwCreateWGPUSurface(WGPUInstance instance, GLFWwindow* window) {
    id metal_layer = [CAMetalLayer layer];
    NSWindow* ns_window = glfwGetCocoaWindow(window);
    [ns_window.contentView setWantsLayer:YES];
    [ns_window.contentView setLayer:metal_layer];

    WGPUSurfaceSourceMetalLayer source = {
        .chain = { .sType = WGPUSType_SurfaceSourceMetalLayer },
        .layer = metal_layer,
    };
    WGPUSurfaceDescriptor descriptor = {
        .nextInChain = (const WGPUChainedStruct*)&source,
    };
    return wgpuInstanceCreateSurface(instance, &descriptor);
}
