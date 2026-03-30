#include "renderer/renderer.h"

#include "renderer/gpu_context.h"

#include <cstdio>
#include <stdexcept>

namespace gg {

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------

std::unique_ptr<Renderer> Renderer::create(GpuContext& ctx, const std::string& shader_source) {
    auto renderer = std::unique_ptr<Renderer>(new Renderer(ctx));

    // -- Shader module -------------------------------------------------------
    WGPUShaderSourceWGSL wgsl_source{};
    wgsl_source.chain.next = nullptr;
    wgsl_source.chain.sType = WGPUSType_ShaderSourceWGSL;
    wgsl_source.code = {.data = shader_source.c_str(), .length = shader_source.size()};

    WGPUShaderModuleDescriptor shader_desc{};
    shader_desc.nextInChain = &wgsl_source.chain;
    shader_desc.label = {.data = "triangle-shader", .length = WGPU_STRLEN};

    WGPUShaderModule shader = wgpuDeviceCreateShaderModule(ctx.device(), &shader_desc);
    if (!shader) {
        throw std::runtime_error("Renderer: failed to create shader module");
    }

    // -- Pipeline ------------------------------------------------------------
    WGPUColorTargetState color_target{};
    color_target.nextInChain = nullptr;
    color_target.format = ctx.surface_format();
    color_target.blend = nullptr;
    color_target.writeMask = WGPUColorWriteMask_All;

    WGPUFragmentState fragment{};
    fragment.nextInChain = nullptr;
    fragment.module = shader;
    fragment.entryPoint = {.data = "fs_main", .length = WGPU_STRLEN};
    fragment.constantCount = 0;
    fragment.constants = nullptr;
    fragment.targetCount = 1;
    fragment.targets = &color_target;

    WGPUVertexState vertex{};
    vertex.nextInChain = nullptr;
    vertex.module = shader;
    vertex.entryPoint = {.data = "vs_main", .length = WGPU_STRLEN};
    vertex.constantCount = 0;
    vertex.constants = nullptr;
    vertex.bufferCount = 0;
    vertex.buffers = nullptr;

    WGPUPrimitiveState primitive{};
    primitive.nextInChain = nullptr;
    primitive.topology = WGPUPrimitiveTopology_TriangleList;
    primitive.stripIndexFormat = WGPUIndexFormat_Undefined;
    primitive.frontFace = WGPUFrontFace_CCW;
    primitive.cullMode = WGPUCullMode_None;

    WGPUMultisampleState multisample{};
    multisample.nextInChain = nullptr;
    multisample.count = 1;
    multisample.mask = 0xFFFFFFFF;
    multisample.alphaToCoverageEnabled = false;

    WGPURenderPipelineDescriptor pipeline_desc{};
    pipeline_desc.nextInChain = nullptr;
    pipeline_desc.label = {.data = "triangle-pipeline", .length = WGPU_STRLEN};
    pipeline_desc.layout = nullptr; // auto layout
    pipeline_desc.vertex = vertex;
    pipeline_desc.primitive = primitive;
    pipeline_desc.depthStencil = nullptr;
    pipeline_desc.multisample = multisample;
    pipeline_desc.fragment = &fragment;

    renderer->pipeline_ = wgpuDeviceCreateRenderPipeline(ctx.device(), &pipeline_desc);

    // Shader module can be released after pipeline is created
    wgpuShaderModuleRelease(shader);

    if (!renderer->pipeline_) {
        throw std::runtime_error("Renderer: failed to create render pipeline");
    }

    return renderer;
}

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

Renderer::Renderer(GpuContext& ctx) : ctx_(ctx) {}

Renderer::~Renderer() {
    if (render_pass_) {
        wgpuRenderPassEncoderRelease(render_pass_);
        render_pass_ = nullptr;
    }
    if (encoder_) {
        wgpuCommandEncoderRelease(encoder_);
        encoder_ = nullptr;
    }
    if (frame_view_) {
        wgpuTextureViewRelease(frame_view_);
        frame_view_ = nullptr;
    }
    if (frame_texture_) {
        wgpuTextureRelease(frame_texture_);
        frame_texture_ = nullptr;
    }
    if (pipeline_) {
        wgpuRenderPipelineRelease(pipeline_);
        pipeline_ = nullptr;
    }
}

// ---------------------------------------------------------------------------
// Per-frame methods
// ---------------------------------------------------------------------------

bool Renderer::begin_frame() {
    WGPUSurfaceTexture surface_tex{};
    surface_tex.nextInChain = nullptr;
    wgpuSurfaceGetCurrentTexture(ctx_.surface(), &surface_tex);

    if (surface_tex.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal &&
        surface_tex.status != WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal) {
        std::fprintf(stderr,
                     "[Renderer] Surface texture not ready (status=%d)\n",
                     static_cast<int>(surface_tex.status));
        if (surface_tex.texture) {
            wgpuTextureRelease(surface_tex.texture);
        }
        return false;
    }

    frame_texture_ = surface_tex.texture;

    WGPUTextureViewDescriptor view_desc{};
    view_desc.nextInChain = nullptr;
    view_desc.label = {.data = "frame-view", .length = WGPU_STRLEN};
    view_desc.format = ctx_.surface_format();
    view_desc.dimension = WGPUTextureViewDimension_2D;
    view_desc.baseMipLevel = 0;
    view_desc.mipLevelCount = 1;
    view_desc.baseArrayLayer = 0;
    view_desc.arrayLayerCount = 1;
    view_desc.aspect = WGPUTextureAspect_All;
    view_desc.usage = WGPUTextureUsage_RenderAttachment;

    frame_view_ = wgpuTextureCreateView(frame_texture_, &view_desc);
    if (!frame_view_) {
        wgpuTextureRelease(frame_texture_);
        frame_texture_ = nullptr;
        return false;
    }

    WGPUCommandEncoderDescriptor enc_desc{};
    enc_desc.nextInChain = nullptr;
    enc_desc.label = {.data = "frame-encoder", .length = WGPU_STRLEN};
    encoder_ = wgpuDeviceCreateCommandEncoder(ctx_.device(), &enc_desc);

    // Begin render pass
    WGPURenderPassColorAttachment color_att{};
    color_att.nextInChain = nullptr;
    color_att.view = frame_view_;
    color_att.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
    color_att.resolveTarget = nullptr;
    color_att.loadOp = WGPULoadOp_Clear;
    color_att.storeOp = WGPUStoreOp_Store;
    color_att.clearValue = {.r = 0.1, .g = 0.1, .b = 0.1, .a = 1.0};

    WGPURenderPassDescriptor pass_desc{};
    pass_desc.nextInChain = nullptr;
    pass_desc.label = {.data = "main-pass", .length = WGPU_STRLEN};
    pass_desc.colorAttachmentCount = 1;
    pass_desc.colorAttachments = &color_att;
    pass_desc.depthStencilAttachment = nullptr;
    pass_desc.occlusionQuerySet = nullptr;
    pass_desc.timestampWrites = nullptr;

    render_pass_ = wgpuCommandEncoderBeginRenderPass(encoder_, &pass_desc);
    return true;
}

void Renderer::draw_triangle() {
    if (!render_pass_ || !pipeline_) {
        return;
    }
    wgpuRenderPassEncoderSetPipeline(render_pass_, pipeline_);
    wgpuRenderPassEncoderDraw(render_pass_, 3, 1, 0, 0);
}

void Renderer::end_frame() {
    if (!render_pass_) {
        return;
    }

    wgpuRenderPassEncoderEnd(render_pass_);
    wgpuRenderPassEncoderRelease(render_pass_);
    render_pass_ = nullptr;

    WGPUCommandBufferDescriptor cmd_desc{};
    cmd_desc.nextInChain = nullptr;
    cmd_desc.label = {.data = "frame-commands", .length = WGPU_STRLEN};
    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(encoder_, &cmd_desc);
    wgpuCommandEncoderRelease(encoder_);
    encoder_ = nullptr;

    wgpuQueueSubmit(ctx_.queue(), 1, &cmd);
    wgpuCommandBufferRelease(cmd);

    wgpuSurfacePresent(ctx_.surface());

    wgpuTextureViewRelease(frame_view_);
    frame_view_ = nullptr;
    wgpuTextureRelease(frame_texture_);
    frame_texture_ = nullptr;
}

} // namespace gg
