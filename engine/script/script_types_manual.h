#pragma once

// Manual supplement for BodyDef JSON conversions.
// BodyDef is excluded from GG_SCRIPTABLE codegen because it contains a
// std::variant field.  The conversions here use the generated enum helpers
// (motion_type_to_string / motion_type_from_string) from script_types_gen.h.
//
#include "physics/physics_components.h"
#include "script_types_gen.h"

#include <nlohmann/json.hpp>
#include <string>

namespace gg::script_types {

// ---- BodyDef (manual — variant shape field) --------------------------------

inline nlohmann::json to_json(const BodyDef& bd) {
    nlohmann::json j;

    j["motionType"] = motion_type_to_string(bd.motion_type);
    j["isSensor"] = bd.is_sensor;
    j["friction"] = bd.friction;
    j["restitution"] = bd.restitution;

    std::visit(
        [&j](const auto& shape) {
            using T = std::decay_t<decltype(shape)>;
            if constexpr (std::is_same_v<T, BoxShapeDesc>) {
                j["shape"] = "box";
                j["halfExtents"] = nlohmann::json{
                    {"x", shape.half_x},
                    {"y", shape.half_y},
                    {"z", shape.half_z},
                };
            } else if constexpr (std::is_same_v<T, SphereShapeDesc>) {
                j["shape"] = "sphere";
                j["radius"] = shape.radius;
            } else if constexpr (std::is_same_v<T, CapsuleShapeDesc>) {
                j["shape"] = "capsule";
                j["halfHeight"] = shape.half_height;
                j["radius"] = shape.radius;
            }
        },
        bd.shape);

    return j;
}

inline BodyDef bodydef_from_json(const nlohmann::json& j) {
    if (!j.is_object()) {
        return {};
    }

    BodyDef bd{};

    bd.motion_type = motion_type_from_string(j.value("motionType", "dynamic"));
    bd.is_sensor = j.value("isSensor", false);
    bd.friction = j.value("friction", 0.2f);
    bd.restitution = j.value("restitution", 0.0f);

    const auto shape_str = j.value("shape", "box");

    if (shape_str == "sphere") {
        bd.shape = SphereShapeDesc{j.value("radius", 0.5f)};
    } else if (shape_str == "capsule") {
        bd.shape = CapsuleShapeDesc{
            j.value("halfHeight", 0.5f),
            j.value("radius", 0.25f),
        };
    } else {
        // Default: box
        const auto he = (j.contains("halfExtents") && j["halfExtents"].is_object())
                            ? j["halfExtents"]
                            : nlohmann::json::object();
        bd.shape = BoxShapeDesc{
            he.value("x", 0.5f),
            he.value("y", 0.5f),
            he.value("z", 0.5f),
        };
    }

    return bd;
}

} // namespace gg::script_types
