#include "renderer/mesh.h"
#include "renderer/gpu_context.h"

namespace gg {

std::unique_ptr<Mesh> Mesh::create(GpuContext& ctx,
                                   const std::vector<Vertex>& vertices,
                                   const std::vector<uint32_t>& indices) {
    if (vertices.empty() || indices.empty()) {
        return nullptr;
    }

    auto mesh = std::unique_ptr<Mesh>(new Mesh());
    mesh->vertex_count_ = static_cast<uint32_t>(vertices.size());
    mesh->index_count_ = static_cast<uint32_t>(indices.size());

    // Vertex buffer
    {
        WGPUBufferDescriptor desc{};
        desc.nextInChain = nullptr;
        desc.label = {.data = "mesh-vertex-buf", .length = WGPU_STRLEN};
        desc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex;
        desc.size = vertices.size() * sizeof(Vertex);
        desc.mappedAtCreation = false;
        mesh->vertex_buffer_ = wgpuDeviceCreateBuffer(ctx.device(), &desc);
        wgpuQueueWriteBuffer(ctx.queue(), mesh->vertex_buffer_, 0,
                             vertices.data(), desc.size);
    }

    // Index buffer
    {
        WGPUBufferDescriptor desc{};
        desc.nextInChain = nullptr;
        desc.label = {.data = "mesh-index-buf", .length = WGPU_STRLEN};
        desc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Index;
        desc.size = indices.size() * sizeof(uint32_t);
        desc.mappedAtCreation = false;
        mesh->index_buffer_ = wgpuDeviceCreateBuffer(ctx.device(), &desc);
        wgpuQueueWriteBuffer(ctx.queue(), mesh->index_buffer_, 0,
                             indices.data(), desc.size);
    }

    return mesh;
}

Mesh::~Mesh() {
    if (vertex_buffer_) {
        wgpuBufferRelease(vertex_buffer_);
    }
    if (index_buffer_) {
        wgpuBufferRelease(index_buffer_);
    }
}

} // namespace gg
