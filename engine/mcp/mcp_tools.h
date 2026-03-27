#pragma once
namespace gg {
class McpServer;
class World;
class PhysicsWorld;
void register_mcp_tools(McpServer& server, World& world, PhysicsWorld& physics);
} // namespace gg
