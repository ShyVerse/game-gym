#include "scene/scene_loader.h"

#include "assets/asset_paths.h"
#include "ecs/components.h"
#include "ecs/world.h"
#include "scene/scene_serializer.h"

#include <set>

namespace gg {

SceneLoadSummary load_scene_into_world(const std::string& scene_path,
                                       const std::filesystem::path& project_root,
                                       World& world) {
    auto scene_result = load_scene_document(scene_path);
    if (!scene_result.ok) {
        return {.ok = false, .error = scene_result.error};
    }

    std::set<std::string> mesh_assets;
    std::set<std::string> script_assets;

    for (const auto& entity_data : scene_result.document.entities) {
        auto entity = world.create_entity(entity_data.name);
        entity.set<Name>({entity_data.name});
        entity.set<Transform>(entity_data.transform);

        if (!entity_data.mesh_asset_path.empty()) {
            auto resolved = resolve_project_path(project_root, entity_data.mesh_asset_path);
            if (resolved.ok) {
                const auto path = resolved.path.string();
                entity.set<Renderable>({.mesh_asset_path = path});
                mesh_assets.insert(path);
            }
        }

        if (!entity_data.script_asset_path.empty()) {
            auto resolved = resolve_project_path(project_root, entity_data.script_asset_path);
            if (resolved.ok) {
                const auto path = resolved.path.string();
                entity.set<ScriptComponent>({.script_asset_path = path});
                script_assets.insert(path);
            }
        }
    }

    SceneLoadSummary summary;
    summary.ok = true;
    summary.entity_count = scene_result.document.entities.size();
    summary.mesh_assets.assign(mesh_assets.begin(), mesh_assets.end());
    summary.script_assets.assign(script_assets.begin(), script_assets.end());
    return summary;
}

} // namespace gg
