#include "renderer/gizmo_renderer.h"

#include "renderer/camera.h"
#include "renderer/gpu_context.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <numbers>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace gg {

static std::string read_shader_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("GizmoRenderer: cannot open shader: " + path);
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

struct GizmoVertex {
    float position[3];
    float color[3];
};

static constexpr float SHAFT_LENGTH = 1.2f;
static constexpr float CONE_LENGTH = 0.3f;
static constexpr float SHAFT_RADIUS = 0.025f;
static constexpr float CONE_RADIUS = 0.07f;
static constexpr int SEGMENTS = 8;
// Per axis: shaft (SEGMENTS * 6) + cone (SEGMENTS * 3) = 8*6 + 8*3 = 72
// 3 axes: 216 vertices
static constexpr uint32_t VERTS_PER_AXIS = SEGMENTS * 6 + SEGMENTS * 3;
static constexpr uint32_t VERTEX_COUNT = VERTS_PER_AXIS * 3;

/// Generate arrow vertices along a given axis direction.
/// axis: 0=X, 1=Y, 2=Z
static void build_arrow(
    std::vector<GizmoVertex>& out, float px, float py, float pz,
    int axis, const float color[3], float scale) {
    float shaft_length = SHAFT_LENGTH * scale;
    float cone_length = CONE_LENGTH * scale;
    float shaft_radius = SHAFT_RADIUS * scale;
    float cone_radius = CONE_RADIUS * scale;

    // Two perpendicular directions to the arrow axis
    // axis=0(X): perp1=Y, perp2=Z
    // axis=1(Y): perp1=Z, perp2=X
    // axis=2(Z): perp1=X, perp2=Y
    auto make_point = [&](float along, float r1, float r2) -> GizmoVertex {
        float pos[3] = {px, py, pz};
        int p1 = (axis + 1) % 3;
        int p2 = (axis + 2) % 3;
        pos[axis] += along;
        pos[p1] += r1;
        pos[p2] += r2;
        return {{pos[0], pos[1], pos[2]}, {color[0], color[1], color[2]}};
    };

    constexpr float tau = 2.0f * std::numbers::pi_v<float>;

    // Shaft: cylinder from 0 to shaft_length
    for (int i = 0; i < SEGMENTS; ++i) {
        float a0 = tau * float(i) / float(SEGMENTS);
        float a1 = tau * float(i + 1) / float(SEGMENTS);
        float c0 = std::cos(a0) * shaft_radius;
        float s0 = std::sin(a0) * shaft_radius;
        float c1 = std::cos(a1) * shaft_radius;
        float s1 = std::sin(a1) * shaft_radius;

        // Two triangles per quad
        out.push_back(make_point(0.0f, c0, s0));
        out.push_back(make_point(shaft_length, c0, s0));
        out.push_back(make_point(shaft_length, c1, s1));

        out.push_back(make_point(0.0f, c0, s0));
        out.push_back(make_point(shaft_length, c1, s1));
        out.push_back(make_point(0.0f, c1, s1));
    }

    // Cone: from shaft_length to shaft_length + cone_length
    float tip = shaft_length + cone_length;
    for (int i = 0; i < SEGMENTS; ++i) {
        float a0 = tau * float(i) / float(SEGMENTS);
        float a1 = tau * float(i + 1) / float(SEGMENTS);
        float c0 = std::cos(a0) * cone_radius;
        float s0 = std::sin(a0) * cone_radius;
        float c1 = std::cos(a1) * cone_radius;
        float s1 = std::sin(a1) * cone_radius;

        out.push_back(make_point(shaft_length, c0, s0));
        out.push_back(make_point(tip, 0.0f, 0.0f));
        out.push_back(make_point(shaft_length, c1, s1));
    }
}

std::unique_ptr<GizmoRenderer> GizmoRenderer::create(GpuContext& ctx) {
    auto gr = std::unique_ptr<GizmoRenderer>(new GizmoRenderer(ctx));

    const std::string wgsl = read_shader_file("shaders/gizmo.wgsl");

    WGPUShaderSourceWGSL wgsl_source{};
    wgsl_source.chain.next = nullptr;
    wgsl_source.chain.sType = WGPUSType_ShaderSourceWGSL;
    wgsl_source.code = {.data = wgsl.c_str(), .length = wgsl.size()};

    WGPUShaderModuleDescriptor shader_desc{};
    shader_desc.nextInChain = &wgsl_source.chain;
    shader_desc.label = {.data = "gizmo-shader", .length = WGPU_STRLEN};

    WGPUShaderModule shader = wgpuDeviceCreateShaderModule(ctx.device(), &shader_desc);
    if (!shader) {
        throw std::runtime_error("GizmoRenderer: failed to create shader module");
    }

    // Vertex buffer (updated each frame with position offset)
    {
        WGPUBufferDescriptor desc{};
        desc.nextInChain = nullptr;
        desc.label = {.data = "gizmo-vertex-buf", .length = WGPU_STRLEN};
        desc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex;
        desc.size = VERTEX_COUNT * sizeof(GizmoVertex);
        desc.mappedAtCreation = false;
        gr->vertex_buffer_ = wgpuDeviceCreateBuffer(ctx.device(), &desc);
    }

    // Uniform buffer (view_proj mat4 = 64 bytes)
    {
        WGPUBufferDescriptor desc{};
        desc.nextInChain = nullptr;
        desc.label = {.data = "gizmo-uniform-buf", .length = WGPU_STRLEN};
        desc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform;
        desc.size = sizeof(Mat4);
        desc.mappedAtCreation = false;
        gr->uniform_buffer_ = wgpuDeviceCreateBuffer(ctx.device(), &desc);
    }

    // Vertex layout: position(vec3f) + color(vec3f) = stride 24
    WGPUVertexAttribute attribs[2]{};
    attribs[0].shaderLocation = 0;
    attribs[0].format = WGPUVertexFormat_Float32x3;
    attribs[0].offset = 0;
    attribs[1].shaderLocation = 1;
    attribs[1].format = WGPUVertexFormat_Float32x3;
    attribs[1].offset = 12;

    WGPUVertexBufferLayout vb_layout{};
    vb_layout.arrayStride = sizeof(GizmoVertex);
    vb_layout.stepMode = WGPUVertexStepMode_Vertex;
    vb_layout.attributeCount = 2;
    vb_layout.attributes = attribs;

    WGPUVertexState vertex{};
    vertex.nextInChain = nullptr;
    vertex.module = shader;
    vertex.entryPoint = {.data = "vs_main", .length = WGPU_STRLEN};
    vertex.constantCount = 0;
    vertex.constants = nullptr;
    vertex.bufferCount = 1;
    vertex.buffers = &vb_layout;

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

    // Triangle list topology (3D arrow geometry)
    WGPUPrimitiveState primitive{};
    primitive.nextInChain = nullptr;
    primitive.topology = WGPUPrimitiveTopology_TriangleList;
    primitive.stripIndexFormat = WGPUIndexFormat_Undefined;
    primitive.frontFace = WGPUFrontFace_CCW;
    primitive.cullMode = WGPUCullMode_None;

    // Depth: always on top
    WGPUDepthStencilState depth_stencil{};
    depth_stencil.nextInChain = nullptr;
    depth_stencil.format = WGPUTextureFormat_Depth24Plus;
    depth_stencil.depthWriteEnabled = WGPUOptionalBool_False;
    depth_stencil.depthCompare = WGPUCompareFunction_Always;
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
    pipeline_desc.label = {.data = "gizmo-pipeline", .length = WGPU_STRLEN};
    pipeline_desc.layout = nullptr;
    pipeline_desc.vertex = vertex;
    pipeline_desc.primitive = primitive;
    pipeline_desc.depthStencil = &depth_stencil;
    pipeline_desc.multisample = multisample;
    pipeline_desc.fragment = &fragment;

    gr->pipeline_ = wgpuDeviceCreateRenderPipeline(ctx.device(), &pipeline_desc);
    wgpuShaderModuleRelease(shader);

    if (!gr->pipeline_) {
        throw std::runtime_error("GizmoRenderer: failed to create pipeline");
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
    bg_desc.label = {.data = "gizmo-bind-group", .length = WGPU_STRLEN};
    bg_desc.layout = bgl;
    bg_desc.entryCount = 1;
    bg_desc.entries = &entry;

    gr->bind_group_ = wgpuDeviceCreateBindGroup(ctx.device(), &bg_desc);
    wgpuBindGroupLayoutRelease(bgl);

    return gr;
}

GizmoRenderer::GizmoRenderer(GpuContext& ctx) : ctx_(ctx) {}

GizmoRenderer::~GizmoRenderer() {
    if (bind_group_) {
        wgpuBindGroupRelease(bind_group_);
    }
    if (uniform_buffer_) {
        wgpuBufferRelease(uniform_buffer_);
    }
    if (vertex_buffer_) {
        wgpuBufferRelease(vertex_buffer_);
    }
    if (pipeline_) {
        wgpuRenderPipelineRelease(pipeline_);
    }
}

void GizmoRenderer::draw(const Vec3& position, const Camera& camera, WGPURenderPassEncoder pass,
                          float scale, int hovered_axis, int dragging_axis) {
    const float px = position.x;
    const float py = position.y;
    const float pz = position.z;

    std::vector<GizmoVertex> vertices;
    vertices.reserve(VERTEX_COUNT);

    const float base_colors[3][3] = {
        {0.9f, 0.2f, 0.2f},  // X red
        {0.2f, 0.9f, 0.2f},  // Y green
        {0.3f, 0.3f, 1.0f},  // Z blue
    };
    const float drag_color[3] = {1.0f, 1.0f, 0.3f};
    const float dim_factor = 0.4f;
    const float hover_boost = 0.15f;

    for (int axis = 0; axis < 3; ++axis) {
        float color[3];
        if (dragging_axis >= 0) {
            if (axis == dragging_axis) {
                color[0] = drag_color[0];
                color[1] = drag_color[1];
                color[2] = drag_color[2];
            } else {
                color[0] = base_colors[axis][0] * dim_factor;
                color[1] = base_colors[axis][1] * dim_factor;
                color[2] = base_colors[axis][2] * dim_factor;
            }
        } else if (axis == hovered_axis) {
            color[0] = std::min(base_colors[axis][0] + hover_boost, 1.0f);
            color[1] = std::min(base_colors[axis][1] + hover_boost, 1.0f);
            color[2] = std::min(base_colors[axis][2] + hover_boost, 1.0f);
        } else {
            color[0] = base_colors[axis][0];
            color[1] = base_colors[axis][1];
            color[2] = base_colors[axis][2];
        }
        build_arrow(vertices, px, py, pz, axis, color, scale);
    }

    wgpuQueueWriteBuffer(
        ctx_.queue(), vertex_buffer_, 0, vertices.data(), vertices.size() * sizeof(GizmoVertex));

    Mat4 vp = camera.view_projection_matrix();
    wgpuQueueWriteBuffer(ctx_.queue(), uniform_buffer_, 0, &vp, sizeof(Mat4));

    wgpuRenderPassEncoderSetPipeline(pass, pipeline_);
    wgpuRenderPassEncoderSetBindGroup(pass, 0, bind_group_, 0, nullptr);
    wgpuRenderPassEncoderSetVertexBuffer(pass, 0, vertex_buffer_, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderDraw(pass, static_cast<uint32_t>(vertices.size()), 1, 0, 0);
}

} // namespace gg
