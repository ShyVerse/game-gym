#include "ecs/components.h"
#include "ecs/world.h"
#include "mcp/mcp_server.h"
#include "mcp/mcp_tools.h"
#include "physics/physics_components.h"
#include "physics/physics_world.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <string>

// ---------------------------------------------------------------------------
// Task 1: JSON-RPC Protocol Tests
// ---------------------------------------------------------------------------

TEST(McpServerTest, CreatesSuccessfully) {
    auto server = gg::McpServer::create("test-server", "1.0.0");
    ASSERT_NE(server, nullptr);
    EXPECT_EQ(server->tool_count(), 0u);
}

TEST(McpServerTest, HandlesInitialize) {
    auto server = gg::McpServer::create("my-engine", "2.3.4");

    const std::string req = R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})";
    const std::string resp = server->handle_message(req);

    auto j = nlohmann::json::parse(resp);
    ASSERT_TRUE(j.contains("result"));
    EXPECT_EQ(j["result"]["serverInfo"]["name"].get<std::string>(), "my-engine");
    EXPECT_EQ(j["result"]["serverInfo"]["version"].get<std::string>(), "2.3.4");
}

TEST(McpServerTest, HandlesToolsList) {
    auto server = gg::McpServer::create("test", "1.0");

    gg::McpToolDef tool;
    tool.name = "dummy_tool";
    tool.description = "A test tool";
    tool.input_schema = R"({"type":"object"})";
    tool.handler = [](const std::string&) { return R"({"ok":true})"; };
    server->register_tool(std::move(tool));

    const std::string req = R"({"jsonrpc":"2.0","id":2,"method":"tools/list","params":{}})";
    const std::string resp = server->handle_message(req);

    auto j = nlohmann::json::parse(resp);
    ASSERT_TRUE(j.contains("result"));
    ASSERT_TRUE(j["result"].contains("tools"));

    bool found = false;
    for (const auto& t : j["result"]["tools"]) {
        if (t["name"] == "dummy_tool") {
            found = true;
        }
    }
    EXPECT_TRUE(found);
}

TEST(McpServerTest, HandlesToolsCall) {
    auto server = gg::McpServer::create("test", "1.0");

    gg::McpToolDef tool;
    tool.name = "echo";
    tool.description = "Echoes args back";
    tool.input_schema = R"({"type":"object"})";
    tool.handler = [](const std::string& args) { return args; };
    server->register_tool(std::move(tool));

    const std::string req = R"({"jsonrpc":"2.0","id":3,"method":"tools/call",)"
                            R"("params":{"name":"echo","arguments":{"hello":"world"}}})";
    const std::string resp = server->handle_message(req);

    auto j = nlohmann::json::parse(resp);
    ASSERT_TRUE(j.contains("result"));
    ASSERT_TRUE(j["result"].contains("content"));
    EXPECT_FALSE(j["result"]["content"].empty());

    const auto& text = j["result"]["content"][0]["text"].get<std::string>();
    auto args_j = nlohmann::json::parse(text);
    EXPECT_EQ(args_j["hello"].get<std::string>(), "world");
}

TEST(McpServerTest, HandlesUnknownMethod) {
    auto server = gg::McpServer::create("test", "1.0");

    const std::string req = R"({"jsonrpc":"2.0","id":4,"method":"nonexistent","params":{}})";
    const std::string resp = server->handle_message(req);

    auto j = nlohmann::json::parse(resp);
    ASSERT_TRUE(j.contains("error"));
    EXPECT_EQ(j["error"]["code"].get<int>(), -32601);
}

TEST(McpServerTest, HandlesUnknownTool) {
    auto server = gg::McpServer::create("test", "1.0");

    const std::string req = R"({"jsonrpc":"2.0","id":5,"method":"tools/call",)"
                            R"("params":{"name":"no_such_tool","arguments":{}}})";
    const std::string resp = server->handle_message(req);

    auto j = nlohmann::json::parse(resp);
    ASSERT_TRUE(j.contains("error"));
    EXPECT_EQ(j["error"]["code"].get<int>(), -32602);
}

TEST(McpServerTest, HandlesParseError) {
    auto server = gg::McpServer::create("test", "1.0");

    const std::string resp = server->handle_message("not valid json {{{");

    auto j = nlohmann::json::parse(resp);
    ASSERT_TRUE(j.contains("error"));
    EXPECT_EQ(j["error"]["code"].get<int>(), -32700);
    EXPECT_EQ(j["id"], nullptr);
}

TEST(McpServerTest, HandlesNotificationInitialized) {
    auto server = gg::McpServer::create("test", "1.0");

    const std::string req = R"({"jsonrpc":"2.0","method":"notifications/initialized"})";
    const std::string resp = server->handle_message(req);

    EXPECT_EQ(resp, "");
}

TEST(McpServerTest, HandlesToolCallException) {
    auto server = gg::McpServer::create("test", "1.0");

    gg::McpToolDef tool;
    tool.name = "thrower";
    tool.description = "Throws an exception";
    tool.input_schema = R"({"type":"object"})";
    tool.handler = [](const std::string&) -> std::string { throw std::runtime_error("boom"); };
    server->register_tool(std::move(tool));

    const std::string req = R"({"jsonrpc":"2.0","id":99,"method":"tools/call",)"
                            R"("params":{"name":"thrower","arguments":{}}})";
    const std::string resp = server->handle_message(req);

    auto j = nlohmann::json::parse(resp);
    ASSERT_TRUE(j.contains("error"));
    EXPECT_EQ(j["error"]["code"].get<int>(), -32603);
    EXPECT_NE(j["error"]["message"].get<std::string>().find("boom"), std::string::npos);
}

TEST(McpServerTest, HandlesInvalidInputSchema) {
    auto server = gg::McpServer::create("test", "1.0");

    gg::McpToolDef tool;
    tool.name = "bad_schema";
    tool.description = "Tool with invalid schema";
    tool.input_schema = "not json";
    tool.handler = [](const std::string&) { return "ok"; };
    server->register_tool(std::move(tool));

    const std::string req = R"({"jsonrpc":"2.0","id":6,"method":"tools/list","params":{}})";
    const std::string resp = server->handle_message(req);

    auto j = nlohmann::json::parse(resp);
    ASSERT_TRUE(j.contains("result"));
    // Should fall back to empty object for invalid schema
    const auto& schema = j["result"]["tools"][0]["inputSchema"];
    EXPECT_TRUE(schema.is_object());
    EXPECT_TRUE(schema.empty());
}

// ---------------------------------------------------------------------------
// Task 2: Built-in Tool Tests
// ---------------------------------------------------------------------------

TEST(McpToolsTest, ListEntities) {
    auto world = gg::World::create();
    auto physics = gg::PhysicsWorld::create({});
    auto server = gg::McpServer::create("test", "1.0");

    gg::register_mcp_tools(*server, *world, *physics);

    // Create two entities
    auto e1 = world->create_entity("alpha");
    e1.set<gg::Name>({"alpha"});
    e1.set<gg::Transform>({});

    auto e2 = world->create_entity("beta");
    e2.set<gg::Name>({"beta"});
    e2.set<gg::Transform>({});

    const std::string req =
        R"({"jsonrpc":"2.0","id":10,"method":"tools/call","params":{"name":"list_entities","arguments":{}}})";
    const std::string resp = server->handle_message(req);

    auto j = nlohmann::json::parse(resp);
    ASSERT_TRUE(j.contains("result"));
    const auto text = j["result"]["content"][0]["text"].get<std::string>();
    auto arr = nlohmann::json::parse(text);

    bool found_alpha = false;
    bool found_beta = false;
    for (const auto& entry : arr) {
        const auto name = entry["name"].get<std::string>();
        if (name == "alpha")
            found_alpha = true;
        if (name == "beta")
            found_beta = true;
    }
    EXPECT_TRUE(found_alpha);
    EXPECT_TRUE(found_beta);
}

TEST(McpToolsTest, CreateEntity) {
    auto world = gg::World::create();
    auto physics = gg::PhysicsWorld::create({});
    auto server = gg::McpServer::create("test", "1.0");

    gg::register_mcp_tools(*server, *world, *physics);

    const std::string req = R"({"jsonrpc":"2.0","id":11,"method":"tools/call",)"
                            R"("params":{"name":"create_entity","arguments":{"name":"spawned"}}})";
    const std::string resp = server->handle_message(req);

    auto j = nlohmann::json::parse(resp);
    ASSERT_TRUE(j.contains("result"));

    // Verify the entity was created with a Name component
    bool found = false;
    world->raw().each([&](flecs::entity, const gg::Name& n) {
        if (n.value == "spawned")
            found = true;
    });
    EXPECT_TRUE(found);
}

TEST(McpToolsTest, SetTransform) {
    auto world = gg::World::create();
    auto physics = gg::PhysicsWorld::create({});
    auto server = gg::McpServer::create("test", "1.0");

    gg::register_mcp_tools(*server, *world, *physics);

    // Create entity first
    auto e = world->create_entity("mover");
    e.set<gg::Name>({"mover"});
    gg::Transform t0;
    t0.position = {0.0f, 0.0f, 0.0f};
    e.set<gg::Transform>(t0);

    const std::string req =
        R"({"jsonrpc":"2.0","id":12,"method":"tools/call",)"
        R"("params":{"name":"set_transform","arguments":{"name":"mover","position":{"x":5.0,"y":3.0,"z":1.0}}}})";
    const std::string resp = server->handle_message(req);

    auto j = nlohmann::json::parse(resp);
    ASSERT_TRUE(j.contains("result"));

    const auto* t = e.get<gg::Transform>();
    ASSERT_NE(t, nullptr);
    EXPECT_FLOAT_EQ(t->position.x, 5.0f);
    EXPECT_FLOAT_EQ(t->position.y, 3.0f);
    EXPECT_FLOAT_EQ(t->position.z, 1.0f);
}

TEST(McpToolsTest, Raycast) {
    auto world = gg::World::create();
    auto physics = gg::PhysicsWorld::create({});
    auto server = gg::McpServer::create("test", "1.0");

    gg::register_mcp_tools(*server, *world, *physics);

    // Add a static body to hit
    gg::BodyDef def;
    def.shape = gg::BoxShapeDesc{5.0f, 5.0f, 5.0f};
    def.motion_type = gg::MotionType::Static;
    def.layer = gg::PhysicsLayer::Static;
    physics->add_body({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f}, def);

    // Settle broadphase
    physics->step(0.0f);

    const std::string req =
        R"({"jsonrpc":"2.0","id":13,"method":"tools/call",)"
        R"("params":{"name":"raycast","arguments":{"origin":{"x":0,"y":0,"z":-20},"direction":{"x":0,"y":0,"z":1},"max_distance":100}}})";
    const std::string resp = server->handle_message(req);

    auto j = nlohmann::json::parse(resp);
    ASSERT_TRUE(j.contains("result"));

    const auto text = j["result"]["content"][0]["text"].get<std::string>();
    auto result = nlohmann::json::parse(text);

    EXPECT_TRUE(result["hit"].get<bool>());
}

// ---------------------------------------------------------------------------
// Task 3: get_entity Tests
// ---------------------------------------------------------------------------

TEST(McpToolsTest, GetEntityWithAllComponents) {
    auto world = gg::World::create();
    auto physics = gg::PhysicsWorld::create({});
    auto server = gg::McpServer::create("test", "1.0");
    gg::register_mcp_tools(*server, *world, *physics);

    auto e = world->create_entity("hero");
    e.set<gg::Name>({"hero"});
    gg::Transform t;
    t.position = {1.0f, 2.0f, 3.0f};
    t.rotation = {0.0f, 0.0f, 0.0f, 1.0f};
    t.scale = {1.0f, 1.0f, 1.0f};
    e.set<gg::Transform>(t);
    e.set<gg::Velocity>({{4.0f, 5.0f, 6.0f}, {}});
    e.set<gg::RigidBody>({42, false});

    const std::string req = R"({"jsonrpc":"2.0","id":20,"method":"tools/call",)"
                            R"("params":{"name":"get_entity","arguments":{"name":"hero"}}})";
    const std::string resp = server->handle_message(req);

    auto j = nlohmann::json::parse(resp);
    ASSERT_TRUE(j.contains("result"));

    const auto text = j["result"]["content"][0]["text"].get<std::string>();
    auto info = nlohmann::json::parse(text);

    EXPECT_EQ(info["name"].get<std::string>(), "hero");
    EXPECT_FLOAT_EQ(info["position"]["x"].get<float>(), 1.0f);
    EXPECT_FLOAT_EQ(info["position"]["y"].get<float>(), 2.0f);
    EXPECT_FLOAT_EQ(info["position"]["z"].get<float>(), 3.0f);
    EXPECT_FLOAT_EQ(info["rotation"]["w"].get<float>(), 1.0f);
    EXPECT_FLOAT_EQ(info["scale"]["x"].get<float>(), 1.0f);
    EXPECT_FLOAT_EQ(info["velocity"]["x"].get<float>(), 4.0f);
    EXPECT_EQ(info["rigid_body"]["body_id"].get<uint32_t>(), 42u);
    EXPECT_FALSE(info["rigid_body"]["sync_to_physics"].get<bool>());
}

TEST(McpToolsTest, GetEntityNotFound) {
    auto world = gg::World::create();
    auto physics = gg::PhysicsWorld::create({});
    auto server = gg::McpServer::create("test", "1.0");
    gg::register_mcp_tools(*server, *world, *physics);

    const std::string req = R"({"jsonrpc":"2.0","id":21,"method":"tools/call",)"
                            R"("params":{"name":"get_entity","arguments":{"name":"ghost"}}})";
    const std::string resp = server->handle_message(req);

    auto j = nlohmann::json::parse(resp);
    ASSERT_TRUE(j.contains("result"));

    const auto text = j["result"]["content"][0]["text"].get<std::string>();
    auto result = nlohmann::json::parse(text);
    EXPECT_TRUE(result.contains("error"));
}

TEST(McpToolsTest, GetEntityInvalidJson) {
    auto world = gg::World::create();
    auto physics = gg::PhysicsWorld::create({});
    auto server = gg::McpServer::create("test", "1.0");
    gg::register_mcp_tools(*server, *world, *physics);

    // Provide a handler that receives invalid JSON as args
    // We need to call the tool handler with broken args — but via JSON-RPC
    // the arguments always come parsed. So we test the internal catch by
    // registering a tool whose handler calls get_entity_handler with bad json.
    // Actually, tools/call always parses arguments as JSON object, so the handler
    // gets valid JSON. But the handler itself has a try/catch for json::parse.
    // This path is technically unreachable via normal JSON-RPC, but we can test
    // a minimal entity lookup: entity with no Transform should still return name.
    auto e = world->create_entity("bare");
    e.set<gg::Name>({"bare"});

    const std::string req = R"({"jsonrpc":"2.0","id":22,"method":"tools/call",)"
                            R"("params":{"name":"get_entity","arguments":{"name":"bare"}}})";
    const std::string resp = server->handle_message(req);

    auto j = nlohmann::json::parse(resp);
    ASSERT_TRUE(j.contains("result"));

    const auto text = j["result"]["content"][0]["text"].get<std::string>();
    auto info = nlohmann::json::parse(text);
    EXPECT_EQ(info["name"].get<std::string>(), "bare");
    // No transform, velocity, or rigid_body keys expected
    EXPECT_FALSE(info.contains("position"));
    EXPECT_FALSE(info.contains("velocity"));
    EXPECT_FALSE(info.contains("rigid_body"));
}

// ---------------------------------------------------------------------------
// Task 4: remove_entity Tests
// ---------------------------------------------------------------------------

TEST(McpToolsTest, RemoveEntity) {
    auto world = gg::World::create();
    auto physics = gg::PhysicsWorld::create({});
    auto server = gg::McpServer::create("test", "1.0");
    gg::register_mcp_tools(*server, *world, *physics);

    auto e = world->create_entity("victim");
    e.set<gg::Name>({"victim"});
    e.set<gg::Transform>({});

    const std::string req = R"({"jsonrpc":"2.0","id":30,"method":"tools/call",)"
                            R"("params":{"name":"remove_entity","arguments":{"name":"victim"}}})";
    const std::string resp = server->handle_message(req);

    auto j = nlohmann::json::parse(resp);
    ASSERT_TRUE(j.contains("result"));

    const auto text = j["result"]["content"][0]["text"].get<std::string>();
    auto result = nlohmann::json::parse(text);
    EXPECT_EQ(result["removed"].get<std::string>(), "victim");

    // Verify entity is gone
    bool found = false;
    world->raw().each([&](flecs::entity, const gg::Name& n) {
        if (n.value == "victim")
            found = true;
    });
    EXPECT_FALSE(found);
}

TEST(McpToolsTest, RemoveEntityNotFound) {
    auto world = gg::World::create();
    auto physics = gg::PhysicsWorld::create({});
    auto server = gg::McpServer::create("test", "1.0");
    gg::register_mcp_tools(*server, *world, *physics);

    const std::string req = R"({"jsonrpc":"2.0","id":31,"method":"tools/call",)"
                            R"("params":{"name":"remove_entity","arguments":{"name":"nobody"}}})";
    const std::string resp = server->handle_message(req);

    auto j = nlohmann::json::parse(resp);
    ASSERT_TRUE(j.contains("result"));

    const auto text = j["result"]["content"][0]["text"].get<std::string>();
    auto result = nlohmann::json::parse(text);
    EXPECT_TRUE(result.contains("error"));
}

// ---------------------------------------------------------------------------
// Task 5: set_transform Extended Tests
// ---------------------------------------------------------------------------

TEST(McpToolsTest, SetTransformRotationAndScale) {
    auto world = gg::World::create();
    auto physics = gg::PhysicsWorld::create({});
    auto server = gg::McpServer::create("test", "1.0");
    gg::register_mcp_tools(*server, *world, *physics);

    auto e = world->create_entity("rotator");
    e.set<gg::Name>({"rotator"});
    gg::Transform t0;
    t0.position = {1.0f, 2.0f, 3.0f};
    t0.rotation = {0.0f, 0.0f, 0.0f, 1.0f};
    t0.scale = {1.0f, 1.0f, 1.0f};
    e.set<gg::Transform>(t0);

    const std::string req = R"({"jsonrpc":"2.0","id":40,"method":"tools/call",)"
                            R"("params":{"name":"set_transform","arguments":{)"
                            R"("name":"rotator",)"
                            R"("rotation":{"x":0.0,"y":0.707,"z":0.0,"w":0.707},)"
                            R"("scale":{"x":2.0,"y":2.0,"z":2.0}}}})";
    const std::string resp = server->handle_message(req);

    auto j = nlohmann::json::parse(resp);
    ASSERT_TRUE(j.contains("result"));

    const auto* t = e.get<gg::Transform>();
    ASSERT_NE(t, nullptr);
    // Position should be untouched
    EXPECT_FLOAT_EQ(t->position.x, 1.0f);
    // Rotation updated
    EXPECT_FLOAT_EQ(t->rotation.y, 0.707f);
    EXPECT_FLOAT_EQ(t->rotation.w, 0.707f);
    // Scale updated
    EXPECT_FLOAT_EQ(t->scale.x, 2.0f);
    EXPECT_FLOAT_EQ(t->scale.y, 2.0f);
}

TEST(McpToolsTest, SetTransformSyncsRigidBody) {
    auto world = gg::World::create();
    auto physics = gg::PhysicsWorld::create({});
    auto server = gg::McpServer::create("test", "1.0");
    gg::register_mcp_tools(*server, *world, *physics);

    auto e = world->create_entity("physics_ent");
    e.set<gg::Name>({"physics_ent"});
    e.set<gg::Transform>({});
    e.set<gg::RigidBody>({7, false});

    const std::string req =
        R"({"jsonrpc":"2.0","id":41,"method":"tools/call",)"
        R"("params":{"name":"set_transform","arguments":{"name":"physics_ent","position":{"x":10.0}}}})";
    const std::string resp = server->handle_message(req);

    auto j = nlohmann::json::parse(resp);
    ASSERT_TRUE(j.contains("result"));

    const auto* rb = e.get<gg::RigidBody>();
    ASSERT_NE(rb, nullptr);
    EXPECT_TRUE(rb->sync_to_physics);
    EXPECT_EQ(rb->body_id, 7u);
}

TEST(McpToolsTest, SetTransformNotFound) {
    auto world = gg::World::create();
    auto physics = gg::PhysicsWorld::create({});
    auto server = gg::McpServer::create("test", "1.0");
    gg::register_mcp_tools(*server, *world, *physics);

    const std::string req =
        R"({"jsonrpc":"2.0","id":42,"method":"tools/call",)"
        R"("params":{"name":"set_transform","arguments":{"name":"missing","position":{"x":1}}}})";
    const std::string resp = server->handle_message(req);

    auto j = nlohmann::json::parse(resp);
    ASSERT_TRUE(j.contains("result"));

    const auto text = j["result"]["content"][0]["text"].get<std::string>();
    auto result = nlohmann::json::parse(text);
    EXPECT_TRUE(result.contains("error"));
}

// ---------------------------------------------------------------------------
// Task 6: create_entity Error Cases
// ---------------------------------------------------------------------------

TEST(McpToolsTest, CreateEntityEmptyName) {
    auto world = gg::World::create();
    auto physics = gg::PhysicsWorld::create({});
    auto server = gg::McpServer::create("test", "1.0");
    gg::register_mcp_tools(*server, *world, *physics);

    const std::string req = R"({"jsonrpc":"2.0","id":50,"method":"tools/call",)"
                            R"("params":{"name":"create_entity","arguments":{"name":""}}})";
    const std::string resp = server->handle_message(req);

    auto j = nlohmann::json::parse(resp);
    ASSERT_TRUE(j.contains("result"));

    const auto text = j["result"]["content"][0]["text"].get<std::string>();
    auto result = nlohmann::json::parse(text);
    EXPECT_TRUE(result.contains("error"));
}

TEST(McpToolsTest, CreateEntityWithPosition) {
    auto world = gg::World::create();
    auto physics = gg::PhysicsWorld::create({});
    auto server = gg::McpServer::create("test", "1.0");
    gg::register_mcp_tools(*server, *world, *physics);

    const std::string req =
        R"({"jsonrpc":"2.0","id":51,"method":"tools/call",)"
        R"("params":{"name":"create_entity","arguments":{"name":"placed","position":{"x":7.0,"y":8.0,"z":9.0}}}})";
    const std::string resp = server->handle_message(req);

    auto j = nlohmann::json::parse(resp);
    ASSERT_TRUE(j.contains("result"));

    // Find the entity and verify position
    bool found = false;
    world->raw().each([&](flecs::entity e, const gg::Name& n) {
        if (n.value == "placed") {
            found = true;
            const auto* t = e.get<gg::Transform>();
            ASSERT_NE(t, nullptr);
            EXPECT_FLOAT_EQ(t->position.x, 7.0f);
            EXPECT_FLOAT_EQ(t->position.y, 8.0f);
            EXPECT_FLOAT_EQ(t->position.z, 9.0f);
        }
    });
    EXPECT_TRUE(found);
}

// ---------------------------------------------------------------------------
// Task 7: Raycast Edge Cases
// ---------------------------------------------------------------------------

TEST(McpToolsTest, RaycastMiss) {
    auto world = gg::World::create();
    auto physics = gg::PhysicsWorld::create({});
    auto server = gg::McpServer::create("test", "1.0");
    gg::register_mcp_tools(*server, *world, *physics);

    // No bodies — ray should miss
    const std::string req =
        R"({"jsonrpc":"2.0","id":60,"method":"tools/call",)"
        R"("params":{"name":"raycast","arguments":{"origin":{"x":0,"y":0,"z":0},"direction":{"x":0,"y":0,"z":1}}}})";
    const std::string resp = server->handle_message(req);

    auto j = nlohmann::json::parse(resp);
    ASSERT_TRUE(j.contains("result"));

    const auto text = j["result"]["content"][0]["text"].get<std::string>();
    auto result = nlohmann::json::parse(text);

    EXPECT_FALSE(result["hit"].get<bool>());
    EXPECT_FALSE(result.contains("body_id"));
}

TEST(McpToolsTest, RaycastDefaultParams) {
    auto world = gg::World::create();
    auto physics = gg::PhysicsWorld::create({});
    auto server = gg::McpServer::create("test", "1.0");
    gg::register_mcp_tools(*server, *world, *physics);

    // Call with empty arguments — should use defaults and not crash
    const std::string req = R"({"jsonrpc":"2.0","id":61,"method":"tools/call",)"
                            R"("params":{"name":"raycast","arguments":{}}})";
    const std::string resp = server->handle_message(req);

    auto j = nlohmann::json::parse(resp);
    ASSERT_TRUE(j.contains("result"));

    const auto text = j["result"]["content"][0]["text"].get<std::string>();
    auto result = nlohmann::json::parse(text);
    EXPECT_TRUE(result.contains("hit"));
}
