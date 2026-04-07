#include "compute/compute_pipeline.h"
#include "compute/gpu_buffer.h"
#include "compute/particle_system.h"
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
        pipeline->dispatch(
            ctx->device(), ctx->queue(), {pbuf.handle(), ubuf.handle()}, workgroups(kCount));
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
    pipeline->dispatch(
        ctx->device(), ctx->queue(), {pbuf.handle(), ubuf.handle()}, workgroups(kCount));

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
    pipeline->dispatch(
        ctx->device(), ctx->queue(), {pbuf.handle(), ubuf.handle()}, workgroups(kCount));

    auto result = pbuf.readback(ctx->device(), ctx->queue());
    const auto* out = reinterpret_cast<const GpuParticle*>(result.data());

    EXPECT_LE(out->pos[0], 10.0f - 0.05f) << "Particle must stay inside wall";
    EXPECT_LT(out->vel[0], 0.0f) << "Velocity should flip after wall bounce";
}

// ---------------------------------------------------------------------------
// Pass 2: Grid Build
// ---------------------------------------------------------------------------

TEST(GpuParticleTest, GridBuildCorrectness) {
    auto window = make_window();
    auto ctx = gg::GpuContext::create(*window);

    auto clear_shader = read_file("shaders/particle_grid_build.wgsl");
    auto insert_shader = clear_shader; // same file, different entry points

    constexpr uint32_t kCount = 4;
    constexpr uint32_t kGridRes = 4;
    constexpr uint32_t kTotalCells = kGridRes * kGridRes * kGridRes; // 64
    constexpr uint32_t kMaxPerCell = 8;

    // Place 4 particles in known positions within a small grid
    std::vector<GpuParticle> particles(kCount);
    // All in cell (0,0,0)
    for (auto& p : particles) {
        p.pos[0] = -9.0f;
        p.pos[1] = 0.5f;
        p.pos[2] = -9.0f;
        p.pos[3] = 0.05f;
        p.vel[0] = 0.0f;
        p.vel[1] = 0.0f;
        p.vel[2] = 0.0f;
        p.vel[3] = 1.0f;
    }

    SimParams params{};
    params.count = kCount;
    params.grid_res = kGridRes;
    params.bounds_min[0] = -10.0f;
    params.bounds_min[1] = 0.0f;
    params.bounds_min[2] = -10.0f;
    params.bounds_max[0] = 10.0f;
    params.bounds_max[1] = 20.0f;
    params.bounds_max[2] = 10.0f;
    params.cell_size = 20.0f / static_cast<float>(kGridRes); // 5.0

    uint64_t pbytes = kCount * sizeof(GpuParticle);
    gg::GpuBuffer pbuf = gg::GpuBuffer::create_storage(ctx->device(), pbytes);
    pbuf.upload(ctx->queue(), particles.data(), pbytes);

    gg::GpuBuffer ubuf = gg::GpuBuffer::create_uniform(ctx->device(), sizeof(SimParams));
    ubuf.upload(ctx->queue(), &params, sizeof(SimParams));

    uint64_t counts_bytes = kTotalCells * sizeof(uint32_t);
    gg::GpuBuffer counts_buf = gg::GpuBuffer::create_storage(ctx->device(), counts_bytes);

    uint64_t entries_bytes = kTotalCells * kMaxPerCell * sizeof(uint32_t);
    gg::GpuBuffer entries_buf = gg::GpuBuffer::create_storage(ctx->device(), entries_bytes);

    // Clear pass
    auto clear_pipeline = gg::ComputePipeline::create(ctx->device(), clear_shader, "clear");
    clear_pipeline->dispatch(
        ctx->device(),
        ctx->queue(),
        {pbuf.handle(), ubuf.handle(), counts_buf.handle(), entries_buf.handle()},
        workgroups(kTotalCells));

    // Insert pass
    auto insert_pipeline = gg::ComputePipeline::create(ctx->device(), insert_shader, "insert");
    insert_pipeline->dispatch(
        ctx->device(),
        ctx->queue(),
        {pbuf.handle(), ubuf.handle(), counts_buf.handle(), entries_buf.handle()},
        workgroups(kCount));

    // Readback counts
    auto counts_data = counts_buf.readback(ctx->device(), ctx->queue());
    const auto* counts = reinterpret_cast<const uint32_t*>(counts_data.data());

    // Cell (0,0,0) = index 0 should have 4 particles
    // pos (-9, 0.5, -9) → rel = (1, 0.5, 1)/5 = (0.2, 0.1, 0.2) → cell (0,0,0) = 0
    EXPECT_EQ(counts[0], 4u) << "Cell (0,0,0) should contain all 4 particles";

    // All other cells should be 0
    for (uint32_t i = 1; i < kTotalCells; ++i) {
        EXPECT_EQ(counts[i], 0u) << "Cell " << i << " should be empty";
    }
}

// ---------------------------------------------------------------------------
// Pass 3: Particle Collision
// ---------------------------------------------------------------------------

TEST(GpuParticleTest, TwoParticleCollision) {
    auto window = make_window();
    auto ctx = gg::GpuContext::create(*window);

    auto integrate_src = read_file("shaders/particle_integrate.wgsl");
    auto grid_src = read_file("shaders/particle_grid_build.wgsl");
    auto collide_src = read_file("shaders/particle_collide.wgsl");

    constexpr uint32_t kCount = 2;
    constexpr uint32_t kGridRes = 4;
    constexpr uint32_t kTotalCells = kGridRes * kGridRes * kGridRes;
    constexpr uint32_t kMaxPerCell = 8;

    // Two particles approaching each other head-on
    std::vector<GpuParticle> particles(kCount);
    // Particle 0: moving right
    particles[0].pos[0] = -0.04f;
    particles[0].pos[1] = 5.0f;
    particles[0].pos[2] = 0.0f;
    particles[0].pos[3] = 0.05f; // radius
    particles[0].vel[0] = 2.0f;
    particles[0].vel[1] = 0.0f;
    particles[0].vel[2] = 0.0f;
    particles[0].vel[3] = 1.0f;

    // Particle 1: moving left
    particles[1].pos[0] = 0.04f;
    particles[1].pos[1] = 5.0f;
    particles[1].pos[2] = 0.0f;
    particles[1].pos[3] = 0.05f;
    particles[1].vel[0] = -2.0f;
    particles[1].vel[1] = 0.0f;
    particles[1].vel[2] = 0.0f;
    particles[1].vel[3] = 1.0f;

    SimParams params = make_default_params(kCount);
    params.gravity = 0.0f; // disable gravity
    params.grid_res = kGridRes;
    params.cell_size = 20.0f / static_cast<float>(kGridRes);

    uint64_t pbytes = kCount * sizeof(GpuParticle);
    gg::GpuBuffer pbuf = gg::GpuBuffer::create_storage(ctx->device(), pbytes);
    pbuf.upload(ctx->queue(), particles.data(), pbytes);

    gg::GpuBuffer ubuf = gg::GpuBuffer::create_uniform(ctx->device(), sizeof(SimParams));
    ubuf.upload(ctx->queue(), &params, sizeof(SimParams));

    uint64_t counts_bytes = kTotalCells * sizeof(uint32_t);
    gg::GpuBuffer counts_buf = gg::GpuBuffer::create_storage(ctx->device(), counts_bytes);

    uint64_t entries_bytes = kTotalCells * kMaxPerCell * sizeof(uint32_t);
    gg::GpuBuffer entries_buf = gg::GpuBuffer::create_storage(ctx->device(), entries_bytes);

    auto integrate_pipe = gg::ComputePipeline::create(ctx->device(), integrate_src);
    auto clear_pipe = gg::ComputePipeline::create(ctx->device(), grid_src, "clear");
    auto insert_pipe = gg::ComputePipeline::create(ctx->device(), grid_src, "insert");
    auto collide_pipe = gg::ComputePipeline::create(ctx->device(), collide_src);

    // Run full 3-pass pipeline
    integrate_pipe->dispatch(
        ctx->device(), ctx->queue(), {pbuf.handle(), ubuf.handle()}, workgroups(kCount));
    clear_pipe->dispatch(ctx->device(),
                         ctx->queue(),
                         {pbuf.handle(), ubuf.handle(), counts_buf.handle(), entries_buf.handle()},
                         workgroups(kTotalCells));
    insert_pipe->dispatch(ctx->device(),
                          ctx->queue(),
                          {pbuf.handle(), ubuf.handle(), counts_buf.handle(), entries_buf.handle()},
                          workgroups(kCount));
    collide_pipe->dispatch(
        ctx->device(),
        ctx->queue(),
        {pbuf.handle(), ubuf.handle(), counts_buf.handle(), entries_buf.handle()},
        workgroups(kCount));

    auto result = pbuf.readback(ctx->device(), ctx->queue());
    const auto* out = reinterpret_cast<const GpuParticle*>(result.data());

    // After collision: particles should be pushed apart (separation >= combined radius)
    float separation = std::abs(out[0].pos[0] - out[1].pos[0]);
    EXPECT_GE(separation, 0.09f) << "Particles should not overlap after collision";

    // Particle 0 velocity should decrease after collision
    EXPECT_LT(out[0].vel[0], 2.0f) << "Particle 0 velocity should decrease after collision";
}

// ---------------------------------------------------------------------------
// ParticleSystem Integration Tests
// ---------------------------------------------------------------------------

TEST(ParticleSystemTest, CreateAndSpawn) {
    auto window = make_window();
    auto ctx = gg::GpuContext::create(*window);

    auto sys = gg::ParticleSystem::create(*ctx, {.max_particles = 1000});
    ASSERT_NE(sys, nullptr);
    EXPECT_EQ(sys->count(), 0u);

    sys->spawn(100, {0, 5, 0}, {1, 1, 1}, {1, 1, 1});
    EXPECT_EQ(sys->count(), 100u);

    auto data = sys->readback();
    ASSERT_EQ(data.size(), 100u);
}

TEST(ParticleSystemTest, StepAppliesGravity) {
    auto window = make_window();
    auto ctx = gg::GpuContext::create(*window);

    auto sys = gg::ParticleSystem::create(*ctx,
                                          {
                                              .max_particles = 1,
                                              .gravity = -9.81f,
                                              .grid_resolution = 4,
                                          });

    // Manually spawn one particle at known position
    sys->spawn(1, {0, 10, 0}, {0, 0, 0}, {0, 0, 0});

    for (int i = 0; i < 10; ++i) {
        sys->step(1.0f / 60.0f);
    }

    auto data = sys->readback();
    ASSERT_EQ(data.size(), 1u);
    EXPECT_LT(data[0].pos[1], 10.0f) << "Particle should fall under gravity";
}

TEST(ParticleSystemTest, LargeScaleStability) {
    auto window = make_window();
    auto ctx = gg::GpuContext::create(*window);

    auto sys = gg::ParticleSystem::create(*ctx,
                                          {
                                              .max_particles = 10000,
                                              .grid_resolution = 16,
                                          });

    sys->spawn(10000, {0, 10, 0}, {5, 5, 5}, {2, 2, 2});

    // Run 60 frames
    for (int i = 0; i < 60; ++i) {
        sys->step(1.0f / 60.0f);
    }

    auto data = sys->readback();
    ASSERT_EQ(data.size(), 10000u);

    // Check no NaN/Inf
    for (size_t i = 0; i < data.size(); ++i) {
        EXPECT_FALSE(std::isnan(data[i].pos[0])) << "NaN at particle " << i;
        EXPECT_FALSE(std::isnan(data[i].pos[1])) << "NaN at particle " << i;
        EXPECT_FALSE(std::isnan(data[i].pos[2])) << "NaN at particle " << i;
        EXPECT_FALSE(std::isinf(data[i].pos[0])) << "Inf at particle " << i;
        EXPECT_FALSE(std::isinf(data[i].pos[1])) << "Inf at particle " << i;
        EXPECT_FALSE(std::isinf(data[i].pos[2])) << "Inf at particle " << i;
    }

    // All particles should be within bounds (with small tolerance)
    for (size_t i = 0; i < data.size(); ++i) {
        EXPECT_GE(data[i].pos[0], -10.5f) << "Out of bounds particle " << i;
        EXPECT_LE(data[i].pos[0], 10.5f) << "Out of bounds particle " << i;
        EXPECT_GE(data[i].pos[1], -0.5f) << "Out of bounds particle " << i;
        EXPECT_LE(data[i].pos[1], 20.5f) << "Out of bounds particle " << i;
    }
}
