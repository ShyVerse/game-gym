#pragma once
#include "ecs/components.h"
#include "math/mat4.h"

#include <memory>
#include <webgpu/webgpu.h>

namespace gg {
class Camera;
class GpuContext;

class GizmoRenderer {
public:
    static std::unique_ptr<GizmoRenderer> create(GpuContext& ctx);
    ~GizmoRenderer();
    GizmoRenderer(const GizmoRenderer&) = delete;
    GizmoRenderer& operator=(const GizmoRenderer&) = delete;
    GizmoRenderer(GizmoRenderer&&) = delete;
    GizmoRenderer& operator=(GizmoRenderer&&) = delete;

    void draw(const Vec3& position, const Camera& camera, WGPURenderPassEncoder pass,
              float scale = 1.0f, int hovered_axis = -1, int dragging_axis = -1);

private:
    explicit GizmoRenderer(GpuContext& ctx);
    GpuContext& ctx_;
    WGPURenderPipeline pipeline_ = nullptr;
    WGPUBuffer vertex_buffer_ = nullptr;
    WGPUBuffer uniform_buffer_ = nullptr;
    WGPUBindGroup bind_group_ = nullptr;
};
} // namespace gg
