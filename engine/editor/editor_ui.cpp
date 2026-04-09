#include "editor/editor_ui.h"

#include "ecs/component_registry.h"
#include "ecs/components.h"
#include "ecs/world.h"
#include "physics/physics_components.h"
#include "physics/physics_world.h"
#include "renderer/gpu_context.h"

#include <GLFW/glfw3.h>
#include <chrono>
#include <cstdio>
#include <flecs.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_wgpu.h>
#include <imgui_internal.h>

namespace gg {

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------

std::unique_ptr<EditorUI>
EditorUI::create(GLFWwindow* window, GpuContext& gpu, WGPUTextureFormat depth_format) {
    auto editor = std::unique_ptr<EditorUI>(new EditorUI());

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    // Platform backend: GLFW (for Other, since we own the render pass)
    ImGui_ImplGlfw_InitForOther(window, true);

    // Renderer backend: WebGPU / wgpu-native
    ImGui_ImplWGPU_InitInfo wgpu_info{};
    wgpu_info.Device = gpu.device();
    wgpu_info.NumFramesInFlight = 3;
    wgpu_info.RenderTargetFormat = gpu.surface_format();
    wgpu_info.DepthStencilFormat = depth_format;

    if (!ImGui_ImplWGPU_Init(&wgpu_info)) {
        std::fprintf(stderr, "[EditorUI] ImGui_ImplWGPU_Init failed\n");
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        return nullptr;
    }

    return editor;
}

// ---------------------------------------------------------------------------
// Destructor
// ---------------------------------------------------------------------------

EditorUI::~EditorUI() {
    ImGui_ImplWGPU_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

// ---------------------------------------------------------------------------
// Frame lifecycle
// ---------------------------------------------------------------------------

void EditorUI::begin_frame() {
    ImGui_ImplWGPU_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void EditorUI::draw_panels(World& world,
                           PhysicsWorld& /*physics*/,
                           const EditorSessionInfo& session) {
    if (!visible_) {
        return;
    }

    // --- DockSpace ----------------------------------------------------------
    constexpr ImGuiDockNodeFlags dock_flags = ImGuiDockNodeFlags_PassthruCentralNode;
    ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
    ImGui::DockSpaceOverViewport(dockspace_id, nullptr, dock_flags);

    // --- Default layout (first frame only) ----------------------------------
    if (first_frame_) {
        first_frame_ = false;

        ImGui::DockBuilderRemoveNode(dockspace_id);
        ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace | dock_flags);

        ImVec2 viewport_size = ImGui::GetMainViewport()->Size;
        ImGui::DockBuilderSetNodeSize(dockspace_id, viewport_size);

        // Split left: Hierarchy (18%)
        ImGuiID dock_left = 0;
        ImGuiID dock_center = 0;
        ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.18f, &dock_left, &dock_center);

        // Split right from center: Inspector (24% of remaining)
        ImGuiID dock_right = 0;
        ImGuiID dock_viewport = 0;
        ImGui::DockBuilderSplitNode(
            dock_center, ImGuiDir_Right, 0.24f, &dock_right, &dock_viewport);

        // Split bottom from viewport: Stats/Scene (25% of remaining)
        ImGuiID dock_bottom = 0;
        ImGuiID dock_main = 0;
        ImGui::DockBuilderSplitNode(dock_viewport, ImGuiDir_Down, 0.25f, &dock_bottom, &dock_main);

        // Dock windows by name
        ImGui::DockBuilderDockWindow("Hierarchy", dock_left);
        ImGui::DockBuilderDockWindow("Inspector", dock_right);
        ImGui::DockBuilderDockWindow("Stats", dock_bottom);
        ImGui::DockBuilderDockWindow("Scene", dock_bottom); // tabs with Stats

        ImGui::DockBuilderFinish(dockspace_id);
    }

    // --- Menu bar -----------------------------------------------------------
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Toggle Editor", "F1")) {
                visible_ = !visible_;
            }
            if (ImGui::MenuItem("Reset Layout")) {
                first_frame_ = true;
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    // --- Hierarchy panel ----------------------------------------------------
    static flecs::entity selected_entity{};

    ImGui::Begin("Hierarchy");
    world.raw().each([&](flecs::entity e, const Name& name) {
        bool is_selected = (selected_entity == e);
        ImGuiSelectableFlags flags = ImGuiSelectableFlags_None;
        if (ImGui::Selectable(name.value.c_str(), is_selected, flags)) {
            selected_entity = e;
        }
    });
    ImGui::End();

    // --- Inspector panel ----------------------------------------------------
    ImGui::Begin("Inspector");
    if (selected_entity.is_valid()) {
        ImGui::Text("Entity: %llu", static_cast<unsigned long long>(selected_entity.id()));

        const auto section_label = [](std::string_view stable_id) -> const char* {
            const ComponentMeta* meta = find_component_meta(stable_id);
            return meta != nullptr ? meta->display_name.data() : "";
        };

        // Transform
        if (Transform* tf = selected_entity.get_mut<Transform>()) {
            ImGui::SeparatorText(section_label("transform"));
            bool changed = false;
            changed |= ImGui::DragFloat3("Position", &tf->position.x, 0.01f);
            changed |= ImGui::DragFloat3("Scale", &tf->scale.x, 0.01f);

            if (changed) {
                // Notify physics to pick up the new transform
                if (RigidBody* rb = selected_entity.get_mut<RigidBody>()) {
                    rb->sync_to_physics = true;
                }
            }
        }

        if (const Renderable* renderable = selected_entity.get<Renderable>()) {
            ImGui::SeparatorText(section_label("mesh_renderer"));
            ImGui::TextWrapped("Asset: %s", renderable->mesh_asset_path.c_str());
        }

        if (const ScriptComponent* script = selected_entity.get<ScriptComponent>()) {
            ImGui::SeparatorText(section_label("script"));
            ImGui::TextWrapped("Asset: %s", script->script_asset_path.c_str());
        }

        // RigidBody (read-only info)
        if (const RigidBody* rb = selected_entity.get<RigidBody>()) {
            ImGui::SeparatorText(section_label("rigid_body"));
            ImGui::Text("body_id: %u", rb->body_id);
            ImGui::Text("sync_to_physics: %s", rb->sync_to_physics ? "true" : "false");
        }

        // Velocity
        if (Velocity* vel = selected_entity.get_mut<Velocity>()) {
            ImGui::SeparatorText(section_label("velocity"));
            ImGui::DragFloat3("Linear", &vel->linear.x, 0.01f);
            ImGui::DragFloat3("Angular", &vel->angular.x, 0.01f);
        }
    } else {
        ImGui::TextDisabled("No entity selected");
    }
    ImGui::End();

    // --- Stats panel --------------------------------------------------------
    ImGui::Begin("Stats");
    ImGuiIO& io = ImGui::GetIO();
    ImGui::Text("FPS:        %.1f", io.Framerate);
    ImGui::Text("Frame time: %.3f ms", 1000.0f / io.Framerate);
    ImGui::End();

    // --- Scene panel --------------------------------------------------------
    ImGui::Begin("Scene");
    if (!session.project_path.empty()) {
        ImGui::TextWrapped("Project: %s", session.project_path.c_str());
    } else {
        ImGui::TextDisabled("Project: none");
    }

    if (!session.scene_path.empty()) {
        ImGui::TextWrapped("Startup scene: %s", session.scene_path.c_str());
    } else {
        ImGui::TextDisabled("Startup scene: none");
    }

    int entity_count = static_cast<int>(world.raw().count<Name>());

    ImGui::Separator();
    ImGui::Text("Entities: %d", entity_count);
    ImGui::Text("Mesh assets: %zu", session.mesh_asset_count);
    ImGui::Text("Scripts: %zu", session.script_count);

    if (!session.status_text.empty()) {
        ImGui::Separator();
        ImGui::TextWrapped("%s", session.status_text.c_str());
    }
    ImGui::End();
}

void EditorUI::render(WGPURenderPassEncoder pass) {
    ImGui::Render();
    ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), pass);
}

// ---------------------------------------------------------------------------
// Visibility
// ---------------------------------------------------------------------------

void EditorUI::set_visible(bool visible) {
    visible_ = visible;
}
bool EditorUI::is_visible() const {
    return visible_;
}

} // namespace gg
