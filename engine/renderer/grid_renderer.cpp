#include "renderer/grid_renderer.h"

#include "renderer/camera.h"
#include "renderer/gpu_context.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace gg {

static std::string read_shader_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("GridRenderer: cannot open shader: " + path);
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

std::unique_ptr<GridRenderer> GridRenderer::create(GpuContext& ctx) {
    auto gr = std::unique_ptr<GridRenderer>(new GridRenderer(ctx));

    const std::string wgsl = read_shader_file("shaders/grid.wgsl");

    WGPUShaderSourceWGSL wgsl_source{};
    wgsl_source.chain.next = nullptr;
    wgsl_source.chain.sType = WGPUSType_ShaderSourceWGSL;
    wgsl_source.code = {.data = wgsl.c_str(), .length = wgsl.size()};

    WGPUShaderModuleDescriptor shader_desc{};
    shader_desc.nextInChain = &wgsl_source.chain;
    shader_desc.label = {.data = "grid-shader", .length = WGPU_STRLEN};

    WGPUShaderModule shader = wgpuDeviceCreateShaderModule(ctx.device(), &shader_desc);
    if (!shader) {
        throw std::runtime_error("GridRenderer: failed to create shader module");
    }

    // Uniform buffer (single mat4 = 64 bytes)
    {
        WGPUBufferDescriptor desc{};
        desc.nextInChain = nullptr;
        desc.label = {.data = "grid-uniform-buf", .length = WGPU_STRLEN};
        desc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform;
        desc.size = sizeof(Mat4);
        desc.mappedAtCreation = false;
        gr->uniform_buffer_ = wgpuDeviceCreateBuffer(ctx.device(), &desc);
    }

    // Vertex state (no vertex buffer — procedural from vertex_index)
    WGPUVertexState vertex{};
    vertex.nextInChain = nullptr;
    vertex.module = shader;
    vertex.entryPoint = {.data = "vs_main", .length = WGPU_STRLEN};
    vertex.constantCount = 0;
    vertex.constants = nullptr;
    vertex.bufferCount = 0;
    vertex.buffers = nullptr;

    // Fragment with alpha blending
    WGPUBlendComponent blend_color{};
    blend_color.srcFactor = WGPUBlendFactor_SrcAlpha;
    blend_color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
    blend_color.operation = WGPUBlendOperation_Add;

    WGPUBlendComponent blend_alpha{};
    blend_alpha.srcFactor = WGPUBlendFactor_One;
    blend_alpha.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
    blend_alpha.operation = WGPUBlendOperation_Add;

    WGPUBlendState blend{};
    blend.color = blend_color;
    blend.alpha = blend_alpha;

    WGPUColorTargetState color_target{};
    color_target.nextInChain = nullptr;
    color_target.format = ctx.surface_format();
    color_target.blend = &blend;
    color_target.writeMask = WGPUColorWriteMask_All;

    WGPUFragmentState fragment{};
    fragment.nextInChain = nullptr;
    fragment.module = shader;
    fragment.entryPoint = {.data = "fs_main", .length = WGPU_STRLEN};
    fragment.constantCount = 0;
    fragment.constants = nullptr;
    fragment.targetCount = 1;
    fragment.targets = &color_target;

    // Primitive (triangles, no cull — grid visible from both sides)
    WGPUPrimitiveState primitive{};
    primitive.nextInChain = nullptr;
    primitive.topology = WGPUPrimitiveTopology_TriangleList;
    primitive.stripIndexFormat = WGPUIndexFormat_Undefined;
    primitive.frontFace = WGPUFrontFace_CCW;
    primitive.cullMode = WGPUCullMode_None;

    // Depth (read but don't write — grid behind meshes)
    WGPUDepthStencilState depth_stencil{};
    depth_stencil.nextInChain = nullptr;
    depth_stencil.format = WGPUTextureFormat_Depth24Plus;
    depth_stencil.depthWriteEnabled = WGPUOptionalBool_False;
    depth_stencil.depthCompare = WGPUCompareFunction_Less;
    depth_stencil.stencilFront = {
        .compare = WGPUCompareFunction_Always,
        .failOp = WGPUStencilOperation_Keep,
        .depthFailOp = WGPUStencilOperation_Keep,
        .passOp = WGPUStencilOperation_Keep,
    };
    depth_stencil.stencilBack = depth_stencil.stencilFront;
    depth_stencil.stencilReadMask = 0xFFFFFFFF;
    depth_stencil.stencilWriteMask = 0xFFFFFFFF;
    depth_stencil.depthBias = 0;
    depth_stencil.depthBiasSlopeScale = 0.0f;
    depth_stencil.depthBiasClamp = 0.0f;

    WGPUMultisampleState multisample{};
    multisample.nextInChain = nullptr;
    multisample.count = 1;
    multisample.mask = 0xFFFFFFFF;
    multisample.alphaToCoverageEnabled = false;

    WGPURenderPipelineDescriptor pipeline_desc{};
    pipeline_desc.nextInChain = nullptr;
    pipeline_desc.label = {.data = "grid-pipeline", .length = WGPU_STRLEN};
    pipeline_desc.layout = nullptr;
    pipeline_desc.vertex = vertex;
    pipeline_desc.primitive = primitive;
    pipeline_desc.depthStencil = &depth_stencil;
    pipeline_desc.multisample = multisample;
    pipeline_desc.fragment = &fragment;

    gr->pipeline_ = wgpuDeviceCreateRenderPipeline(ctx.device(), &pipeline_desc);
    wgpuShaderModuleRelease(shader);

    if (!gr->pipeline_) {
        throw std::runtime_error("GridRenderer: failed to create pipeline");
    }

    // Bind group
    WGPUBindGroupLayout bgl = wgpuRenderPipelineGetBindGroupLayout(gr->pipeline_, 0);
    WGPUBindGroupEntry entry{};
    entry.nextInChain = nullptr;
    entry.binding = 0;
    entry.buffer = gr->uniform_buffer_;
    entry.offset = 0;
    entry.size = sizeof(Mat4);
    entry.sampler = nullptr;
    entry.textureView = nullptr;

    WGPUBindGroupDescriptor bg_desc{};
    bg_desc.nextInChain = nullptr;
    bg_desc.label = {.data = "grid-bind-group", .length = WGPU_STRLEN};
    bg_desc.layout = bgl;
    bg_desc.entryCount = 1;
    bg_desc.entries = &entry;

    gr->bind_group_ = wgpuDeviceCreateBindGroup(ctx.device(), &bg_desc);
    wgpuBindGroupLayoutRelease(bgl);

    return gr;
}

GridRenderer::GridRenderer(GpuContext& ctx) : ctx_(ctx) {}

GridRenderer::~GridRenderer() {
    if (bind_group_) {
        wgpuBindGroupRelease(bind_group_);
    }
    if (uniform_buffer_) {
        wgpuBufferRelease(uniform_buffer_);
    }
    if (pipeline_) {
        wgpuRenderPipelineRelease(pipeline_);
    }
}

void GridRenderer::update_camera(const Camera& camera) {
    view_projection_ = camera.view_projection_matrix();
}

void GridRenderer::draw(WGPURenderPassEncoder pass) {
    wgpuQueueWriteBuffer(ctx_.queue(), uniform_buffer_, 0, &view_projection_, sizeof(Mat4));
    wgpuRenderPassEncoderSetPipeline(pass, pipeline_);
    wgpuRenderPassEncoderSetBindGroup(pass, 0, bind_group_, 0, nullptr);
    wgpuRenderPassEncoderDraw(pass, 6, 1, 0, 0);
}

} // namespace gg
