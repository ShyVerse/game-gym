#include "core/engine.h"
#include "version/version_info.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string>

int main(int argc, char* argv[]) {
    const char* shader_path = "shaders/triangle.wgsl";
    const char* model_path = nullptr;
    std::string project_file;
    std::string startup_scene_override;
    bool enable_mcp = false;
    uint16_t mcp_port = 9315;

    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--version") {
            std::printf("game-gym %.*s (%.*s)\n",
                        int(gg::build_version::display_version().size()),
                        gg::build_version::display_version().data(),
                        int(gg::build_version::git_describe().size()),
                        gg::build_version::git_describe().data());
            return EXIT_SUCCESS;
        } else if (std::string(argv[i]) == "--model" && i + 1 < argc) {
            model_path = argv[++i];
        } else if (std::string(argv[i]) == "--project" && i + 1 < argc) {
            project_file = argv[++i];
        } else if (std::string(argv[i]) == "--scene" && i + 1 < argc) {
            startup_scene_override = argv[++i];
        } else if (std::string(argv[i]) == "--mcp-port" && i + 1 < argc) {
            mcp_port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (std::string(argv[i]) == "--mcp") {
            enable_mcp = true;
        } else if (i == 1 && argv[i][0] != '-') {
            // Legacy positional argument: first arg is shader path
            shader_path = argv[i];
        }
    }

    const bool has_default_project = std::filesystem::exists("project.ggym");
    const bool use_project_boot =
        !project_file.empty() || (!has_default_project ? false : model_path == nullptr);
    if (project_file.empty() && use_project_boot) {
        project_file = "project.ggym";
    }

    try {
        auto engine = gg::Engine::create({
            .title = "Game-Gym Engine",
            .width = 1280,
            .height = 720,
            .resizable = true,
            .shader_path = shader_path,
            .model_path = model_path ? model_path : "",
            .project_file = project_file,
            .startup_scene_override = startup_scene_override,
            .enable_project_boot = use_project_boot || !startup_scene_override.empty(),
            .enable_mcp = enable_mcp,
            .mcp_port = mcp_port,
            .enable_scripts = use_project_boot || !startup_scene_override.empty(),
        });
        engine->run();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Fatal error: %s\n", e.what());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
