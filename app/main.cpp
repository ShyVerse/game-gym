#include "core/engine.h"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>

int main(int argc, char* argv[]) {
    const char* shader_path = "shaders/triangle.wgsl";
    const char* model_path = nullptr;

    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--model" && i + 1 < argc) {
            model_path = argv[++i];
        } else if (i == 1 && argv[i][0] != '-') {
            // Legacy positional argument: first arg is shader path
            shader_path = argv[i];
        }
    }

    try {
        auto engine = gg::Engine::create({
            .title = "Game-Gym Engine",
            .width = 1280,
            .height = 720,
            .resizable = true,
            .shader_path = shader_path,
            .model_path = model_path ? model_path : "",
        });
        engine->run();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Fatal error: %s\n", e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
