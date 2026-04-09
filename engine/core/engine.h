#pragma once
#include <chrono>
#include <cstdint>
#include <flecs.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace gg {

class Window;
class GpuContext;
class Renderer;
class World;
class PhysicsWorld;
class EditorUI;
class McpServer;
class McpSseTransport;
class Camera;
class Mesh;
class MeshRenderer;
class GridRenderer;
class GizmoRenderer;
class GizmoInteraction;

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
    std::string project_file = "";
    std::string startup_scene_override = "";
    bool enable_project_boot = false;
    bool enable_mcp = false;
    uint16_t mcp_port = 9315;
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
    std::unique_ptr<McpSseTransport> mcp_transport_;

    std::unique_ptr<Camera> camera_;
    std::unique_ptr<MeshRenderer> mesh_renderer_;
    std::unique_ptr<GridRenderer> grid_renderer_;
    std::unique_ptr<GizmoRenderer> gizmo_renderer_;
    std::unique_ptr<GizmoInteraction> gizmo_interaction_;
    std::vector<std::unique_ptr<Mesh>> meshes_;
    std::unordered_map<std::string, std::vector<std::unique_ptr<Mesh>>> mesh_assets_;
    std::string active_project_path_;
    std::string active_scene_path_;
    std::string boot_status_text_;
    size_t loaded_mesh_asset_count_ = 0;
    size_t loaded_script_count_ = 0;
    bool was_right_down_ = false;
    bool has_last_mouse_position_ = false;
    float last_mouse_x_ = 0.0f;
    float last_mouse_y_ = 0.0f;
    bool has_last_frame_time_ = false;
    std::chrono::steady_clock::time_point last_frame_time_{};
    flecs::entity gizmo_target_entity_{};

#ifdef GG_ENABLE_SCRIPTS
    std::unique_ptr<ScriptEngine> script_engine_;
    std::unique_ptr<ScriptManager> script_manager_;
#endif
};

} // namespace gg
