#pragma once
#include "math/mat4.h"

#include <cstdint>
#include <memory>
#include <webgpu/webgpu.h>

namespace gg {
class Camera;
class GpuContext;
class Mesh;

class MeshRenderer {
public:
    static std::unique_ptr<MeshRenderer> create(GpuContext& ctx);
    ~MeshRenderer();
    MeshRenderer(const MeshRenderer&) = delete;
    MeshRenderer& operator=(const MeshRenderer&) = delete;
    MeshRenderer(MeshRenderer&&) = delete;
    MeshRenderer& operator=(MeshRenderer&&) = delete;

    void update_camera(const Camera& camera);
    void draw(const Mesh& mesh, const Mat4& model_matrix, WGPURenderPassEncoder pass);
    [[nodiscard]] WGPUTextureView depth_view() const { return depth_view_; }
    void resize_depth(uint32_t width, uint32_t height);

private:
    explicit MeshRenderer(GpuContext& ctx);
    GpuContext& ctx_;
    WGPURenderPipeline pipeline_ = nullptr;
    WGPUBuffer camera_buffer_ = nullptr;
    WGPUBindGroup bind_group_ = nullptr;
    WGPUTexture depth_texture_ = nullptr;
    WGPUTextureView depth_view_ = nullptr;
    Mat4 view_projection_ = Mat4::identity();
};
} // namespace gg
