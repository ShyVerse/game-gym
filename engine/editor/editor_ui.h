#pragma once
#include <memory>
#include <webgpu/webgpu.h>

struct GLFWwindow;

namespace gg {
class GpuContext;
class World;
class PhysicsWorld;

class EditorUI {
public:
    static std::unique_ptr<EditorUI> create(GLFWwindow* window, GpuContext& gpu);
    ~EditorUI();
    EditorUI(const EditorUI&) = delete;
    EditorUI& operator=(const EditorUI&) = delete;

    void begin_frame();
    void draw_panels(World& world, PhysicsWorld& physics);
    void render(WGPURenderPassEncoder pass);
    void set_visible(bool visible);
    [[nodiscard]] bool is_visible() const;

private:
    EditorUI() = default;
    bool visible_ = true;
};
} // namespace gg
