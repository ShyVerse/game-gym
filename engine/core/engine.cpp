#include "core/engine.h"

#include "assets/asset_paths.h"
#include "core/window.h"
#include "ecs/world.h"
#include "editor/editor_ui.h"
#include "editor/gizmo_interaction.h"
#include "math/ray.h"
#include "math/vec3.h"
#include "mcp/mcp_server.h"
#include "mcp/mcp_stdio_transport.h"
#include "mcp/mcp_tools.h"
#include "physics/physics_world.h"
#include "project/project_config.h"
#include "renderer/camera.h"
#include "renderer/gizmo_renderer.h"
#include "renderer/gltf_loader.h"
#include "renderer/gpu_context.h"
#include "renderer/grid_renderer.h"
#include "renderer/mesh.h"
#include "renderer/mesh_renderer.h"
#include "renderer/renderer.h"
#include "scene/scene_loader.h"
#include "version/version_info.h"

#ifdef GG_ENABLE_SCRIPTS
#include "script/script_bindings.h"
#include "script/script_engine.h"
#include "script/script_manager.h"
#endif

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
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

    if (has_project_config && !config.startup_scene_override.empty() &&
        !config.project_file.empty()) {
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

        auto scene_summary = load_scene_into_world(
            project_config.startup_scene.string(), project_config.project_root, *engine->world_);
        if (!scene_summary.ok) {
            throw std::runtime_error(scene_summary.error);
        }

        engine->loaded_mesh_asset_count_ = scene_summary.mesh_assets.size();
        engine->loaded_script_count_ = scene_summary.script_assets.size();
        startup_script_paths = std::move(scene_summary.script_assets);

        if (!scene_summary.mesh_assets.empty()) {
            for (const auto& mesh_asset_path : scene_summary.mesh_assets) {
                auto meshes = GltfLoader::load(mesh_asset_path, *engine->gpu_);
                if (!meshes.empty()) {
                    engine->mesh_assets_.emplace(mesh_asset_path, std::move(meshes));
                }
            }

            if (!engine->mesh_assets_.empty()) {
                engine->mesh_renderer_ = MeshRenderer::create(*engine->gpu_);
                engine->camera_ = Camera::create();
                engine->camera_->set_aspect(static_cast<float>(config.width) / static_cast<float>(config.height));
                engine->grid_renderer_ = GridRenderer::create(*engine->gpu_);
                engine->gizmo_renderer_ = GizmoRenderer::create(*engine->gpu_);
                engine->gizmo_interaction_ = GizmoInteraction::create();
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
            engine->camera_->set_aspect(static_cast<float>(config.width) / static_cast<float>(config.height));
            engine->grid_renderer_ = GridRenderer::create(*engine->gpu_);
            engine->gizmo_renderer_ = GizmoRenderer::create(*engine->gpu_);
            engine->gizmo_interaction_ = GizmoInteraction::create();
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
        engine->mcp_ =
            McpServer::create("game-gym-engine", std::string(build_version::display_version()));
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
            } else if (!has_project_config) {
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

        // Gizmo interaction + camera orbit
        static constexpr float GIZMO_SCREEN_RATIO = 0.12f;

        float mx = window_->mouse_x();
        float my = window_->mouse_y();
        bool mouse_down = window_->mouse_button(0);
        static float last_mx = 0.0f;
        static float last_my = 0.0f;
        static flecs::entity gizmo_target_entity;

        if (gizmo_interaction_ && camera_ && gizmo_renderer_) {
            Vec3 eye = camera_->eye_position();
            Mat4 vp = camera_->view_projection_matrix();
            Mat4 inv_vp = vp.inverse();
            Ray ray = ray_from_screen(
                mx, my, window_->framebuffer_width(), window_->framebuffer_height(), inv_vp);

            // Find best gizmo+axis by ray hit (not camera distance)
            struct GizmoTarget {
                flecs::entity entity;
                Vec3 position;
                float ray_dist;
            };
            GizmoTarget best_target{};
            best_target.ray_dist = 1e30f;
            bool found_target = false;

            if (!mesh_assets_.empty()) {
                world_->raw().each(
                    [&](flecs::entity e, const Transform& transform, const Renderable& /*r*/) {
                        float cam_dist = vec3_length(vec3_sub(eye, transform.position));
                        float scale =
                            cam_dist * std::tan(camera_->fov() / 2.0f) * GIZMO_SCREEN_RATIO;
                        float threshold = scale * 0.15f;
                        float arrow_len = 1.5f * scale;

                        for (int axis = 0; axis < 3; ++axis) {
                            Vec3 axis_dir = (axis == 0)   ? Vec3{1, 0, 0}
                                            : (axis == 1) ? Vec3{0, 1, 0}
                                                          : Vec3{0, 0, 1};
                            float t = 0.0f;
                            float dist = ray_axis_distance(ray, transform.position, axis_dir, t);
                            if (t >= 0.0f && t <= arrow_len && dist < threshold &&
                                dist < best_target.ray_dist) {
                                best_target.entity = e;
                                best_target.position = transform.position;
                                best_target.ray_dist = dist;
                                found_target = true;
                            }
                        }
                    });
            }

            if (found_target) {
                float cam_dist = vec3_length(vec3_sub(eye, best_target.position));
                float gizmo_scale = cam_dist * std::tan(camera_->fov() / 2.0f) * GIZMO_SCREEN_RATIO;
                gizmo_interaction_->update(mx,
                                           my,
                                           mouse_down,
                                           window_->framebuffer_width(),
                                           window_->framebuffer_height(),
                                           *camera_,
                                           best_target.position,
                                           gizmo_scale);
                gizmo_target_entity = best_target.entity;

                Vec3 delta = gizmo_interaction_->position_delta();
                if (gizmo_interaction_->state().dragging_axis >= 0) {
                    auto* t = best_target.entity.get_mut<Transform>();
                    if (t != nullptr) {
                        t->position = vec3_add(t->position, delta);

                        const auto* rb = best_target.entity.get<RigidBody>();
                        if (rb != nullptr) {
                            best_target.entity.set<RigidBody>({
                                .body_id = rb->body_id,
                                .sync_to_physics = true,
                            });
                        }
                    }
                }
            } else if (gizmo_interaction_->state().dragging_axis < 0) {
                // No target hovered and not dragging — clear state
                gizmo_interaction_->update(mx,
                                           my,
                                           false,
                                           window_->framebuffer_width(),
                                           window_->framebuffer_height(),
                                           *camera_,
                                           {},
                                           1.0f);
                gizmo_target_entity = flecs::entity{};
            }
        }

        // Camera orbit (skip while dragging gizmo)
        bool is_dragging_gizmo =
            gizmo_interaction_ && gizmo_interaction_->state().dragging_axis >= 0;
        if (camera_ && !is_dragging_gizmo) {
            if (mouse_down) {
                camera_->orbit(mx - last_mx, my - last_my);
            }
            camera_->zoom(window_->scroll_delta_y());
            window_->reset_scroll();
        } else if (camera_) {
            camera_->zoom(window_->scroll_delta_y());
            window_->reset_scroll();
        }
        // Always update last mouse position (prevents jump after drag ends)
        last_mx = mx;
        last_my = my;

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
            if (mesh_renderer_ && camera_ &&
                !mesh_assets_.empty()) { // NOLINT(bugprone-branch-clone)
                mesh_renderer_->update_camera(*camera_);
                world_->raw().each([&](flecs::entity /*entity*/,
                                       const Transform& transform,
                                       const Renderable& renderable) {
                    auto it = mesh_assets_.find(renderable.mesh_asset_path);
                    if (it == mesh_assets_.end()) {
                        return;
                    }

                    const Mat4 model_matrix = Mat4::from_transform(transform);
                    for (const auto& mesh : it->second) {
                        mesh_renderer_->draw(*mesh, model_matrix, renderer_->render_pass());
                    }
                });
            } else if (mesh_renderer_ && camera_ && !meshes_.empty()) {
                mesh_renderer_->update_camera(*camera_);
                for (const auto& mesh : meshes_) {
                    mesh_renderer_->draw(*mesh, Mat4::identity(), renderer_->render_pass());
                }
            } else {
                renderer_->draw_triangle();
            }
            // Grid overlay
            if (grid_renderer_ && camera_) {
                grid_renderer_->update_camera(*camera_);
                grid_renderer_->draw(renderer_->render_pass());
            }

            // Gizmo on renderable entities
            if (gizmo_renderer_ && camera_ && !mesh_assets_.empty()) {
                Vec3 eye = camera_->eye_position();
                int hovered = gizmo_interaction_ ? gizmo_interaction_->state().hovered_axis : -1;
                int dragging = gizmo_interaction_ ? gizmo_interaction_->state().dragging_axis : -1;

                world_->raw().each(
                    [&](flecs::entity e, const Transform& transform, const Renderable& /*r*/) {
                        float dist = vec3_length(vec3_sub(eye, transform.position));
                        float s = dist * std::tan(camera_->fov() / 2.0f) * GIZMO_SCREEN_RATIO;
                        // Only highlight the entity being interacted with
                        bool is_target = gizmo_target_entity.is_valid() && e == gizmo_target_entity;
                        int eh = is_target ? hovered : -1;
                        int ed = is_target ? dragging : -1;
                        gizmo_renderer_->draw(
                            transform.position, *camera_, renderer_->render_pass(), s, eh, ed);
                    });
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
