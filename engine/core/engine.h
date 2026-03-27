#pragma once
#include <memory>
#include <string>

namespace gg {

class Window;
class GpuContext;
class Renderer;
class World;

struct EngineConfig {
    std::string title       = "Game-Gym Engine";
    uint32_t    width       = 1280;
    uint32_t    height      = 720;
    bool        resizable   = true;
    std::string shader_path = "shaders/triangle.wgsl";
};

class Engine {
public:
    /// Factory: creates Window, GpuContext, Renderer, and ECS World.
    static std::unique_ptr<Engine> create(const EngineConfig& config);

    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    Engine(Engine&&) = delete;
    Engine& operator=(Engine&&) = delete;

    /// Run the main loop until the window is closed.
    void run();

private:
    Engine() = default;

    static std::string read_file(const std::string& path);

    std::unique_ptr<Window>     window_;
    std::unique_ptr<GpuContext> gpu_;
    std::unique_ptr<Renderer>   renderer_;
    std::unique_ptr<World>      world_;
};

} // namespace gg
