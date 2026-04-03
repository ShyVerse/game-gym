#pragma once

#include "scene/scene_format.h"

#include <filesystem>
#include <string>

namespace gg {

class World;

SceneLoadSummary load_scene_into_world(const std::string& scene_path,
                                       const std::filesystem::path& project_root,
                                       World& world);

} // namespace gg
