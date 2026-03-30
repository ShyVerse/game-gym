#include "core/engine.h"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>

int main(int argc, char* argv[]) {
    const char* shader_path = (argc > 1) ? argv[1] : "shaders/triangle.wgsl";

    try {
        auto engine = gg::Engine::create({
            .title = "Game-Gym Engine",
            .width = 1280,
            .height = 720,
            .resizable = true,
            .shader_path = shader_path,
        });
        engine->run();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Fatal error: %s\n", e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
