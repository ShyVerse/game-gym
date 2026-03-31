#include "script/script_bindings.h"

#include "ecs/components.h"
#include "ecs/world.h"
#include "physics/physics_components.h"
#include "physics/physics_world.h"
#include "script/script_engine.h"
#include "script_types_gen.h"
#include "script/script_types_manual.h"
#include "codegen_bindings_check.h"

#include <flecs.h>
#include <nlohmann/json.hpp>
#include <string>

namespace gg {

namespace {

using json = nlohmann::json;
namespace st = script_types;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/// Find an entity by its Name component value.  Returns an invalid entity if
/// no match is found.
flecs::entity find_entity_by_name(flecs::world& raw, const std::string& name) {
    flecs::entity found;
    raw.each([&](flecs::entity e, const Name& n) {
        if (n.value == name) {
            found = e;
        }
    });
    return found;
}

/// Parse the first element of a JSON array as a string.
std::string parse_first_string_arg(const json& args) {
    if (!args.is_array() || args.empty()) {
        return {};
    }
    return args[0].get<std::string>();
}

/// Build an error-result JSON string.
std::string make_error(const std::string& message) {
    return json{{"error", message}}.dump();
}

// ---------------------------------------------------------------------------
// ECS binding callbacks
// ---------------------------------------------------------------------------

/// __ecs_createEntity(name) -> entity info or error
std::string ecs_create_entity(World& world, const std::string& args_json) {
    try {
        const auto args = json::parse(args_json);
        const auto name = parse_first_string_arg(args);

        if (name.empty()) {
            return make_error("entity name must not be empty");
        }

        auto entity = world.create_entity(name);
        entity.set<Name>({name});
        entity.set<Transform>({});

        json result;
        result["name"] = name;
        result["transform"] = st::to_json(Transform{});
        return result.dump();
    } catch (const std::exception& e) {
        return make_error(std::string("createEntity failed: ") + e.what());
    }
}

/// __ecs_destroyEntity(name) -> {ok:true} or error
std::string ecs_destroy_entity(World& world, const std::string& args_json) {
    try {
        const auto args = json::parse(args_json);
        const auto name = parse_first_string_arg(args);

        auto entity = find_entity_by_name(world.raw(), name);
        if (!entity.is_valid()) {
            return make_error("entity not found: " + name);
        }

        world.destroy_entity(entity);
        return json{{"ok", true}}.dump();
    } catch (const std::exception& e) {
        return make_error(std::string("destroyEntity failed: ") + e.what());
    }
}

/// __ecs_getEntity(name) -> entity info or null
std::string ecs_get_entity(World& world, const std::string& args_json) {
    try {
        const auto args = json::parse(args_json);
        const auto name = parse_first_string_arg(args);

        auto entity = find_entity_by_name(world.raw(), name);
        if (!entity.is_valid()) {
            return "null";
        }

        json info;
        info["name"] = name;

        const auto* t = entity.get<Transform>();
        if (t != nullptr) {
            info["transform"] = st::to_json(*t);
        }

        const auto* v = entity.get<Velocity>();
        if (v != nullptr) {
            info["velocity"] = {
                {"linear", st::to_json(v->linear)},
                {"angular", st::to_json(v->angular)},
            };
        }

        return info.dump();
    } catch (const std::exception& e) {
        return make_error(std::string("getEntity failed: ") + e.what());
    }
}

/// __ecs_listEntities() -> array of entity names
std::string ecs_list_entities(World& world, const std::string& /*args_json*/) {
    try {
        json names = json::array();
        world.raw().each([&](flecs::entity, const Name& n) { names.push_back(n.value); });
        return names.dump();
    } catch (const std::exception& e) {
        return make_error(std::string("listEntities failed: ") + e.what());
    }
}

/// __ecs_setTransform(name, transformObj) -> {ok:true} or error
std::string ecs_set_transform(World& world, const std::string& args_json) {
    try {
        const auto args = json::parse(args_json);
        if (!args.is_array() || args.size() < 2) {
            return make_error("expected [name, transform]");
        }

        const auto name = args[0].get<std::string>();
        const auto new_transform = st::transform_from_json(args[1]);

        auto entity = find_entity_by_name(world.raw(), name);
        if (!entity.is_valid()) {
            return make_error("entity not found: " + name);
        }

        entity.set<Transform>(new_transform);

        // If entity has a RigidBody, mark it for physics sync
        const auto* rb = entity.get<RigidBody>();
        if (rb != nullptr) {
            RigidBody updated_rb{
                .body_id = rb->body_id,
                .sync_to_physics = true,
            };
            entity.set<RigidBody>(updated_rb);
        }

        return json{{"ok", true}}.dump();
    } catch (const std::exception& e) {
        return make_error(std::string("setTransform failed: ") + e.what());
    }
}

/// __ecs_hasComponent(name, componentType) -> true/false or error
std::string ecs_has_component(World& world, const std::string& args_json) {
    try {
        const auto args = json::parse(args_json);
        if (!args.is_array() || args.size() < 2) {
            return make_error("expected [name, componentType]");
        }

        const auto name = args[0].get<std::string>();
        const auto component_type = args[1].get<std::string>();

        auto entity = find_entity_by_name(world.raw(), name);
        if (!entity.is_valid()) {
            return make_error("entity not found: " + name);
        }

        bool has = false;
        if (component_type == "Transform") {
            has = entity.has<Transform>();
        } else if (component_type == "Velocity") {
            has = entity.has<Velocity>();
        } else if (component_type == "Name") {
            has = entity.has<Name>();
        } else if (component_type == "Renderable") {
            has = entity.has<Renderable>();
        } else if (component_type == "RigidBody") {
            has = entity.has<RigidBody>();
        } else {
            return make_error("unknown component type: " + component_type);
        }

        return has ? "true" : "false";
    } catch (const std::exception& e) {
        return make_error(std::string("hasComponent failed: ") + e.what());
    }
}

// ---------------------------------------------------------------------------
// Physics binding callbacks
// ---------------------------------------------------------------------------

/// __physics_addBody(position, rotation, bodyDef) -> bodyId
std::string physics_add_body(PhysicsWorld& physics, const std::string& args_json) {
    try {
        const auto args = json::parse(args_json);
        if (!args.is_array() || args.size() < 3) {
            return make_error("expected [position, rotation, bodyDef]");
        }

        const auto pos = st::vec3_from_json(args[0]);
        const auto rot = st::quat_from_json(args[1]);
        const auto body_def = st::bodydef_from_json(args[2]);

        const uint32_t body_id = physics.add_body(pos, rot, body_def);
        return std::to_string(body_id);
    } catch (const std::exception& e) {
        return make_error(std::string("addBody failed: ") + e.what());
    }
}

/// __physics_removeBody(bodyId) -> {ok:true} or error
std::string physics_remove_body(PhysicsWorld& physics, const std::string& args_json) {
    try {
        const auto args = json::parse(args_json);
        if (!args.is_array() || args.empty()) {
            return make_error("expected [bodyId]");
        }

        const uint32_t body_id = args[0].get<uint32_t>();
        physics.remove_body(body_id);
        return json{{"ok", true}}.dump();
    } catch (const std::exception& e) {
        return make_error(std::string("removeBody failed: ") + e.what());
    }
}

/// __physics_getPosition(bodyId) -> Vec3
std::string physics_get_position(PhysicsWorld& physics, const std::string& args_json) {
    try {
        const auto args = json::parse(args_json);
        if (!args.is_array() || args.empty()) {
            return make_error("expected [bodyId]");
        }

        const uint32_t body_id = args[0].get<uint32_t>();
        const auto pos = physics.get_position(body_id);
        return st::to_json(pos).dump();
    } catch (const std::exception& e) {
        return make_error(std::string("getPosition failed: ") + e.what());
    }
}

/// __physics_setPosition(bodyId, position) -> {ok:true} or error
std::string physics_set_position(PhysicsWorld& physics, const std::string& args_json) {
    try {
        const auto args = json::parse(args_json);
        if (!args.is_array() || args.size() < 2) {
            return make_error("expected [bodyId, position]");
        }

        const uint32_t body_id = args[0].get<uint32_t>();
        const auto pos = st::vec3_from_json(args[1]);
        physics.set_position(body_id, pos);
        return json{{"ok", true}}.dump();
    } catch (const std::exception& e) {
        return make_error(std::string("setPosition failed: ") + e.what());
    }
}

/// __physics_raycast(origin, direction, maxDistance) -> RayHit or null
std::string physics_raycast(PhysicsWorld& physics, const std::string& args_json) {
    try {
        const auto args = json::parse(args_json);
        if (!args.is_array() || args.size() < 3) {
            return make_error("expected [origin, direction, maxDistance]");
        }

        const auto origin = st::vec3_from_json(args[0]);
        const auto direction = st::vec3_from_json(args[1]);
        const float max_distance = args[2].get<float>();

        RayHit hit;
        const bool did_hit = physics.raycast(origin, direction, max_distance, hit);

        if (!did_hit) {
            return "null";
        }

        return st::to_json(hit).dump();
    } catch (const std::exception& e) {
        return make_error(std::string("raycast failed: ") + e.what());
    }
}

/// __physics_contactEvents() -> array of ContactEvent
std::string physics_contact_events(PhysicsWorld& physics, const std::string& /*args_json*/) {
    try {
        json events = json::array();
        for (const auto& ce : physics.contact_events()) {
            events.push_back(st::to_json(ce));
        }
        return events.dump();
    } catch (const std::exception& e) {
        return make_error(std::string("contactEvents failed: ") + e.what());
    }
}

// ---------------------------------------------------------------------------
// JS wrapper code injected into the engine context
// ---------------------------------------------------------------------------

constexpr const char* kJsWrapperCode = R"JS(
const world = {
    createEntity(name) { return __ecs_createEntity(name); },
    destroyEntity(name) { return __ecs_destroyEntity(name); },
    getEntity(name) { return __ecs_getEntity(name); },
    listEntities() { return __ecs_listEntities(); },
    setTransform(name, t) { return __ecs_setTransform(name, t); },
    hasComponent(name, c) { return __ecs_hasComponent(name, c); },
};
const physics = {
    addBody(pos, rot, def) { return __physics_addBody(pos, rot, def); },
    removeBody(id) { return __physics_removeBody(id); },
    getPosition(id) { return __physics_getPosition(id); },
    setPosition(id, pos) { return __physics_setPosition(id, pos); },
    raycast(origin, dir, dist) { return __physics_raycast(origin, dir, dist); },
    contactEvents() { return __physics_contactEvents(); },
};
)JS";

} // anonymous namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void register_script_bindings(ScriptEngine& engine, World& world, PhysicsWorld& physics) {
    // -- ECS bindings ---------------------------------------------------------

    engine.register_function("__ecs_createEntity", [&world](const std::string& args) {
        return ecs_create_entity(world, args);
    });

    engine.register_function("__ecs_destroyEntity", [&world](const std::string& args) {
        return ecs_destroy_entity(world, args);
    });

    engine.register_function("__ecs_getEntity", [&world](const std::string& args) {
        return ecs_get_entity(world, args);
    });

    engine.register_function("__ecs_listEntities", [&world](const std::string& args) {
        return ecs_list_entities(world, args);
    });

    engine.register_function("__ecs_setTransform", [&world](const std::string& args) {
        return ecs_set_transform(world, args);
    });

    engine.register_function("__ecs_hasComponent", [&world](const std::string& args) {
        return ecs_has_component(world, args);
    });

    // -- Physics bindings -----------------------------------------------------

    engine.register_function("__physics_addBody", [&physics](const std::string& args) {
        return physics_add_body(physics, args);
    });

    engine.register_function("__physics_removeBody", [&physics](const std::string& args) {
        return physics_remove_body(physics, args);
    });

    engine.register_function("__physics_getPosition", [&physics](const std::string& args) {
        return physics_get_position(physics, args);
    });

    engine.register_function("__physics_setPosition", [&physics](const std::string& args) {
        return physics_set_position(physics, args);
    });

    engine.register_function("__physics_raycast", [&physics](const std::string& args) {
        return physics_raycast(physics, args);
    });

    engine.register_function("__physics_contactEvents", [&physics](const std::string& args) {
        return physics_contact_events(physics, args);
    });

    // -- Inject JS wrapper objects --------------------------------------------

    engine.execute(kJsWrapperCode, "<script_bindings>");

    gg::codegen::check_all_bindings();
}

} // namespace gg

namespace gg::codegen {
void assert_bound_Vec3() {}
void assert_bound_Quat() {}
void assert_bound_Transform() {}
void assert_bound_Velocity() {}
void assert_bound_BoxShapeDesc() {}
void assert_bound_SphereShapeDesc() {}
void assert_bound_CapsuleShapeDesc() {}
void assert_bound_ContactEvent() {}
void assert_bound_RayHit() {}
} // namespace gg::codegen
