#pragma once

#include <filesystem>
#include <string>

namespace gg {

struct ProjectConfig {
    std::string name = "game-gym";
    std::filesystem::path project_file;
    std::filesystem::path project_root;
    std::filesystem::path startup_scene;
    std::filesystem::path assets_dir;
    std::filesystem::path scripts_dir;
};

struct ProjectConfigLoadResult {
    bool ok = false;
    ProjectConfig config;
    std::string error;
};

ProjectConfigLoadResult load_project_config(const std::string& path);

} // namespace gg
