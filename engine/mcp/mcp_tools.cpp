#include "mcp/mcp_tools.h"
#include "mcp/mcp_server.h"
#include "ecs/world.h"
#include "ecs/components.h"
#include "physics/physics_world.h"
#include "physics/physics_components.h"
#include <nlohmann/json.hpp>
#include <flecs.h>
#include <string>

namespace gg {

namespace {

// Helper: find entity by Name component value
flecs::entity find_entity_by_name(flecs::world& raw, const std::string& name) {
    flecs::entity found;
    raw.each([&](flecs::entity e, const Name& n) {
        if (n.value == name) {
            found = e;
        }
    });
    return found;
}

std::string list_entities_handler(World& world, const std::string& /*args_json*/) {
    using json = nlohmann::json;
    json result = json::array();

    world.raw().each([&](flecs::entity e, const Name& n) {
        json entry;
        entry["name"] = n.value;

        const auto* t = e.get<Transform>();
        if (t) {
            entry["position"] = {
                {"x", t->position.x},
                {"y", t->position.y},
                {"z", t->position.z}
            };
        }
        result.push_back(entry);
    });

    return result.dump();
}

std::string get_entity_handler(World& world, const std::string& args_json) {
    using json = nlohmann::json;

    json args;
    try {
        args = json::parse(args_json);
    } catch (...) {
        return json{{"error", "Invalid JSON"}}.dump();
    }

    const auto name = args.value("name", std::string{});
    auto entity = find_entity_by_name(world.raw(), name);

    if (!entity.is_valid()) {
        return json{{"error", "Entity not found: " + name}}.dump();
    }

    json info;
    info["name"] = name;

    const auto* t = entity.get<Transform>();
    if (t) {
        info["position"] = {{"x", t->position.x}, {"y", t->position.y}, {"z", t->position.z}};
        info["rotation"] = {{"x", t->rotation.x}, {"y", t->rotation.y},
                            {"z", t->rotation.z}, {"w", t->rotation.w}};
        info["scale"]    = {{"x", t->scale.x}, {"y", t->scale.y}, {"z", t->scale.z}};
    }

    const auto* v = entity.get<Velocity>();
    if (v) {
        info["velocity"] = {{"x", v->linear.x}, {"y", v->linear.y}, {"z", v->linear.z}};
    }

    const auto* rb = entity.get<RigidBody>();
    if (rb) {
        info["rigid_body"] = {{"body_id", rb->body_id}, {"sync_to_physics", rb->sync_to_physics}};
    }

    return info.dump();
}

std::string create_entity_handler(World& world, const std::string& args_json) {
    using json = nlohmann::json;

    json args;
    try {
        args = json::parse(args_json);
    } catch (...) {
        return json{{"error", "Invalid JSON"}}.dump();
    }

    const auto name = args.value("name", std::string{});
    if (name.empty()) {
        return json{{"error", "name is required"}}.dump();
    }

    auto entity = world.create_entity(name);

    Transform t;
    if (args.contains("position")) {
        const auto& pos = args["position"];
        t.position.x = pos.value("x", 0.0f);
        t.position.y = pos.value("y", 0.0f);
        t.position.z = pos.value("z", 0.0f);
    }
    entity.set<Transform>(t);

    Name n;
    n.value = name;
    entity.set<Name>(n);

    return json{{"created", name}}.dump();
}

std::string set_transform_handler(World& world, const std::string& args_json) {
    using json = nlohmann::json;

    json args;
    try {
        args = json::parse(args_json);
    } catch (...) {
        return json{{"error", "Invalid JSON"}}.dump();
    }

    const auto name = args.value("name", std::string{});
    auto entity = find_entity_by_name(world.raw(), name);

    if (!entity.is_valid()) {
        return json{{"error", "Entity not found: " + name}}.dump();
    }

    const auto* existing = entity.get<Transform>();
    Transform t = existing ? *existing : Transform{};

    if (args.contains("position")) {
        const auto& pos = args["position"];
        t.position.x = pos.value("x", t.position.x);
        t.position.y = pos.value("y", t.position.y);
        t.position.z = pos.value("z", t.position.z);
    }
    if (args.contains("rotation")) {
        const auto& rot = args["rotation"];
        t.rotation.x = rot.value("x", t.rotation.x);
        t.rotation.y = rot.value("y", t.rotation.y);
        t.rotation.z = rot.value("z", t.rotation.z);
        t.rotation.w = rot.value("w", t.rotation.w);
    }
    if (args.contains("scale")) {
        const auto& sc = args["scale"];
        t.scale.x = sc.value("x", t.scale.x);
        t.scale.y = sc.value("y", t.scale.y);
        t.scale.z = sc.value("z", t.scale.z);
    }

    entity.set<Transform>(t);

    // Mark sync if entity has a RigidBody
    const auto* rb = entity.get<RigidBody>();
    if (rb) {
        RigidBody new_rb = *rb;
        new_rb.sync_to_physics = true;
        entity.set<RigidBody>(new_rb);
    }

    return json{{"updated", name}}.dump();
}

std::string remove_entity_handler(World& world, const std::string& args_json) {
    using json = nlohmann::json;

    json args;
    try {
        args = json::parse(args_json);
    } catch (...) {
        return json{{"error", "Invalid JSON"}}.dump();
    }

    const auto name = args.value("name", std::string{});
    auto entity = find_entity_by_name(world.raw(), name);

    if (!entity.is_valid()) {
        return json{{"error", "Entity not found: " + name}}.dump();
    }

    world.destroy_entity(entity);
    return json{{"removed", name}}.dump();
}

std::string raycast_handler(PhysicsWorld& physics, const std::string& args_json) {
    using json = nlohmann::json;

    json args;
    try {
        args = json::parse(args_json);
    } catch (...) {
        return json{{"error", "Invalid JSON"}}.dump();
    }

    Vec3 origin{0.0f, 0.0f, 0.0f};
    Vec3 direction{0.0f, 0.0f, 1.0f};
    float max_distance = 100.0f;

    if (args.contains("origin")) {
        const auto& o = args["origin"];
        origin.x = o.value("x", 0.0f);
        origin.y = o.value("y", 0.0f);
        origin.z = o.value("z", 0.0f);
    }
    if (args.contains("direction")) {
        const auto& d = args["direction"];
        direction.x = d.value("x", 0.0f);
        direction.y = d.value("y", 0.0f);
        direction.z = d.value("z", 1.0f);
    }
    max_distance = args.value("max_distance", 100.0f);

    RayHit hit;
    const bool did_hit = physics.raycast(origin, direction, max_distance, hit);

    json result;
    result["hit"] = did_hit;
    if (did_hit) {
        result["body_id"]  = hit.body_id;
        result["fraction"] = hit.fraction;
        result["point"]    = {{"x", hit.point.x}, {"y", hit.point.y}, {"z", hit.point.z}};
        result["normal"]   = {{"x", hit.normal.x}, {"y", hit.normal.y}, {"z", hit.normal.z}};
    }
    return result.dump();
}

} // anonymous namespace

void register_mcp_tools(McpServer& server, World& world, PhysicsWorld& physics) {
    server.register_tool({
        "list_entities",
        "List all entities in the world with their positions",
        R"({"type":"object","properties":{}})",
        [&world](const std::string& args) { return list_entities_handler(world, args); }
    });

    server.register_tool({
        "get_entity",
        "Get detailed info about an entity by name",
        R"({"type":"object","properties":{"name":{"type":"string"}},"required":["name"]})",
        [&world](const std::string& args) { return get_entity_handler(world, args); }
    });

    server.register_tool({
        "create_entity",
        "Create a new entity with a name and optional position",
        R"({"type":"object","properties":{"name":{"type":"string"},"position":{"type":"object"}},"required":["name"]})",
        [&world](const std::string& args) { return create_entity_handler(world, args); }
    });

    server.register_tool({
        "set_transform",
        "Set the transform (position/rotation/scale) of an entity by name",
        R"({"type":"object","properties":{"name":{"type":"string"},"position":{"type":"object"},"rotation":{"type":"object"},"scale":{"type":"object"}},"required":["name"]})",
        [&world](const std::string& args) { return set_transform_handler(world, args); }
    });

    server.register_tool({
        "remove_entity",
        "Remove an entity by name",
        R"({"type":"object","properties":{"name":{"type":"string"}},"required":["name"]})",
        [&world](const std::string& args) { return remove_entity_handler(world, args); }
    });

    server.register_tool({
        "raycast",
        "Cast a ray and return hit information",
        R"({"type":"object","properties":{"origin":{"type":"object"},"direction":{"type":"object"},"max_distance":{"type":"number"}}})",
        [&physics](const std::string& args) { return raycast_handler(physics, args); }
    });
}

} // namespace gg
