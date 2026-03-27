#pragma once

#include <webgpu/webgpu.h>
#include <memory>
#include <string>
#include <vector>

namespace gg {

/// Wraps a WGPUComputePipeline created from WGSL source.
/// Uses auto-layout (layout = nullptr) so bind group layouts are inferred
/// from the shader.
class ComputePipeline {
public:
    /// Create a compute pipeline from WGSL shader source.
    static std::unique_ptr<ComputePipeline> create(
        WGPUDevice device,
        const std::string& shader_source,
        const std::string& entry_point = "main");

    ~ComputePipeline();

    // No copy or move (owns GPU resources)
    ComputePipeline(const ComputePipeline&) = delete;
    ComputePipeline& operator=(const ComputePipeline&) = delete;
    ComputePipeline(ComputePipeline&&) = delete;
    ComputePipeline& operator=(ComputePipeline&&) = delete;

    /// Dispatch the compute shader.
    /// @param buffers  Buffers to bind at group 0 (binding 0, 1, 2, …)
    void dispatch(WGPUDevice device,
                  WGPUQueue queue,
                  const std::vector<WGPUBuffer>& buffers,
                  uint32_t workgroups_x,
                  uint32_t workgroups_y = 1,
                  uint32_t workgroups_z = 1);

private:
    ComputePipeline() = default;

    WGPUComputePipeline pipeline_ = nullptr;
};

} // namespace gg
