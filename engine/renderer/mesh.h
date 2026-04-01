#pragma once
#include <cstdint>
#include <memory>
#include <vector>
#include <webgpu/webgpu.h>

namespace gg {

class GpuContext;

struct Vertex {
    float position[3];
    float normal[3];
    float uv[2];
};

class Mesh {
public:
    static std::unique_ptr<Mesh> create(GpuContext& ctx,
                                        const std::vector<Vertex>& vertices,
                                        const std::vector<uint32_t>& indices);
    ~Mesh();
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&&) = delete;
    Mesh& operator=(Mesh&&) = delete;

    [[nodiscard]] WGPUBuffer vertex_buffer() const { return vertex_buffer_; }
    [[nodiscard]] WGPUBuffer index_buffer() const { return index_buffer_; }
    [[nodiscard]] uint32_t vertex_count() const { return vertex_count_; }
    [[nodiscard]] uint32_t index_count() const { return index_count_; }

private:
    Mesh() = default;
    WGPUBuffer vertex_buffer_ = nullptr;
    WGPUBuffer index_buffer_ = nullptr;
    uint32_t vertex_count_ = 0;
    uint32_t index_count_ = 0;
};

} // namespace gg
