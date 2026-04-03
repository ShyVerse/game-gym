#include "core/engine.h"

#include "assets/asset_paths.h"
#include "core/window.h"
#include "ecs/world.h"
#include "editor/editor_ui.h"
#include "mcp/mcp_server.h"
#include "mcp/mcp_stdio_transport.h"
#include "mcp/mcp_tools.h"
#include "physics/physics_world.h"
#include "project/project_config.h"
#include "renderer/camera.h"
#include "renderer/gltf_loader.h"
#include "renderer/gpu_context.h"
#include "renderer/mesh.h"
#include "renderer/mesh_renderer.h"
#include "renderer/renderer.h"
#include "scene/scene_loader.h"

#ifdef GG_ENABLE_SCRIPTS
#include "script/script_bindings.h"
#include "script/script_engine.h"
#include "script/script_manager.h"
#endif

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>

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
    std::vector<std::string> startup_script_paths;
    ProjectConfig project_config;
    bool has_project_config = false;

    if (config.enable_project_boot) {
        if (!config.project_file.empty()) {
            auto project_result = load_project_config(config.project_file);
            if (!project_result.ok) {
                throw std::runtime_error(project_result.error);
            }
            project_config = std::move(project_result.config);
            has_project_config = true;
        } else if (!config.startup_scene_override.empty()) {
            project_config.name = "game-gym";
            project_config.project_root = std::filesystem::current_path();
            project_config.assets_dir = project_config.project_root / "assets";
            project_config.scripts_dir = project_config.project_root / config.script_dir;

            auto startup_scene =
                resolve_project_path(project_config.project_root, config.startup_scene_override);
            if (!startup_scene.ok) {
                throw std::runtime_error(startup_scene.error);
            }
            project_config.startup_scene = startup_scene.path;
            has_project_config = true;
        }
    }

    if (has_project_config && !config.startup_scene_override.empty() && !config.project_file.empty()) {
        auto startup_scene =
            resolve_project_path(project_config.project_root, config.startup_scene_override);
        if (!startup_scene.ok) {
            throw std::runtime_error(startup_scene.error);
        }
        project_config.startup_scene = startup_scene.path;
    }

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

    if (has_project_config) {
        engine->active_project_path_ = project_config.project_file.empty()
                                           ? project_config.project_root.string()
                                           : project_config.project_file.string();
        engine->active_scene_path_ = project_config.startup_scene.string();

        auto scene_summary = load_scene_into_world(project_config.startup_scene.string(),
                                                   project_config.project_root,
                                                   *engine->world_);
        if (!scene_summary.ok) {
            throw std::runtime_error(scene_summary.error);
        }

        engine->loaded_mesh_asset_count_ = scene_summary.mesh_assets.size();
        engine->loaded_script_count_ = scene_summary.script_assets.size();
        startup_script_paths = std::move(scene_summary.script_assets);

        if (!scene_summary.mesh_assets.empty()) {
            engine->meshes_ = GltfLoader::load(scene_summary.mesh_assets.front(), *engine->gpu_);
            if (!engine->meshes_.empty()) {
                engine->mesh_renderer_ = MeshRenderer::create(*engine->gpu_);
                engine->camera_ = Camera::create();
                engine->camera_->set_aspect(float(config.width) / float(config.height));
                engine->boot_status_text_ = "Startup scene loaded from project file.";
            } else {
                engine->boot_status_text_ =
                    "Startup scene loaded, but the first mesh asset could not be parsed.";
            }
        } else {
            engine->boot_status_text_ = "Startup scene loaded. No mesh assets were attached.";
        }
    } else if (!config.model_path.empty()) {
        engine->meshes_ = GltfLoader::load(config.model_path, *engine->gpu_);
        if (!engine->meshes_.empty()) {
            engine->mesh_renderer_ = MeshRenderer::create(*engine->gpu_);
            engine->camera_ = Camera::create();
            engine->camera_->set_aspect(float(config.width) / float(config.height));
            engine->boot_status_text_ = "Direct model debug boot.";
        }
    }

    const auto editor_depth_format =
        engine->mesh_renderer_ ? WGPUTextureFormat_Depth24Plus : WGPUTextureFormat_Undefined;
    engine->editor_ =
        EditorUI::create(engine->window_->native_handle(), *engine->gpu_, editor_depth_format);
    if (!engine->editor_) {
        throw std::runtime_error("Failed to create EditorUI");
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

        const std::string script_dir =
            has_project_config ? project_config.scripts_dir.string() : config.script_dir;
        engine->script_manager_ = ScriptManager::create(*engine->script_engine_, script_dir);
        if (engine->script_manager_) {
            if (!startup_script_paths.empty()) {
                engine->script_manager_->load_paths(startup_script_paths);
            } else {
                engine->script_manager_->load_all();
            }
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

        physics_->step_with_ecs(FIXED_DT, world_->raw());
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

        editor_->begin_frame();
        editor_->draw_panels(*world_,
                             *physics_,
                             {
                                 .project_path = active_project_path_,
                                 .scene_path = active_scene_path_,
                                 .mesh_asset_count = loaded_mesh_asset_count_,
                                 .script_count = loaded_script_count_,
                                 .status_text = boot_status_text_,
                             });

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
