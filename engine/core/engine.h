#pragma once
#include <memory>
#include <string>
#include <vector>

namespace gg {

class Window;
class GpuContext;
class Renderer;
class World;
class PhysicsWorld;
class EditorUI;
class McpServer;
class McpTransport;
class Camera;
class Mesh;
class MeshRenderer;

#ifdef GG_ENABLE_SCRIPTS
class ScriptEngine;
class ScriptManager;
#endif

struct EngineConfig {
    std::string title = "Game-Gym Engine";
    uint32_t width = 1280;
    uint32_t height = 720;
    bool resizable = true;
    std::string shader_path = "shaders/triangle.wgsl";
    std::string model_path = "";
    bool enable_mcp = false;
    bool enable_scripts = false;
    std::string script_dir = "assets/scripts";
};

class Engine {
public:
    static std::unique_ptr<Engine> create(const EngineConfig& config);
    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    Engine(Engine&&) = delete;
    Engine& operator=(Engine&&) = delete;

    void run();

    /// Access the ECS world.
    World& world();

    /// Access the physics world.
    PhysicsWorld& physics();

    /// Access the GPU context (for compute shaders, custom rendering).
    GpuContext& gpu();

private:
    Engine() = default;
    static std::string read_file(const std::string& path);

    std::unique_ptr<Window> window_;
    std::unique_ptr<GpuContext> gpu_;
    std::unique_ptr<Renderer> renderer_;
    std::unique_ptr<World> world_;
    std::unique_ptr<PhysicsWorld> physics_;
    std::unique_ptr<EditorUI> editor_;
    std::unique_ptr<McpServer> mcp_;
    std::unique_ptr<McpTransport> mcp_transport_;

    std::unique_ptr<Camera> camera_;
    std::unique_ptr<MeshRenderer> mesh_renderer_;
    std::vector<std::unique_ptr<Mesh>> meshes_;

#ifdef GG_ENABLE_SCRIPTS
    std::unique_ptr<ScriptEngine> script_engine_;
    std::unique_ptr<ScriptManager> script_manager_;
#endif
};

} // namespace gg
