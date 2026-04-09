#pragma once

#include <array>
#include <span>
#include <string_view>

namespace gg {

struct ComponentMeta {
    std::string_view stable_id;
    std::string_view display_name;
    std::string_view category;
    bool serializable;
    bool editor_visible;
    bool addable;
};

inline constexpr std::array<ComponentMeta, 5> kBuiltInComponents{{
    {
        .stable_id = "transform",
        .display_name = "Transform",
        .category = "Core",
        .serializable = true,
        .editor_visible = true,
        .addable = true,
    },
    {
        .stable_id = "mesh_renderer",
        .display_name = "Mesh Renderer",
        .category = "Rendering",
        .serializable = true,
        .editor_visible = true,
        .addable = true,
    },
    {
        .stable_id = "script",
        .display_name = "Script",
        .category = "Scripting",
        .serializable = true,
        .editor_visible = true,
        .addable = true,
    },
    {
        .stable_id = "rigid_body",
        .display_name = "Rigid Body",
        .category = "Physics",
        .serializable = true,
        .editor_visible = true,
        .addable = true,
    },
    {
        .stable_id = "velocity",
        .display_name = "Velocity",
        .category = "Physics",
        .serializable = true,
        .editor_visible = true,
        .addable = true,
    },
}};

inline std::span<const ComponentMeta> built_in_components() {
    return kBuiltInComponents;
}

inline const ComponentMeta* find_component_meta(std::string_view stable_id) {
    for (const auto& meta : kBuiltInComponents) {
        if (meta.stable_id == stable_id) {
            return &meta;
        }
    }
    return nullptr;
}

inline bool is_addable_component(std::string_view stable_id) {
    const ComponentMeta* meta = find_component_meta(stable_id);
    return meta != nullptr && meta->addable;
}

} // namespace gg
