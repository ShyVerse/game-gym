#include "project/project_config.h"

#include "assets/asset_paths.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>

namespace gg {
namespace {

using json = nlohmann::json;

std::string read_text_file(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return {};
    }

    std::ostringstream stream;
    stream << file.rdbuf();
    if (file.bad()) {
        return {};
    }

    return stream.str();
}

} // namespace

ProjectConfigLoadResult load_project_config(const std::string& path) {
    const auto project_file = std::filesystem::absolute(path).lexically_normal();
    const auto contents = read_text_file(project_file);
    if (contents.empty()) {
        return {.ok = false, .error = "cannot read project file: " + project_file.string()};
    }

    json doc;
    try {
        doc = json::parse(contents);
    } catch (const std::exception& e) {
        return {.ok = false, .error = std::string("invalid project json: ") + e.what()};
    }

    if (!doc.contains("startup_scene")) {
        return {.ok = false, .error = "project file is missing startup_scene"};
    }

    ProjectConfig config;
    config.project_file = project_file;
    config.project_root = project_file.parent_path();
    config.name = doc.value("name", project_file.stem().string());

    std::string startup_scene_value;
    try {
        startup_scene_value = doc.at("startup_scene").get<std::string>();
    } catch (const std::exception& e) {
        return {.ok = false, .error = std::string("invalid startup_scene: ") + e.what()};
    }

    auto startup_scene = resolve_project_path(config.project_root, startup_scene_value);
    if (!startup_scene.ok) {
        return {.ok = false, .error = startup_scene.error};
    }
    config.startup_scene = startup_scene.path;

    std::string assets_dir_value = "assets";
    try {
        assets_dir_value = doc.value("assets_dir", std::string("assets"));
    } catch (const std::exception& e) {
        return {.ok = false, .error = std::string("invalid assets_dir: ") + e.what()};
    }

    auto assets_dir = resolve_project_path(config.project_root, assets_dir_value);
    if (!assets_dir.ok) {
        return {.ok = false, .error = assets_dir.error};
    }
    config.assets_dir = assets_dir.path;

    std::string scripts_dir_value = "assets/scripts";
    try {
        scripts_dir_value = doc.value("scripts_dir", std::string("assets/scripts"));
    } catch (const std::exception& e) {
        return {.ok = false, .error = std::string("invalid scripts_dir: ") + e.what()};
    }

    auto scripts_dir = resolve_project_path(config.project_root, scripts_dir_value);
    if (!scripts_dir.ok) {
        return {.ok = false, .error = scripts_dir.error};
    }
    config.scripts_dir = scripts_dir.path;

    return {.ok = true, .config = std::move(config)};
}

} // namespace gg
