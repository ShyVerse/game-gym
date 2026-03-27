#pragma once
#include <string>

namespace gg {

struct Vec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;
};

struct Quat {
    float x = 0.0f, y = 0.0f, z = 0.0f, w = 1.0f;
};

struct Transform {
    Vec3 position{};
    Quat rotation{};
    Vec3 scale{1.0f, 1.0f, 1.0f};
};

struct Velocity {
    Vec3 linear{};
    Vec3 angular{};
};

struct Name {
    std::string value;
};

struct Renderable {};

} // namespace gg
