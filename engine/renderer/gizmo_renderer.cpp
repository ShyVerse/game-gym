#include "renderer/gizmo_renderer.h"

#include "renderer/camera.h"
#include "renderer/gpu_context.h"

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
    std::vector<GizmoVertex>& out, float px, float py, float pz, int axis, const float color[3]) {
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

    // Shaft: cylinder from 0 to SHAFT_LENGTH
    for (int i = 0; i < SEGMENTS; ++i) {
        float a0 = tau * float(i) / float(SEGMENTS);
        float a1 = tau * float(i + 1) / float(SEGMENTS);
        float c0 = std::cos(a0) * SHAFT_RADIUS;
        float s0 = std::sin(a0) * SHAFT_RADIUS;
        float c1 = std::cos(a1) * SHAFT_RADIUS;
        float s1 = std::sin(a1) * SHAFT_RADIUS;

        // Two triangles per quad
        out.push_back(make_point(0.0f, c0, s0));
        out.push_back(make_point(SHAFT_LENGTH, c0, s0));
        out.push_back(make_point(SHAFT_LENGTH, c1, s1));

        out.push_back(make_point(0.0f, c0, s0));
        out.push_back(make_point(SHAFT_LENGTH, c1, s1));
        out.push_back(make_point(0.0f, c1, s1));
    }

    // Cone: from SHAFT_LENGTH to SHAFT_LENGTH + CONE_LENGTH
    float tip = SHAFT_LENGTH + CONE_LENGTH;
    for (int i = 0; i < SEGMENTS; ++i) {
        float a0 = tau * float(i) / float(SEGMENTS);
        float a1 = tau * float(i + 1) / float(SEGMENTS);
        float c0 = std::cos(a0) * CONE_RADIUS;
        float s0 = std::sin(a0) * CONE_RADIUS;
        float c1 = std::cos(a1) * CONE_RADIUS;
        float s1 = std::sin(a1) * CONE_RADIUS;

        out.push_back(make_point(SHAFT_LENGTH, c0, s0));
        out.push_back(make_point(tip, 0.0f, 0.0f));
        out.push_back(make_point(SHAFT_LENGTH, c1, s1));
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

void GizmoRenderer::draw(const Vec3& position, const Camera& camera, WGPURenderPassEncoder pass) {
    const float px = position.x;
    const float py = position.y;
    const float pz = position.z;

    std::vector<GizmoVertex> vertices;
    vertices.reserve(VERTEX_COUNT);

    const float red[3] = {0.9f, 0.2f, 0.2f};
    const float green[3] = {0.2f, 0.9f, 0.2f};
    const float blue[3] = {0.3f, 0.3f, 1.0f};

    build_arrow(vertices, px, py, pz, 0, red);   // X
    build_arrow(vertices, px, py, pz, 1, green); // Y
    build_arrow(vertices, px, py, pz, 2, blue);  // Z

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
