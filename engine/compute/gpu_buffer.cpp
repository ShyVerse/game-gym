#include "compute/gpu_buffer.h"

// wgpuDevicePoll is a wgpu-native extension defined in wgpu.h
#include <webgpu/wgpu.h>

#include <stdexcept>
#include <cstring>
#include <cstdio>

namespace gg {

// ---------------------------------------------------------------------------
// Private constructor
// ---------------------------------------------------------------------------

GpuBuffer::GpuBuffer(WGPUBuffer buffer, uint64_t size)
    : buffer_(buffer), size_(size)
{}

// ---------------------------------------------------------------------------
// Destructor / move
// ---------------------------------------------------------------------------

GpuBuffer::~GpuBuffer()
{
    if (buffer_) {
        wgpuBufferRelease(buffer_);
        buffer_ = nullptr;
    }
}

GpuBuffer::GpuBuffer(GpuBuffer&& other) noexcept
    : buffer_(other.buffer_), size_(other.size_)
{
    other.buffer_ = nullptr;
    other.size_   = 0;
}

GpuBuffer& GpuBuffer::operator=(GpuBuffer&& other) noexcept
{
    if (this != &other) {
        if (buffer_) {
            wgpuBufferRelease(buffer_);
        }
        buffer_ = other.buffer_;
        size_   = other.size_;
        other.buffer_ = nullptr;
        other.size_   = 0;
    }
    return *this;
}

// ---------------------------------------------------------------------------
// Factories
// ---------------------------------------------------------------------------

GpuBuffer GpuBuffer::create_storage(WGPUDevice device, uint64_t size)
{
    WGPUBufferDescriptor desc{};
    desc.nextInChain      = nullptr;
    desc.label            = {.data = "storage-buffer", .length = WGPU_STRLEN};
    desc.usage            = WGPUBufferUsage_Storage
                          | WGPUBufferUsage_CopyDst
                          | WGPUBufferUsage_CopySrc;
    desc.size             = size;
    desc.mappedAtCreation = false;

    WGPUBuffer buf = wgpuDeviceCreateBuffer(device, &desc);
    if (!buf) {
        throw std::runtime_error("GpuBuffer: failed to create storage buffer");
    }
    return GpuBuffer(buf, size);
}

GpuBuffer GpuBuffer::create_uniform(WGPUDevice device, uint64_t size)
{
    WGPUBufferDescriptor desc{};
    desc.nextInChain      = nullptr;
    desc.label            = {.data = "uniform-buffer", .length = WGPU_STRLEN};
    desc.usage            = WGPUBufferUsage_Uniform
                          | WGPUBufferUsage_CopyDst;
    desc.size             = size;
    desc.mappedAtCreation = false;

    WGPUBuffer buf = wgpuDeviceCreateBuffer(device, &desc);
    if (!buf) {
        throw std::runtime_error("GpuBuffer: failed to create uniform buffer");
    }
    return GpuBuffer(buf, size);
}

// ---------------------------------------------------------------------------
// Upload
// ---------------------------------------------------------------------------

void GpuBuffer::upload(WGPUQueue queue, const void* data, uint64_t size, uint64_t offset)
{
    wgpuQueueWriteBuffer(queue, buffer_, offset, data, static_cast<size_t>(size));
}

// ---------------------------------------------------------------------------
// Readback
// ---------------------------------------------------------------------------

namespace {

struct MapResult {
    WGPUMapAsyncStatus status = WGPUMapAsyncStatus_Unknown;
    bool done = false;
};

static void on_buffer_mapped(WGPUMapAsyncStatus status,
                              WGPUStringView /* message */,
                              void* userdata1,
                              void* /* userdata2 */)
{
    auto* result = static_cast<MapResult*>(userdata1);
    result->status = status;
    result->done   = true;
}

} // anonymous namespace

std::vector<uint8_t> GpuBuffer::readback(WGPUDevice device, WGPUQueue queue) const
{
    // 1. Create a staging buffer (CopyDst | MapRead)
    WGPUBufferDescriptor staging_desc{};
    staging_desc.nextInChain      = nullptr;
    staging_desc.label            = {.data = "staging-buffer", .length = WGPU_STRLEN};
    staging_desc.usage            = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
    staging_desc.size             = size_;
    staging_desc.mappedAtCreation = false;

    WGPUBuffer staging = wgpuDeviceCreateBuffer(device, &staging_desc);
    if (!staging) {
        throw std::runtime_error("GpuBuffer::readback: failed to create staging buffer");
    }

    // 2. Copy storage → staging via command encoder
    WGPUCommandEncoderDescriptor enc_desc{};
    enc_desc.nextInChain = nullptr;
    enc_desc.label       = {.data = "readback-encoder", .length = WGPU_STRLEN};
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, &enc_desc);

    wgpuCommandEncoderCopyBufferToBuffer(encoder, buffer_, 0, staging, 0, size_);

    WGPUCommandBufferDescriptor cmd_desc{};
    cmd_desc.nextInChain = nullptr;
    cmd_desc.label       = {.data = "readback-commands", .length = WGPU_STRLEN};
    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(encoder, &cmd_desc);
    wgpuCommandEncoderRelease(encoder);

    // 3. Submit
    wgpuQueueSubmit(queue, 1, &cmd);
    wgpuCommandBufferRelease(cmd);

    // 4. Map the staging buffer asynchronously
    MapResult map_result{};
    WGPUBufferMapCallbackInfo map_cb{};
    map_cb.nextInChain = nullptr;
    map_cb.mode        = WGPUCallbackMode_AllowSpontaneous;
    map_cb.callback    = on_buffer_mapped;
    map_cb.userdata1   = &map_result;
    map_cb.userdata2   = nullptr;

    wgpuBufferMapAsync(staging, WGPUMapMode_Read, 0, size_, map_cb);

    // 5. Poll until callback fires
    while (!map_result.done) {
        wgpuDevicePoll(device, true, nullptr);
    }

    if (map_result.status != WGPUMapAsyncStatus_Success) {
        wgpuBufferRelease(staging);
        throw std::runtime_error("GpuBuffer::readback: buffer mapping failed");
    }

    // 6. Copy out the data
    const void* mapped = wgpuBufferGetConstMappedRange(staging, 0, size_);
    std::vector<uint8_t> result(size_);
    std::memcpy(result.data(), mapped, size_);

    wgpuBufferUnmap(staging);
    wgpuBufferRelease(staging);

    return result;
}

} // namespace gg
