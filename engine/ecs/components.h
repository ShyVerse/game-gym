#pragma once
#include "script/scriptable.h"

#include <string>

namespace gg {

struct GG_SCRIPTABLE Vec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;
};

struct GG_SCRIPTABLE Quat {
    float x = 0.0f, y = 0.0f, z = 0.0f, w = 1.0f;
};

struct GG_SCRIPTABLE Transform {
    Vec3 position{};
    Quat rotation{};
    Vec3 scale{1.0f, 1.0f, 1.0f};
};

struct GG_SCRIPTABLE Velocity {
    Vec3 linear{};
    Vec3 angular{};
};

struct Name {
    std::string value;
};

struct Renderable {
    std::string mesh_asset_path;
};

struct ScriptComponent {
    std::string script_asset_path;
};

} // namespace gg
