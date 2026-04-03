#include "editor/editor_ui.h"

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

    // --- Menu bar -----------------------------------------------------------
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Toggle Editor", "F1")) {
                visible_ = !visible_;
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

        // Transform
        if (Transform* tf = selected_entity.get_mut<Transform>()) {
            ImGui::SeparatorText("Transform");
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

        // Velocity
        if (Velocity* vel = selected_entity.get_mut<Velocity>()) {
            ImGui::SeparatorText("Velocity");
            ImGui::DragFloat3("Linear", &vel->linear.x, 0.01f);
            ImGui::DragFloat3("Angular", &vel->angular.x, 0.01f);
        }

        // RigidBody (read-only info)
        if (const RigidBody* rb = selected_entity.get<RigidBody>()) {
            ImGui::SeparatorText("RigidBody");
            ImGui::Text("body_id: %u", rb->body_id);
            ImGui::Text("sync_to_physics: %s", rb->sync_to_physics ? "true" : "false");
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

    int entity_count = 0;
    world.raw().each([&entity_count](flecs::entity, const Name&) { ++entity_count; });

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
