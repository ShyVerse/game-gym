#pragma once

#include <webgpu/webgpu.h>
#include <cstdint>
#include <vector>

namespace gg {

/// RAII wrapper around WGPUBuffer.
/// Move-only (no copy).
class GpuBuffer {
public:
    /// Create a storage buffer (Storage | CopyDst | CopySrc).
    static GpuBuffer create_storage(WGPUDevice device, uint64_t size);

    /// Create a uniform buffer (Uniform | CopyDst).
    static GpuBuffer create_uniform(WGPUDevice device, uint64_t size);

    ~GpuBuffer();

    // Move semantics
    GpuBuffer(GpuBuffer&& other) noexcept;
    GpuBuffer& operator=(GpuBuffer&& other) noexcept;

    // No copy
    GpuBuffer(const GpuBuffer&) = delete;
    GpuBuffer& operator=(const GpuBuffer&) = delete;

    /// Upload data into the buffer via the queue (wgpuQueueWriteBuffer).
    void upload(WGPUQueue queue, const void* data, uint64_t size, uint64_t offset = 0);

    /// Read back the full buffer contents synchronously.
    /// Creates a temporary staging buffer, copies, maps, returns bytes.
    [[nodiscard]] std::vector<uint8_t> readback(WGPUDevice device, WGPUQueue queue) const;

    [[nodiscard]] WGPUBuffer handle() const { return buffer_; }
    [[nodiscard]] uint64_t   size()   const { return size_; }

private:
    explicit GpuBuffer(WGPUBuffer buffer, uint64_t size);

    WGPUBuffer buffer_ = nullptr;
    uint64_t   size_   = 0;
};

} // namespace gg
