#include "compute/compute_pipeline.h"
#include "compute/gpu_buffer.h"
#include "core/window.h"
#include "renderer/gpu_context.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
#include <stdexcept>

// ---------------------------------------------------------------------------
// Helper
// ---------------------------------------------------------------------------

static std::unique_ptr<gg::Window> make_window() {
    return gg::Window::create({.title = "compute-test", .width = 64, .height = 64});
}

static std::string read_file(const char* path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        throw std::runtime_error(std::string("Cannot open file: ") + path);
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// GPU particle layout matching particle_sim.wgsl
struct GpuParticle {
    float pos[4]; // vec4f
    float vel[4]; // vec4f
};

struct GpuParams {
    float dt;
    uint32_t count;
    uint32_t _pad0;
    uint32_t _pad1;
};

// ---------------------------------------------------------------------------
// GpuBuffer tests
// ---------------------------------------------------------------------------

TEST(GpuBufferTest, CreateStorage) {
    auto window = make_window();
    auto ctx = gg::GpuContext::create(*window);

    gg::GpuBuffer buf = gg::GpuBuffer::create_storage(ctx->device(), 256);
    EXPECT_NE(buf.handle(), nullptr);
    EXPECT_EQ(buf.size(), 256u);
}

TEST(GpuBufferTest, CreateUniform) {
    auto window = make_window();
    auto ctx = gg::GpuContext::create(*window);

    gg::GpuBuffer buf = gg::GpuBuffer::create_uniform(ctx->device(), 64);
    EXPECT_NE(buf.handle(), nullptr);
    EXPECT_EQ(buf.size(), 64u);
}

TEST(GpuBufferTest, UploadAndReadback) {
    auto window = make_window();
    auto ctx = gg::GpuContext::create(*window);

    constexpr uint64_t kSize = 64;
    uint8_t src[kSize];
    for (uint64_t i = 0; i < kSize; ++i) {
        src[i] = static_cast<uint8_t>(i);
    }

    gg::GpuBuffer buf = gg::GpuBuffer::create_storage(ctx->device(), kSize);
    buf.upload(ctx->queue(), src, kSize);

    auto result = buf.readback(ctx->device(), ctx->queue());

    ASSERT_EQ(result.size(), kSize);
    EXPECT_EQ(std::memcmp(result.data(), src, kSize), 0);
}

TEST(GpuBufferTest, MoveSemantics) {
    auto window = make_window();
    auto ctx = gg::GpuContext::create(*window);

    gg::GpuBuffer buf1 = gg::GpuBuffer::create_storage(ctx->device(), 128);
    WGPUBuffer raw = buf1.handle();

    gg::GpuBuffer buf2 = std::move(buf1);

    // buf1 should be empty after move
    EXPECT_EQ(buf1.handle(), nullptr);
    EXPECT_EQ(buf1.size(), 0u);

    // buf2 should own the original handle
    EXPECT_EQ(buf2.handle(), raw);
    EXPECT_EQ(buf2.size(), 128u);
}

// ---------------------------------------------------------------------------
// ComputePipeline tests
// ---------------------------------------------------------------------------

// A trivial WGSL shader that doubles each float in a storage buffer.
static const char* kDoubleShader = R"(
@group(0) @binding(0) var<storage, read_write> data: array<f32>;

@compute @workgroup_size(64)
fn main(@builtin(global_invocation_id) gid: vec3u) {
    let idx = gid.x;
    if (idx >= arrayLength(&data)) { return; }
    data[idx] = data[idx] * 2.0;
}
)";

TEST(ComputePipelineTest, CreatesFromWGSL) {
    auto window = make_window();
    auto ctx = gg::GpuContext::create(*window);

    auto pipeline = gg::ComputePipeline::create(ctx->device(), kDoubleShader);
    EXPECT_NE(pipeline, nullptr);
}

TEST(ComputePipelineTest, DispatchDoublesValues) {
    auto window = make_window();
    auto ctx = gg::GpuContext::create(*window);

    constexpr int kCount = 64;
    float src[kCount];
    for (int i = 0; i < kCount; ++i) {
        src[i] = static_cast<float>(i);
    }

    gg::GpuBuffer buf = gg::GpuBuffer::create_storage(ctx->device(), kCount * sizeof(float));
    buf.upload(ctx->queue(), src, kCount * sizeof(float));

    auto pipeline = gg::ComputePipeline::create(ctx->device(), kDoubleShader);
    pipeline->dispatch(ctx->device(),
                       ctx->queue(),
                       {buf.handle()},
                       /* workgroups_x */ 1);

    auto result = buf.readback(ctx->device(), ctx->queue());
    ASSERT_EQ(result.size(), kCount * sizeof(float));

    const float* out = reinterpret_cast<const float*>(result.data());
    for (int i = 0; i < kCount; ++i) {
        EXPECT_FLOAT_EQ(out[i], src[i] * 2.0f) << "at index " << i;
    }
}

// ---------------------------------------------------------------------------
// ParticleSim tests
// ---------------------------------------------------------------------------

TEST(ParticleSimTest, IntegratesVelocity) {
    auto window = make_window();
    auto ctx = gg::GpuContext::create(*window);

    std::string shader_source = read_file("shaders/particle_sim.wgsl");

    constexpr uint32_t kCount = 128;

    // Prepare particle data: pos=(0,0,0,0), vel=(1,2,3,0)
    std::vector<GpuParticle> particles(kCount);
    for (auto& p : particles) {
        p.pos[0] = 0.0f;
        p.pos[1] = 0.0f;
        p.pos[2] = 0.0f;
        p.pos[3] = 0.0f;
        p.vel[0] = 1.0f;
        p.vel[1] = 2.0f;
        p.vel[2] = 3.0f;
        p.vel[3] = 0.0f;
    }

    GpuParams params{};
    params.dt = 1.0f;
    params.count = kCount;
    params._pad0 = 0;
    params._pad1 = 0;

    uint64_t particle_bytes = kCount * sizeof(GpuParticle);
    gg::GpuBuffer particle_buf = gg::GpuBuffer::create_storage(ctx->device(), particle_bytes);
    particle_buf.upload(ctx->queue(), particles.data(), particle_bytes);

    gg::GpuBuffer param_buf = gg::GpuBuffer::create_uniform(ctx->device(), sizeof(GpuParams));
    param_buf.upload(ctx->queue(), &params, sizeof(GpuParams));

    auto pipeline = gg::ComputePipeline::create(ctx->device(), shader_source);

    // One step: workgroup_size=64, so ceil(128/64)=2 workgroups
    pipeline->dispatch(ctx->device(),
                       ctx->queue(),
                       {particle_buf.handle(), param_buf.handle()},
                       /* workgroups_x */ 2);

    auto result = particle_buf.readback(ctx->device(), ctx->queue());
    ASSERT_EQ(result.size(), particle_bytes);

    const GpuParticle* out = reinterpret_cast<const GpuParticle*>(result.data());

    for (uint32_t i = 0; i < kCount; ++i) {
        EXPECT_FLOAT_EQ(out[i].pos[0], 1.0f) << "particle " << i << " pos.x";
        EXPECT_FLOAT_EQ(out[i].pos[1], 2.0f) << "particle " << i << " pos.y";
        EXPECT_FLOAT_EQ(out[i].pos[2], 3.0f) << "particle " << i << " pos.z";
    }
}

TEST(ParticleSimTest, MultipleSteps) {
    auto window = make_window();
    auto ctx = gg::GpuContext::create(*window);

    std::string shader_source = read_file("shaders/particle_sim.wgsl");

    constexpr uint32_t kCount = 64;
    constexpr int kSteps = 60;
    constexpr float kDt = 1.0f / 60.0f;

    // All particles start at origin with vel=(1,0,0,0)
    std::vector<GpuParticle> particles(kCount);
    for (auto& p : particles) {
        p.pos[0] = 0.0f;
        p.pos[1] = 0.0f;
        p.pos[2] = 0.0f;
        p.pos[3] = 0.0f;
        p.vel[0] = 1.0f;
        p.vel[1] = 0.0f;
        p.vel[2] = 0.0f;
        p.vel[3] = 0.0f;
    }

    GpuParams params{};
    params.dt = kDt;
    params.count = kCount;
    params._pad0 = 0;
    params._pad1 = 0;

    uint64_t particle_bytes = kCount * sizeof(GpuParticle);
    gg::GpuBuffer particle_buf = gg::GpuBuffer::create_storage(ctx->device(), particle_bytes);
    particle_buf.upload(ctx->queue(), particles.data(), particle_bytes);

    gg::GpuBuffer param_buf = gg::GpuBuffer::create_uniform(ctx->device(), sizeof(GpuParams));
    param_buf.upload(ctx->queue(), &params, sizeof(GpuParams));

    auto pipeline = gg::ComputePipeline::create(ctx->device(), shader_source);

    // Run 60 steps
    for (int step = 0; step < kSteps; ++step) {
        pipeline->dispatch(ctx->device(),
                           ctx->queue(),
                           {particle_buf.handle(), param_buf.handle()},
                           /* workgroups_x */ 1);
    }

    auto result = particle_buf.readback(ctx->device(), ctx->queue());
    ASSERT_EQ(result.size(), particle_bytes);

    const GpuParticle* out = reinterpret_cast<const GpuParticle*>(result.data());

    // After 60 steps at dt=1/60, pos.x should be ≈1.0
    const float expected_x = 1.0f * kDt * kSteps; // = 1.0f
    for (uint32_t i = 0; i < kCount; ++i) {
        EXPECT_NEAR(out[i].pos[0], expected_x, 1e-4f) << "particle " << i;
        EXPECT_FLOAT_EQ(out[i].pos[1], 0.0f) << "particle " << i;
        EXPECT_FLOAT_EQ(out[i].pos[2], 0.0f) << "particle " << i;
    }
}
