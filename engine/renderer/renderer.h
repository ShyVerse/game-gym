#pragma once

#include <memory>
#include <string>
#include <webgpu/webgpu.h>

namespace gg {

class GpuContext;

class Renderer {
public:
    static std::unique_ptr<Renderer> create(GpuContext& ctx,
                                            const std::string& shader_source);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;

    // Returns false if the surface texture is not ready (skip this frame).
    [[nodiscard]] bool begin_frame();
    void draw_triangle();
    void end_frame();

    [[nodiscard]] WGPURenderPassEncoder render_pass() const { return render_pass_; }

private:
    explicit Renderer(GpuContext& ctx);

    GpuContext&         ctx_;
    WGPURenderPipeline  pipeline_       = nullptr;

    // Per-frame state (valid between begin_frame and end_frame)
    WGPUTexture         frame_texture_  = nullptr;
    WGPUTextureView     frame_view_     = nullptr;
    WGPUCommandEncoder  encoder_        = nullptr;
    WGPURenderPassEncoder render_pass_  = nullptr;
};

} // namespace gg
