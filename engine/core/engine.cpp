#include "core/engine.h"
#include "core/window.h"
#include "renderer/gpu_context.h"
#include "renderer/renderer.h"
#include "ecs/world.h"
#include "physics/physics_world.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace gg {

Engine::~Engine() = default;

std::string Engine::read_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + path);
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

std::unique_ptr<Engine> Engine::create(const EngineConfig& config) {
    auto engine = std::unique_ptr<Engine>(new Engine());

    engine->window_ = Window::create({
        .title     = config.title,
        .width     = config.width,
        .height    = config.height,
        .resizable = config.resizable,
    });
    if (!engine->window_) {
        throw std::runtime_error("Failed to create window");
    }

    engine->gpu_ = GpuContext::create(*engine->window_);
    if (!engine->gpu_) {
        throw std::runtime_error("Failed to create GPU context");
    }

    const std::string shader_source = read_file(config.shader_path);
    engine->renderer_ = Renderer::create(*engine->gpu_, shader_source);
    if (!engine->renderer_) {
        throw std::runtime_error("Failed to create renderer");
    }

    engine->world_ = World::create();
    if (!engine->world_) {
        throw std::runtime_error("Failed to create ECS world");
    }

    engine->physics_ = PhysicsWorld::create({});
    if (!engine->physics_) {
        throw std::runtime_error("Failed to create physics world");
    }

    return engine;
}

void Engine::run() {
    constexpr float FIXED_DT = 1.0f / 60.0f;

    while (!window_->should_close()) {
        window_->poll_events();

        // Physics step with bidirectional ECS sync
        physics_->step_with_ecs(FIXED_DT, world_->raw());

        // ECS progress (VelocitySystem for non-physics entities, other systems)
        world_->progress(FIXED_DT);

        if (renderer_->begin_frame()) {
            renderer_->draw_triangle();
            renderer_->end_frame();
        }
    }
}

World& Engine::world() { return *world_; }
PhysicsWorld& Engine::physics() { return *physics_; }

} // namespace gg
