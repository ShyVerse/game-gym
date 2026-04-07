#include "compute/compute_pipeline.h"
#include "compute/gpu_buffer.h"
#include "core/window.h"
#include "renderer/gpu_context.h"

#include <cmath>
#include <cstring>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
#include <vector>

// Must match WGSL struct layout exactly
struct GpuParticle {
    float pos[4]; // xyz = position, w = radius
    float vel[4]; // xyz = velocity, w = inverse_mass
};

struct SimParams {
    float dt;
    float gravity;
    uint32_t count;
    uint32_t grid_res;
    float bounds_min[3];
    float _pad0;
    float bounds_max[3];
    float damping;
    float cell_size;
    float _pad1;
    float _pad2;
    float _pad3;
};
static_assert(sizeof(SimParams) == 64, "SimParams must be 64 bytes");

namespace {

std::unique_ptr<gg::Window> make_window() {
    return gg::Window::create({.title = "particle-test", .width = 64, .height = 64});
}

std::string read_file(const char* path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        throw std::runtime_error(std::string("Cannot open: ") + path);
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

SimParams make_default_params(uint32_t count) {
    SimParams p{};
    p.dt = 1.0f / 60.0f;
    p.gravity = -9.81f;
    p.count = count;
    p.grid_res = 64;
    p.bounds_min[0] = -10.0f;
    p.bounds_min[1] = 0.0f;
    p.bounds_min[2] = -10.0f;
    p.bounds_max[0] = 10.0f;
    p.bounds_max[1] = 20.0f;
    p.bounds_max[2] = 10.0f;
    p.damping = 0.8f;
    p.cell_size = (20.0f) / 64.0f;
    return p;
}

uint32_t workgroups(uint32_t count) {
    return (count + 63) / 64;
}

} // namespace

// ---------------------------------------------------------------------------
// Pass 1: Integration + Boundary
// ---------------------------------------------------------------------------

TEST(GpuParticleTest, GravityFall) {
    auto window = make_window();
    auto ctx = gg::GpuContext::create(*window);
    auto shader = read_file("shaders/particle_integrate.wgsl");

    constexpr uint32_t kCount = 1;
    GpuParticle p{};
    p.pos[0] = 0.0f;
    p.pos[1] = 10.0f;
    p.pos[2] = 0.0f;
    p.pos[3] = 0.05f; // radius
    p.vel[0] = 0.0f;
    p.vel[1] = 0.0f;
    p.vel[2] = 0.0f;
    p.vel[3] = 1.0f; // inverse_mass

    auto params = make_default_params(kCount);

    gg::GpuBuffer pbuf = gg::GpuBuffer::create_storage(ctx->device(), sizeof(GpuParticle));
    pbuf.upload(ctx->queue(), &p, sizeof(GpuParticle));
    gg::GpuBuffer ubuf = gg::GpuBuffer::create_uniform(ctx->device(), sizeof(SimParams));
    ubuf.upload(ctx->queue(), &params, sizeof(SimParams));

    auto pipeline = gg::ComputePipeline::create(ctx->device(), shader);

    // Run 10 steps
    for (int i = 0; i < 10; ++i) {
        pipeline->dispatch(ctx->device(), ctx->queue(),
                           {pbuf.handle(), ubuf.handle()}, workgroups(kCount));
    }

    auto result = pbuf.readback(ctx->device(), ctx->queue());
    const auto* out = reinterpret_cast<const GpuParticle*>(result.data());

    // After 10 frames of gravity, y should decrease
    EXPECT_LT(out->pos[1], 10.0f) << "Particle should fall under gravity";
    EXPECT_LT(out->vel[1], 0.0f) << "Velocity should be negative (falling)";
}

TEST(GpuParticleTest, BoundaryBounce) {
    auto window = make_window();
    auto ctx = gg::GpuContext::create(*window);
    auto shader = read_file("shaders/particle_integrate.wgsl");

    constexpr uint32_t kCount = 1;
    GpuParticle p{};
    p.pos[0] = 0.0f;
    p.pos[1] = 0.1f; // near floor (bounds_min.y = 0)
    p.pos[2] = 0.0f;
    p.pos[3] = 0.05f;
    p.vel[0] = 0.0f;
    p.vel[1] = -50.0f; // fast downward
    p.vel[2] = 0.0f;
    p.vel[3] = 1.0f;

    auto params = make_default_params(kCount);
    params.gravity = 0.0f; // disable gravity to isolate bounce

    gg::GpuBuffer pbuf = gg::GpuBuffer::create_storage(ctx->device(), sizeof(GpuParticle));
    pbuf.upload(ctx->queue(), &p, sizeof(GpuParticle));
    gg::GpuBuffer ubuf = gg::GpuBuffer::create_uniform(ctx->device(), sizeof(SimParams));
    ubuf.upload(ctx->queue(), &params, sizeof(SimParams));

    auto pipeline = gg::ComputePipeline::create(ctx->device(), shader);
    pipeline->dispatch(ctx->device(), ctx->queue(),
                       {pbuf.handle(), ubuf.handle()}, workgroups(kCount));

    auto result = pbuf.readback(ctx->device(), ctx->queue());
    const auto* out = reinterpret_cast<const GpuParticle*>(result.data());

    // Should have bounced: pos.y >= bounds_min + radius, vel.y > 0
    EXPECT_GE(out->pos[1], 0.05f) << "Particle must stay inside boundary";
    EXPECT_GT(out->vel[1], 0.0f) << "Velocity should flip after bounce";
}

TEST(GpuParticleTest, WallBounce) {
    auto window = make_window();
    auto ctx = gg::GpuContext::create(*window);
    auto shader = read_file("shaders/particle_integrate.wgsl");

    constexpr uint32_t kCount = 1;
    GpuParticle p{};
    p.pos[0] = 9.9f; // near right wall (bounds_max.x = 10)
    p.pos[1] = 5.0f;
    p.pos[2] = 0.0f;
    p.pos[3] = 0.05f;
    p.vel[0] = 50.0f; // fast rightward
    p.vel[1] = 0.0f;
    p.vel[2] = 0.0f;
    p.vel[3] = 1.0f;

    auto params = make_default_params(kCount);
    params.gravity = 0.0f;

    gg::GpuBuffer pbuf = gg::GpuBuffer::create_storage(ctx->device(), sizeof(GpuParticle));
    pbuf.upload(ctx->queue(), &p, sizeof(GpuParticle));
    gg::GpuBuffer ubuf = gg::GpuBuffer::create_uniform(ctx->device(), sizeof(SimParams));
    ubuf.upload(ctx->queue(), &params, sizeof(SimParams));

    auto pipeline = gg::ComputePipeline::create(ctx->device(), shader);
    pipeline->dispatch(ctx->device(), ctx->queue(),
                       {pbuf.handle(), ubuf.handle()}, workgroups(kCount));

    auto result = pbuf.readback(ctx->device(), ctx->queue());
    const auto* out = reinterpret_cast<const GpuParticle*>(result.data());

    EXPECT_LE(out->pos[0], 10.0f - 0.05f) << "Particle must stay inside wall";
    EXPECT_LT(out->vel[0], 0.0f) << "Velocity should flip after wall bounce";
}
