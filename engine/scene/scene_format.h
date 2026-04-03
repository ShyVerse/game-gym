#pragma once

#include "ecs/components.h"

#include <string>
#include <vector>

namespace gg {

struct SceneEntityData {
    std::string name;
    Transform transform{};
    std::string mesh_asset_path;
    std::string script_asset_path;
};

struct SceneDocument {
    std::vector<SceneEntityData> entities;
};

struct SceneDocumentResult {
    bool ok = false;
    SceneDocument document;
    std::string error;
};

struct SceneLoadSummary {
    bool ok = false;
    std::string error;
    size_t entity_count = 0;
    std::vector<std::string> mesh_assets;
    std::vector<std::string> script_assets;
};

} // namespace gg
