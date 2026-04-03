#include "scene/scene_serializer.h"

#include "script_types_gen.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>

namespace gg {
namespace {

using json = nlohmann::json;
namespace st = script_types;

json entity_to_json(const SceneEntityData& entity) {
    json out;
    out["name"] = entity.name;
    out["transform"] = st::to_json(entity.transform);
    if (!entity.mesh_asset_path.empty()) {
        out["mesh_asset"] = entity.mesh_asset_path;
    }
    if (!entity.script_asset_path.empty()) {
        out["script_asset"] = entity.script_asset_path;
    }
    return out;
}

SceneEntityData entity_from_json(const json& value) {
    SceneEntityData entity;
    entity.name = value.at("name").get<std::string>();
    if (value.contains("transform")) {
        entity.transform = st::transform_from_json(value.at("transform"));
    }
    entity.mesh_asset_path = value.value("mesh_asset", std::string{});
    entity.script_asset_path = value.value("script_asset", std::string{});
    return entity;
}

std::string read_text_file(const std::string& path) {
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

SceneDocumentResult load_scene_document(const std::string& path) {
    const auto text = read_text_file(path);
    if (text.empty()) {
        return {.ok = false, .error = "cannot read scene file: " + path};
    }

    json doc;
    try {
        doc = json::parse(text);
    } catch (const std::exception& e) {
        return {.ok = false, .error = std::string("invalid scene json: ") + e.what()};
    }

    if (!doc.contains("entities") || !doc["entities"].is_array()) {
        return {.ok = false, .error = "scene json must contain an entities array"};
    }

    SceneDocument scene;
    try {
        for (const auto& value : doc["entities"]) {
            scene.entities.push_back(entity_from_json(value));
        }
    } catch (const std::exception& e) {
        return {.ok = false, .error = std::string("invalid scene entity: ") + e.what()};
    }

    return {.ok = true, .document = std::move(scene)};
}

std::string serialize_scene_document(const SceneDocument& document) {
    json out;
    out["entities"] = json::array();
    for (const auto& entity : document.entities) {
        out["entities"].push_back(entity_to_json(entity));
    }
    return out.dump(2);
}

bool save_scene_document(const SceneDocument& document,
                         const std::string& path,
                         std::string* error) {
    std::ofstream file(path);
    if (!file.is_open()) {
        if (error != nullptr) {
            *error = "cannot write scene file: " + path;
        }
        return false;
    }

    file << serialize_scene_document(document);
    if (!file.good()) {
        if (error != nullptr) {
            *error = "failed to write scene file: " + path;
        }
        return false;
    }

    return true;
}

} // namespace gg
