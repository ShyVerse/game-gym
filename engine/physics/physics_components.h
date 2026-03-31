#pragma once

#include "ecs/components.h"
#include "script/scriptable.h"

#include <climits>
#include <cstdint>
#include <variant>
#include <vector>

namespace gg {

enum class GG_SCRIPTABLE_ENUM MotionType : uint8_t { Static, Dynamic, Kinematic };
enum class PhysicsLayer : uint8_t { Static = 0, Dynamic = 1, Trigger = 2 };

struct GG_SCRIPTABLE BoxShapeDesc {
    float half_x = 0.5f;
    float half_y = 0.5f;
    float half_z = 0.5f;
};
struct GG_SCRIPTABLE SphereShapeDesc {
    float radius = 0.5f;
};
struct GG_SCRIPTABLE CapsuleShapeDesc {
    float half_height = 0.5f;
    float radius = 0.25f;
};

using ColliderShape = std::variant<BoxShapeDesc, SphereShapeDesc, CapsuleShapeDesc>;

struct BodyDef {
    ColliderShape shape = BoxShapeDesc{};
    MotionType motion_type = MotionType::Dynamic;
    PhysicsLayer layer = PhysicsLayer::Dynamic;
    bool is_sensor = false;
    float friction = 0.2f;
    float restitution = 0.0f;
};

struct RigidBody {
    uint32_t body_id = UINT32_MAX;
    bool sync_to_physics = false;
};

enum class GG_SCRIPTABLE_ENUM ContactType : uint8_t { Begin, Persist, End };

struct GG_SCRIPTABLE ContactEvent {
    uint32_t body_id_a = 0;
    uint32_t body_id_b = 0;
    ContactType type = ContactType::Begin;
    Vec3 point{};
    Vec3 normal{};
};

struct GG_SCRIPTABLE RayHit {
    uint32_t body_id = UINT32_MAX;
    float fraction = 1.0f;
    Vec3 point{};
    Vec3 normal{};
};

struct PhysicsConfig {
    Vec3 gravity = {0.0f, -9.81f, 0.0f};
    uint32_t max_bodies = 4096;
    int substeps = 2;
};

} // namespace gg
