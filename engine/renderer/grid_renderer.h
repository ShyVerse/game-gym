#pragma once
#include "math/mat4.h"

#include <memory>
#include <webgpu/webgpu.h>

namespace gg {
class Camera;
class GpuContext;

class GridRenderer {
public:
    static std::unique_ptr<GridRenderer> create(GpuContext& ctx);
    ~GridRenderer();
    GridRenderer(const GridRenderer&) = delete;
    GridRenderer& operator=(const GridRenderer&) = delete;
    GridRenderer(GridRenderer&&) = delete;
    GridRenderer& operator=(GridRenderer&&) = delete;

    void update_camera(const Camera& camera);
    void draw(WGPURenderPassEncoder pass);

private:
    explicit GridRenderer(GpuContext& ctx);
    GpuContext& ctx_;
    WGPURenderPipeline pipeline_ = nullptr;
    WGPUBuffer uniform_buffer_ = nullptr;
    WGPUBindGroup bind_group_ = nullptr;
    Mat4 view_projection_ = Mat4::identity();
};
} // namespace gg
