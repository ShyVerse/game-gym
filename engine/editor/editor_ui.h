#pragma once
#include <cstddef>
#include <memory>
#include <string>
#include <webgpu/webgpu.h>

struct GLFWwindow;

namespace gg {
class GpuContext;
class World;
class PhysicsWorld;

struct EditorSessionInfo {
    std::string project_path;
    std::string scene_path;
    size_t mesh_asset_count = 0;
    size_t script_count = 0;
    std::string status_text;
};

class EditorUI {
public:
    static std::unique_ptr<EditorUI>
    create(GLFWwindow* window,
           GpuContext& gpu,
           WGPUTextureFormat depth_format = WGPUTextureFormat_Undefined);
    ~EditorUI();
    EditorUI(const EditorUI&) = delete;
    EditorUI& operator=(const EditorUI&) = delete;

    void begin_frame();
    void draw_panels(World& world, PhysicsWorld& physics, const EditorSessionInfo& session);
    void render(WGPURenderPassEncoder pass);
    void set_visible(bool visible);
    [[nodiscard]] bool is_visible() const;

private:
    EditorUI() = default;
    bool visible_ = true;
    bool first_frame_ = true;
};
} // namespace gg
