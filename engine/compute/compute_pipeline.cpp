#include "compute/compute_pipeline.h"

#include <stdexcept>
#include <cstdio>

namespace gg {

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------

std::unique_ptr<ComputePipeline> ComputePipeline::create(
    WGPUDevice device,
    const std::string& shader_source,
    const std::string& entry_point)
{
    auto pipeline = std::unique_ptr<ComputePipeline>(new ComputePipeline());

    // -- Shader module (same pattern as renderer.cpp) ------------------------
    WGPUShaderSourceWGSL wgsl_source{};
    wgsl_source.chain.next  = nullptr;
    wgsl_source.chain.sType = WGPUSType_ShaderSourceWGSL;
    wgsl_source.code        = {.data   = shader_source.c_str(),
                               .length = shader_source.size()};

    WGPUShaderModuleDescriptor shader_desc{};
    shader_desc.nextInChain = &wgsl_source.chain;
    shader_desc.label       = {.data = "compute-shader", .length = WGPU_STRLEN};

    WGPUShaderModule shader = wgpuDeviceCreateShaderModule(device, &shader_desc);
    if (!shader) {
        throw std::runtime_error("ComputePipeline: failed to create shader module");
    }

    // -- Compute pipeline with auto layout -----------------------------------
    WGPUProgrammableStageDescriptor compute_stage{};
    compute_stage.nextInChain   = nullptr;
    compute_stage.module        = shader;
    compute_stage.entryPoint    = {.data   = entry_point.c_str(),
                                   .length = entry_point.size()};
    compute_stage.constantCount = 0;
    compute_stage.constants     = nullptr;

    WGPUComputePipelineDescriptor pipeline_desc{};
    pipeline_desc.nextInChain = nullptr;
    pipeline_desc.label       = {.data = "compute-pipeline", .length = WGPU_STRLEN};
    pipeline_desc.layout      = nullptr;  // auto layout
    pipeline_desc.compute     = compute_stage;

    pipeline->pipeline_ = wgpuDeviceCreateComputePipeline(device, &pipeline_desc);

    wgpuShaderModuleRelease(shader);

    if (!pipeline->pipeline_) {
        throw std::runtime_error("ComputePipeline: failed to create compute pipeline");
    }

    return pipeline;
}

// ---------------------------------------------------------------------------
// Destructor
// ---------------------------------------------------------------------------

ComputePipeline::~ComputePipeline()
{
    if (pipeline_) {
        wgpuComputePipelineRelease(pipeline_);
        pipeline_ = nullptr;
    }
}

// ---------------------------------------------------------------------------
// Dispatch
// ---------------------------------------------------------------------------

void ComputePipeline::dispatch(WGPUDevice device,
                               WGPUQueue queue,
                               const std::vector<WGPUBuffer>& buffers,
                               uint32_t workgroups_x,
                               uint32_t workgroups_y,
                               uint32_t workgroups_z)
{
    // Get bind group layout from the pipeline (auto-inferred)
    WGPUBindGroupLayout layout =
        wgpuComputePipelineGetBindGroupLayout(pipeline_, 0);

    // Build bind group entries: one per buffer
    std::vector<WGPUBindGroupEntry> entries;
    entries.reserve(buffers.size());
    for (uint32_t i = 0; i < static_cast<uint32_t>(buffers.size()); ++i) {
        WGPUBindGroupEntry entry{};
        entry.nextInChain = nullptr;
        entry.binding     = i;
        entry.buffer      = buffers[i];
        entry.offset      = 0;
        entry.size        = wgpuBufferGetSize(buffers[i]);
        entry.sampler     = nullptr;
        entry.textureView = nullptr;
        entries.push_back(entry);
    }

    WGPUBindGroupDescriptor bg_desc{};
    bg_desc.nextInChain = nullptr;
    bg_desc.label       = {.data = "compute-bind-group", .length = WGPU_STRLEN};
    bg_desc.layout      = layout;
    bg_desc.entryCount  = entries.size();
    bg_desc.entries     = entries.data();

    WGPUBindGroup bind_group = wgpuDeviceCreateBindGroup(device, &bg_desc);

    wgpuBindGroupLayoutRelease(layout);

    if (!bind_group) {
        throw std::runtime_error("ComputePipeline::dispatch: failed to create bind group");
    }

    // Encode compute pass
    WGPUCommandEncoderDescriptor enc_desc{};
    enc_desc.nextInChain = nullptr;
    enc_desc.label       = {.data = "compute-encoder", .length = WGPU_STRLEN};
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, &enc_desc);

    WGPUComputePassDescriptor pass_desc{};
    pass_desc.nextInChain    = nullptr;
    pass_desc.label          = {.data = "compute-pass", .length = WGPU_STRLEN};
    pass_desc.timestampWrites = nullptr;

    WGPUComputePassEncoder pass = wgpuCommandEncoderBeginComputePass(encoder, &pass_desc);

    wgpuComputePassEncoderSetPipeline(pass, pipeline_);
    wgpuComputePassEncoderSetBindGroup(pass, 0, bind_group, 0, nullptr);
    wgpuComputePassEncoderDispatchWorkgroups(pass, workgroups_x, workgroups_y, workgroups_z);
    wgpuComputePassEncoderEnd(pass);
    wgpuComputePassEncoderRelease(pass);

    WGPUCommandBufferDescriptor cmd_desc{};
    cmd_desc.nextInChain = nullptr;
    cmd_desc.label       = {.data = "compute-commands", .length = WGPU_STRLEN};
    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(encoder, &cmd_desc);
    wgpuCommandEncoderRelease(encoder);

    wgpuQueueSubmit(queue, 1, &cmd);
    wgpuCommandBufferRelease(cmd);

    wgpuBindGroupRelease(bind_group);
}

} // namespace gg
