#pragma once

#include <memory>
#include <cstdint>
#include <webgpu/webgpu.h>

namespace gg {

class Window;

class GpuContext {
public:
    static std::unique_ptr<GpuContext> create(const Window& window);
    ~GpuContext();

    GpuContext(const GpuContext&) = delete;
    GpuContext& operator=(const GpuContext&) = delete;
    GpuContext(GpuContext&&) = delete;
    GpuContext& operator=(GpuContext&&) = delete;

    [[nodiscard]] WGPUDevice  device()         const { return device_; }
    [[nodiscard]] WGPUQueue   queue()          const { return queue_; }
    [[nodiscard]] WGPUSurface surface()        const { return surface_; }
    [[nodiscard]] WGPUTextureFormat surface_format() const { return surface_format_; }
    [[nodiscard]] uint32_t    surface_width()  const { return surface_width_; }
    [[nodiscard]] uint32_t    surface_height() const { return surface_height_; }

    void resize(uint32_t w, uint32_t h);

private:
    GpuContext() = default;

    void configure_surface();

    WGPUInstance    instance_      = nullptr;
    WGPUSurface     surface_       = nullptr;
    WGPUAdapter     adapter_       = nullptr;
    WGPUDevice      device_        = nullptr;
    WGPUQueue       queue_         = nullptr;
    WGPUTextureFormat surface_format_ = WGPUTextureFormat_Undefined;
    uint32_t        surface_width_  = 0;
    uint32_t        surface_height_ = 0;
};

} // namespace gg
