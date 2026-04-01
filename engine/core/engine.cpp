#include "core/engine.h"

#include "core/window.h"
#include "ecs/world.h"
#include "editor/editor_ui.h"
#include "mcp/mcp_server.h"
#include "mcp/mcp_stdio_transport.h"
#include "mcp/mcp_tools.h"
#include "physics/physics_world.h"
#include "renderer/camera.h"
#include "renderer/gltf_loader.h"
#include "renderer/gpu_context.h"
#include "renderer/mesh.h"
#include "renderer/mesh_renderer.h"
#include "renderer/renderer.h"

#ifdef GG_ENABLE_SCRIPTS
#include "script/script_bindings.h"
#include "script/script_engine.h"
#include "script/script_manager.h"
#endif

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
        .title = config.title,
        .width = config.width,
        .height = config.height,
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

    engine->editor_ = EditorUI::create(engine->window_->native_handle(), *engine->gpu_);
    if (!engine->editor_) {
        throw std::runtime_error("Failed to create EditorUI");
    }

    if (!config.model_path.empty()) {
        engine->meshes_ = GltfLoader::load(config.model_path, *engine->gpu_);
        if (!engine->meshes_.empty()) {
            engine->mesh_renderer_ = MeshRenderer::create(*engine->gpu_);
            engine->camera_ = Camera::create();
            engine->camera_->set_aspect(float(config.width) / float(config.height));
        }
    }

    if (config.enable_mcp) {
        engine->mcp_ = McpServer::create("game-gym-engine", "1.0.0");
        register_mcp_tools(*engine->mcp_, *engine->world_, *engine->physics_);
        engine->mcp_transport_ = McpStdioTransport::create();
        engine->mcp_transport_->start();
    }

#ifdef GG_ENABLE_SCRIPTS
    if (config.enable_scripts) {
        engine->script_engine_ = ScriptEngine::create();
        if (!engine->script_engine_) {
            throw std::runtime_error("Failed to create ScriptEngine (V8)");
        }
        register_script_bindings(*engine->script_engine_, *engine->world_, *engine->physics_);
        engine->script_manager_ = ScriptManager::create(*engine->script_engine_, config.script_dir);
        if (engine->script_manager_) {
            engine->script_manager_->load_all();
        }
    }
#endif

    return engine;
}

void Engine::run() {
    constexpr float FIXED_DT = 1.0f / 60.0f;

    while (!window_->should_close()) {
        window_->poll_events();

        // MCP: poll and handle incoming requests
        if (mcp_ && mcp_transport_) {
            std::string request = mcp_transport_->poll_request();
            while (!request.empty()) {
                const std::string response = mcp_->handle_message(request);
                if (!response.empty()) {
                    mcp_transport_->send_response(response);
                }
                request = mcp_transport_->poll_request();
            }
        }

#ifdef GG_ENABLE_SCRIPTS
        if (script_manager_) {
            script_manager_->poll_changes();
            script_manager_->call_update(FIXED_DT);
        }
#endif

        // Physics step with bidirectional ECS sync
        physics_->step_with_ecs(FIXED_DT, world_->raw());

        // ECS progress (VelocitySystem for non-physics entities, other systems)
        world_->progress(FIXED_DT);

        if (camera_) {
            static float last_mx = window_->mouse_x();
            static float last_my = window_->mouse_y();
            float mx = window_->mouse_x();
            float my = window_->mouse_y();
            if (window_->mouse_button(0)) {
                camera_->orbit(mx - last_mx, my - last_my);
            }
            camera_->zoom(window_->scroll_delta_y());
            window_->reset_scroll();
            last_mx = mx;
            last_my = my;
        }

        // Editor: begin new ImGui frame and build panel draw calls
        editor_->begin_frame();
        editor_->draw_panels(*world_, *physics_);

        // Handle window resize: update surface and depth texture dimensions.
        {
            uint32_t fb_w = window_->framebuffer_width();
            uint32_t fb_h = window_->framebuffer_height();
            if (fb_w != gpu_->surface_width() || fb_h != gpu_->surface_height()) {
                gpu_->resize(fb_w, fb_h);
                if (mesh_renderer_) {
                    mesh_renderer_->resize_depth(fb_w, fb_h);
                }
                if (camera_) {
                    camera_->set_aspect(static_cast<float>(fb_w) /
                                        static_cast<float>(fb_h > 0 ? fb_h : 1));
                }
            }
        }

        if (mesh_renderer_) {
            renderer_->set_depth_view(mesh_renderer_->depth_view());
        }

        if (renderer_->begin_frame()) {
            if (mesh_renderer_ && camera_ && !meshes_.empty()) {
                mesh_renderer_->update_camera(*camera_);
                for (const auto& mesh : meshes_) {
                    mesh_renderer_->draw(*mesh, renderer_->render_pass());
                }
            } else {
                renderer_->draw_triangle();
            }
            // Render ImGui draw data into the active render pass
            editor_->render(renderer_->render_pass());
            renderer_->end_frame();
        }

#ifdef GG_ENABLE_SCRIPTS
        if (script_engine_) {
            script_engine_->idle_gc(0.002);
        }
#endif
    }
}

World& Engine::world() {
    return *world_;
}
PhysicsWorld& Engine::physics() {
    return *physics_;
}
GpuContext& Engine::gpu() {
    return *gpu_;
}

} // namespace gg
