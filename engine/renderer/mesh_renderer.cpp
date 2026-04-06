#include "renderer/mesh_renderer.h"

#include "math/mat4.h"
#include "renderer/camera.h"
#include "renderer/gpu_context.h"
#include "renderer/mesh.h"
#include "renderer/shader_utils.h"

#include <cstdio>
#include <cstring>

namespace gg {

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------

std::unique_ptr<MeshRenderer> MeshRenderer::create(GpuContext& ctx) {
    auto mr = std::unique_ptr<MeshRenderer>(new MeshRenderer(ctx));

    // -- Shader module -------------------------------------------------------
    const std::string wgsl = read_shader_file("shaders/mesh.wgsl");

    WGPUShaderSourceWGSL wgsl_source{};
    wgsl_source.chain.next = nullptr;
    wgsl_source.chain.sType = WGPUSType_ShaderSourceWGSL;
    wgsl_source.code = {.data = wgsl.c_str(), .length = wgsl.size()};

    WGPUShaderModuleDescriptor shader_desc{};
    shader_desc.nextInChain = &wgsl_source.chain;
    shader_desc.label = {.data = "mesh-shader", .length = WGPU_STRLEN};

    WGPUShaderModule shader = wgpuDeviceCreateShaderModule(ctx.device(), &shader_desc);
    if (!shader) {
        throw std::runtime_error("MeshRenderer: failed to create shader module");
    }

    // -- Camera + model uniform buffer (128 bytes = 2 * mat4x4<f32>) ---------
    {
        WGPUBufferDescriptor desc{};
        desc.nextInChain = nullptr;
        desc.label = {.data = "camera-uniform-buf", .length = WGPU_STRLEN};
        desc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform;
        desc.size = sizeof(Mat4) * 2;
        desc.mappedAtCreation = false;
        mr->camera_buffer_ = wgpuDeviceCreateBuffer(ctx.device(), &desc);
        if (!mr->camera_buffer_) {
            wgpuShaderModuleRelease(shader);
            throw std::runtime_error("MeshRenderer: failed to create camera uniform buffer");
        }
    }

    // -- Vertex buffer layout ------------------------------------------------
    // @location(0) position: Float32x3, offset 0
    // @location(1) normal:   Float32x3, offset 12
    // @location(2) uv:       Float32x2, offset 24
    // arrayStride = 32, stepMode = Vertex
    WGPUVertexAttribute attribs[3]{};
    attribs[0].shaderLocation = 0;
    attribs[0].format = WGPUVertexFormat_Float32x3;
    attribs[0].offset = 0;
    attribs[1].shaderLocation = 1;
    attribs[1].format = WGPUVertexFormat_Float32x3;
    attribs[1].offset = 12;
    attribs[2].shaderLocation = 2;
    attribs[2].format = WGPUVertexFormat_Float32x2;
    attribs[2].offset = 24;

    WGPUVertexBufferLayout vb_layout{};
    vb_layout.arrayStride = 32;
    vb_layout.stepMode = WGPUVertexStepMode_Vertex;
    vb_layout.attributeCount = 3;
    vb_layout.attributes = attribs;

    // -- Vertex state --------------------------------------------------------
    WGPUVertexState vertex{};
    vertex.nextInChain = nullptr;
    vertex.module = shader;
    vertex.entryPoint = {.data = "vs_main", .length = WGPU_STRLEN};
    vertex.constantCount = 0;
    vertex.constants = nullptr;
    vertex.bufferCount = 1;
    vertex.buffers = &vb_layout;

    // -- Fragment state -------------------------------------------------------
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

    // -- Primitive state ------------------------------------------------------
    WGPUPrimitiveState primitive{};
    primitive.nextInChain = nullptr;
    primitive.topology = WGPUPrimitiveTopology_TriangleList;
    primitive.stripIndexFormat = WGPUIndexFormat_Undefined;
    primitive.frontFace = WGPUFrontFace_CCW;
    primitive.cullMode = WGPUCullMode_Back;

    // -- Depth stencil state --------------------------------------------------
    WGPUDepthStencilState depth_stencil{};
    depth_stencil.nextInChain = nullptr;
    depth_stencil.format = WGPUTextureFormat_Depth24Plus;
    depth_stencil.depthWriteEnabled = WGPUOptionalBool_True;
    depth_stencil.depthCompare = WGPUCompareFunction_Less;
    depth_stencil.stencilFront = {
        .compare = WGPUCompareFunction_Always,
        .failOp = WGPUStencilOperation_Keep,
        .depthFailOp = WGPUStencilOperation_Keep,
        .passOp = WGPUStencilOperation_Keep,
    };
    depth_stencil.stencilBack = {
        .compare = WGPUCompareFunction_Always,
        .failOp = WGPUStencilOperation_Keep,
        .depthFailOp = WGPUStencilOperation_Keep,
        .passOp = WGPUStencilOperation_Keep,
    };
    depth_stencil.stencilReadMask = 0xFFFFFFFF;
    depth_stencil.stencilWriteMask = 0xFFFFFFFF;
    depth_stencil.depthBias = 0;
    depth_stencil.depthBiasSlopeScale = 0.0f;
    depth_stencil.depthBiasClamp = 0.0f;

    // -- Multisample state ----------------------------------------------------
    WGPUMultisampleState multisample{};
    multisample.nextInChain = nullptr;
    multisample.count = 1;
    multisample.mask = 0xFFFFFFFF;
    multisample.alphaToCoverageEnabled = false;

    // -- Pipeline -------------------------------------------------------------
    WGPURenderPipelineDescriptor pipeline_desc{};
    pipeline_desc.nextInChain = nullptr;
    pipeline_desc.label = {.data = "mesh-pipeline", .length = WGPU_STRLEN};
    pipeline_desc.layout = nullptr; // auto layout
    pipeline_desc.vertex = vertex;
    pipeline_desc.primitive = primitive;
    pipeline_desc.depthStencil = &depth_stencil;
    pipeline_desc.multisample = multisample;
    pipeline_desc.fragment = &fragment;

    mr->pipeline_ = wgpuDeviceCreateRenderPipeline(ctx.device(), &pipeline_desc);
    wgpuShaderModuleRelease(shader);

    if (!mr->pipeline_) {
        throw std::runtime_error("MeshRenderer: failed to create render pipeline");
    }

    // -- Bind group layout (from pipeline, group 0) ---------------------------
    WGPUBindGroupLayout bgl = wgpuRenderPipelineGetBindGroupLayout(mr->pipeline_, 0);
    if (!bgl) {
        throw std::runtime_error("MeshRenderer: failed to get bind group layout");
    }

    // -- Bind group (camera uniform at binding 0) -----------------------------
    WGPUBindGroupEntry bg_entry{};
    bg_entry.nextInChain = nullptr;
    bg_entry.binding = 0;
    bg_entry.buffer = mr->camera_buffer_;
    bg_entry.offset = 0;
    bg_entry.size = sizeof(Mat4) * 2;
    bg_entry.sampler = nullptr;
    bg_entry.textureView = nullptr;

    WGPUBindGroupDescriptor bg_desc{};
    bg_desc.nextInChain = nullptr;
    bg_desc.label = {.data = "camera-bind-group", .length = WGPU_STRLEN};
    bg_desc.layout = bgl;
    bg_desc.entryCount = 1;
    bg_desc.entries = &bg_entry;

    mr->bind_group_ = wgpuDeviceCreateBindGroup(ctx.device(), &bg_desc);
    wgpuBindGroupLayoutRelease(bgl);

    if (!mr->bind_group_) {
        throw std::runtime_error("MeshRenderer: failed to create bind group");
    }

    // -- Depth texture (initial size from surface) ----------------------------
    mr->resize_depth(ctx.surface_width(), ctx.surface_height());

    return mr;
}

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

MeshRenderer::MeshRenderer(GpuContext& ctx) : ctx_(ctx) {}

MeshRenderer::~MeshRenderer() {
    if (depth_view_) {
        wgpuTextureViewRelease(depth_view_);
        depth_view_ = nullptr;
    }
    if (depth_texture_) {
        wgpuTextureRelease(depth_texture_);
        depth_texture_ = nullptr;
    }
    if (bind_group_) {
        wgpuBindGroupRelease(bind_group_);
        bind_group_ = nullptr;
    }
    if (camera_buffer_) {
        wgpuBufferRelease(camera_buffer_);
        camera_buffer_ = nullptr;
    }
    if (pipeline_) {
        wgpuRenderPipelineRelease(pipeline_);
        pipeline_ = nullptr;
    }
}

// ---------------------------------------------------------------------------
// update_camera
// ---------------------------------------------------------------------------

void MeshRenderer::update_camera(const Camera& camera) {
    view_projection_ = camera.view_projection_matrix();
}

// ---------------------------------------------------------------------------
// draw
// ---------------------------------------------------------------------------

void MeshRenderer::draw(const Mesh& mesh, const Mat4& model_matrix, WGPURenderPassEncoder pass) {
    struct Uniforms {
        Mat4 view_proj;
        Mat4 model;
    } uniforms{.view_proj = view_projection_, .model = model_matrix};

    wgpuQueueWriteBuffer(ctx_.queue(), camera_buffer_, 0, &uniforms, sizeof(Uniforms));
    wgpuRenderPassEncoderSetPipeline(pass, pipeline_);
    wgpuRenderPassEncoderSetBindGroup(pass, 0, bind_group_, 0, nullptr);
    wgpuRenderPassEncoderSetVertexBuffer(pass, 0, mesh.vertex_buffer(), 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderSetIndexBuffer(
        pass, mesh.index_buffer(), WGPUIndexFormat_Uint32, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderDrawIndexed(pass, mesh.index_count(), 1, 0, 0, 0);
}

// ---------------------------------------------------------------------------
// resize_depth
// ---------------------------------------------------------------------------

void MeshRenderer::resize_depth(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) {
        return;
    }

    // Release old resources
    if (depth_view_) {
        wgpuTextureViewRelease(depth_view_);
        depth_view_ = nullptr;
    }
    if (depth_texture_) {
        wgpuTextureRelease(depth_texture_);
        depth_texture_ = nullptr;
    }

    // Create new depth texture
    WGPUTextureDescriptor tex_desc{};
    tex_desc.nextInChain = nullptr;
    tex_desc.label = {.data = "depth-texture", .length = WGPU_STRLEN};
    tex_desc.usage = WGPUTextureUsage_RenderAttachment;
    tex_desc.dimension = WGPUTextureDimension_2D;
    tex_desc.size = {.width = width, .height = height, .depthOrArrayLayers = 1};
    tex_desc.format = WGPUTextureFormat_Depth24Plus;
    tex_desc.mipLevelCount = 1;
    tex_desc.sampleCount = 1;
    tex_desc.viewFormatCount = 0;
    tex_desc.viewFormats = nullptr;

    depth_texture_ = wgpuDeviceCreateTexture(ctx_.device(), &tex_desc);
    if (!depth_texture_) {
        std::fprintf(
            stderr, "[MeshRenderer] Failed to create depth texture (%ux%u)\n", width, height);
        return;
    }

    WGPUTextureViewDescriptor view_desc{};
    view_desc.nextInChain = nullptr;
    view_desc.label = {.data = "depth-view", .length = WGPU_STRLEN};
    view_desc.format = WGPUTextureFormat_Depth24Plus;
    view_desc.dimension = WGPUTextureViewDimension_2D;
    view_desc.baseMipLevel = 0;
    view_desc.mipLevelCount = 1;
    view_desc.baseArrayLayer = 0;
    view_desc.arrayLayerCount = 1;
    view_desc.aspect = WGPUTextureAspect_DepthOnly;
    view_desc.usage = WGPUTextureUsage_RenderAttachment;

    depth_view_ = wgpuTextureCreateView(depth_texture_, &view_desc);
    if (!depth_view_) {
        std::fprintf(stderr, "[MeshRenderer] Failed to create depth texture view\n");
    }
}

} // namespace gg
