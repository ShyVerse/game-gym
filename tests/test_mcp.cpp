#include <gtest/gtest.h>
#include "mcp/mcp_server.h"
#include "mcp/mcp_tools.h"
#include "ecs/world.h"
#include "ecs/components.h"
#include "physics/physics_world.h"
#include "physics/physics_components.h"
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
    tool.name         = "dummy_tool";
    tool.description  = "A test tool";
    tool.input_schema = R"({"type":"object"})";
    tool.handler      = [](const std::string&) { return R"({"ok":true})"; };
    server->register_tool(std::move(tool));

    const std::string req  = R"({"jsonrpc":"2.0","id":2,"method":"tools/list","params":{}})";
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
    tool.name         = "echo";
    tool.description  = "Echoes args back";
    tool.input_schema = R"({"type":"object"})";
    tool.handler      = [](const std::string& args) { return args; };
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

    const std::string req  = R"({"jsonrpc":"2.0","id":4,"method":"nonexistent","params":{}})";
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

// ---------------------------------------------------------------------------
// Task 2: Built-in Tool Tests
// ---------------------------------------------------------------------------

TEST(McpToolsTest, ListEntities) {
    auto world   = gg::World::create();
    auto physics = gg::PhysicsWorld::create({});
    auto server  = gg::McpServer::create("test", "1.0");

    gg::register_mcp_tools(*server, *world, *physics);

    // Create two entities
    auto e1 = world->create_entity("alpha");
    e1.set<gg::Name>({"alpha"});
    e1.set<gg::Transform>({});

    auto e2 = world->create_entity("beta");
    e2.set<gg::Name>({"beta"});
    e2.set<gg::Transform>({});

    const std::string req  = R"({"jsonrpc":"2.0","id":10,"method":"tools/call","params":{"name":"list_entities","arguments":{}}})";
    const std::string resp = server->handle_message(req);

    auto j = nlohmann::json::parse(resp);
    ASSERT_TRUE(j.contains("result"));
    const auto text = j["result"]["content"][0]["text"].get<std::string>();
    auto arr = nlohmann::json::parse(text);

    bool found_alpha = false;
    bool found_beta  = false;
    for (const auto& entry : arr) {
        const auto name = entry["name"].get<std::string>();
        if (name == "alpha") found_alpha = true;
        if (name == "beta")  found_beta  = true;
    }
    EXPECT_TRUE(found_alpha);
    EXPECT_TRUE(found_beta);
}

TEST(McpToolsTest, CreateEntity) {
    auto world   = gg::World::create();
    auto physics = gg::PhysicsWorld::create({});
    auto server  = gg::McpServer::create("test", "1.0");

    gg::register_mcp_tools(*server, *world, *physics);

    const std::string req = R"({"jsonrpc":"2.0","id":11,"method":"tools/call",)"
                            R"("params":{"name":"create_entity","arguments":{"name":"spawned"}}})";
    const std::string resp = server->handle_message(req);

    auto j = nlohmann::json::parse(resp);
    ASSERT_TRUE(j.contains("result"));

    // Verify the entity was created with a Name component
    bool found = false;
    world->raw().each([&](flecs::entity, const gg::Name& n) {
        if (n.value == "spawned") found = true;
    });
    EXPECT_TRUE(found);
}

TEST(McpToolsTest, SetTransform) {
    auto world   = gg::World::create();
    auto physics = gg::PhysicsWorld::create({});
    auto server  = gg::McpServer::create("test", "1.0");

    gg::register_mcp_tools(*server, *world, *physics);

    // Create entity first
    auto e = world->create_entity("mover");
    e.set<gg::Name>({"mover"});
    gg::Transform t0;
    t0.position = {0.0f, 0.0f, 0.0f};
    e.set<gg::Transform>(t0);

    const std::string req = R"({"jsonrpc":"2.0","id":12,"method":"tools/call",)"
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
    auto world   = gg::World::create();
    auto physics = gg::PhysicsWorld::create({});
    auto server  = gg::McpServer::create("test", "1.0");

    gg::register_mcp_tools(*server, *world, *physics);

    // Add a static body to hit
    gg::BodyDef def;
    def.shape       = gg::BoxShapeDesc{5.0f, 5.0f, 5.0f};
    def.motion_type = gg::MotionType::Static;
    def.layer       = gg::PhysicsLayer::Static;
    physics->add_body({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f}, def);

    // Settle broadphase
    physics->step(0.0f);

    const std::string req = R"({"jsonrpc":"2.0","id":13,"method":"tools/call",)"
                            R"("params":{"name":"raycast","arguments":{"origin":{"x":0,"y":0,"z":-20},"direction":{"x":0,"y":0,"z":1},"max_distance":100}}})";
    const std::string resp = server->handle_message(req);

    auto j = nlohmann::json::parse(resp);
    ASSERT_TRUE(j.contains("result"));

    const auto text = j["result"]["content"][0]["text"].get<std::string>();
    auto result = nlohmann::json::parse(text);

    EXPECT_TRUE(result["hit"].get<bool>());
}
