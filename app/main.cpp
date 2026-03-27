#include "core/window.h"
#include "renderer/gpu_context.h"
#include "renderer/renderer.h"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cstdio>
#include <cstdlib>

static std::string read_file(const char* path)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error(std::string("Cannot open file: ") + path);
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

int main(int argc, char* argv[])
{
    // Determine shader path: default to a path relative to working directory.
    // Run from the project root, or pass the path as the first argument.
    const char* shader_path = (argc > 1) ? argv[1] : "shaders/triangle.wgsl";

    try {
        // 1. Window
        auto window = gg::Window::create({
            .title    = "Game-Gym Engine",
            .width    = 1280,
            .height   = 720,
            .resizable = true,
        });

        // 2. GPU context
        auto gpu = gg::GpuContext::create(*window);

        // 3. Shader source
        std::string shader_source = read_file(shader_path);

        // 4. Renderer
        auto renderer = gg::Renderer::create(*gpu, shader_source);

        // 5. Main loop
        while (!window->should_close()) {
            window->poll_events();

            if (renderer->begin_frame()) {
                renderer->draw_triangle();
                renderer->end_frame();
            }
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Fatal error: %s\n", e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
