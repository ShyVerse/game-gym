#pragma once

#include "scene/scene_format.h"

#include <string>

namespace gg {

SceneDocumentResult load_scene_document(const std::string& path);
std::string serialize_scene_document(const SceneDocument& document);
bool save_scene_document(const SceneDocument& document,
                         const std::string& path,
                         std::string* error = nullptr);

} // namespace gg
