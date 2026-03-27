#ifndef GLFW3_WEBGPU_H
#define GLFW3_WEBGPU_H

#include <webgpu.h>
#include <GLFW/glfw3.h>

#ifdef __cplusplus
extern "C" {
#endif

WGPUSurface glfwCreateWGPUSurface(WGPUInstance instance, GLFWwindow* window);

#ifdef __cplusplus
}
#endif

#endif // GLFW3_WEBGPU_H
