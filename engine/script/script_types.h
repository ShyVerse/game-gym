#pragma once

#include "ecs/components.h"
#include "physics/physics_components.h"

#include <nlohmann/json.hpp>
#include <string>

namespace gg::script_types {

// ---- Vec3 ----------------------------------------------------------------

inline nlohmann::json to_json(const Vec3& v) {
    return {{"x", v.x}, {"y", v.y}, {"z", v.z}};
}

inline Vec3 vec3_from_json(const nlohmann::json& j) {
    if (!j.is_object()) {
        return {};
    }
    return {
        j.value("x", 0.0f),
        j.value("y", 0.0f),
        j.value("z", 0.0f),
    };
}

// ---- Quat ----------------------------------------------------------------

inline nlohmann::json to_json(const Quat& q) {
    return {{"x", q.x}, {"y", q.y}, {"z", q.z}, {"w", q.w}};
}

inline Quat quat_from_json(const nlohmann::json& j) {
    if (!j.is_object()) {
        return {};
    }
    return {
        j.value("x", 0.0f),
        j.value("y", 0.0f),
        j.value("z", 0.0f),
        j.value("w", 1.0f),
    };
}

// ---- Transform -----------------------------------------------------------

inline nlohmann::json to_json(const Transform& t) {
    return {
        {"position", to_json(t.position)},
        {"rotation", to_json(t.rotation)},
        {"scale", to_json(t.scale)},
    };
}

inline Transform transform_from_json(const nlohmann::json& j) {
    if (!j.is_object()) {
        return {};
    }
    const auto pos_j = j.value("position", nlohmann::json::object());
    const auto rot_j = j.value("rotation", nlohmann::json::object());
    const auto scl_j = j.value("scale", nlohmann::json::object());

    return {
        .position = vec3_from_json(pos_j),
        .rotation = quat_from_json(rot_j),
        .scale = vec3_from_json(scl_j),
    };
}

// ---- MotionType helpers --------------------------------------------------

inline std::string motion_type_to_string(MotionType mt) {
    switch (mt) {
    case MotionType::Static:
        return "static";
    case MotionType::Dynamic:
        return "dynamic";
    case MotionType::Kinematic:
        return "kinematic";
    default:
        return "dynamic";
    }
}

inline MotionType motion_type_from_string(const std::string& s) {
    if (s == "static")
        return MotionType::Static;
    if (s == "kinematic")
        return MotionType::Kinematic;
    return MotionType::Dynamic;
}

// ---- BodyDef -------------------------------------------------------------

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

// ---- ContactType helper --------------------------------------------------

inline std::string contact_type_to_string(ContactType ct) {
    switch (ct) {
    case ContactType::Begin:
        return "begin";
    case ContactType::Persist:
        return "persist";
    case ContactType::End:
        return "end";
    default:
        return "begin";
    }
}

// ---- ContactEvent (output-only) ------------------------------------------

inline nlohmann::json to_json(const ContactEvent& ce) {
    return {
        {"bodyIdA", ce.body_id_a},
        {"bodyIdB", ce.body_id_b},
        {"type", contact_type_to_string(ce.type)},
        {"point", to_json(ce.point)},
        {"normal", to_json(ce.normal)},
    };
}

// ---- RayHit (output-only) ------------------------------------------------

inline nlohmann::json to_json(const RayHit& hit) {
    return {
        {"bodyId", hit.body_id},
        {"fraction", hit.fraction},
        {"point", to_json(hit.point)},
        {"normal", to_json(hit.normal)},
    };
}

} // namespace gg::script_types
