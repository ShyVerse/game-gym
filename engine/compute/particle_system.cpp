#include "compute/particle_system.h"

#include "compute/compute_pipeline.h"
#include "compute/gpu_buffer.h"
#include "renderer/gpu_context.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <optional>
#include <random>
#include <sstream>
#include <stdexcept>

namespace gg {

namespace {

std::string load_shader(const char* path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        throw std::runtime_error(std::string("Cannot open shader: ") + path);
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

struct GpuSimParams {
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
static_assert(sizeof(GpuSimParams) == 64, "GpuSimParams must be 64 bytes");

constexpr uint32_t WORKGROUP_SIZE = 64;
constexpr uint32_t MAX_PER_CELL = 8;

uint32_t div_ceil(uint32_t n, uint32_t d) {
    return (n + d - 1) / d;
}

} // namespace

struct ParticleSystem::Impl {
    GpuContext* gpu = nullptr;
    ParticleSystemConfig config;
    uint32_t active_count = 0;

    // GPU buffers — optional because GpuBuffer has no default constructor
    std::optional<GpuBuffer> particle_buf;
    std::optional<GpuBuffer> params_buf;
    std::optional<GpuBuffer> grid_counts_buf;
    std::optional<GpuBuffer> grid_entries_buf;

    // Pipelines
    std::unique_ptr<ComputePipeline> integrate_pipe;
    std::unique_ptr<ComputePipeline> grid_clear_pipe;
    std::unique_ptr<ComputePipeline> grid_insert_pipe;
    std::unique_ptr<ComputePipeline> collide_pipe;
};

ParticleSystem::~ParticleSystem() = default;

std::unique_ptr<ParticleSystem> ParticleSystem::create(GpuContext& gpu,
                                                       const ParticleSystemConfig& config) {
    auto sys = std::unique_ptr<ParticleSystem>(new ParticleSystem());
    sys->impl_ = std::make_unique<Impl>();
    auto& impl = *sys->impl_;
    impl.gpu = &gpu;
    impl.config = config;

    auto device = gpu.device();

    // Create buffers
    uint64_t particle_bytes = config.max_particles * sizeof(ParticleData);
    impl.particle_buf.emplace(GpuBuffer::create_storage(device, particle_bytes));
    impl.params_buf.emplace(GpuBuffer::create_uniform(device, sizeof(GpuSimParams)));

    uint32_t total_cells = config.grid_resolution * config.grid_resolution * config.grid_resolution;
    impl.grid_counts_buf.emplace(GpuBuffer::create_storage(device, total_cells * sizeof(uint32_t)));
    impl.grid_entries_buf.emplace(
        GpuBuffer::create_storage(device, total_cells * MAX_PER_CELL * sizeof(uint32_t)));

    // Create pipelines
    auto integrate_src = load_shader("shaders/particle_integrate.wgsl");
    auto grid_src = load_shader("shaders/particle_grid_build.wgsl");
    auto collide_src = load_shader("shaders/particle_collide.wgsl");

    impl.integrate_pipe = ComputePipeline::create(device, integrate_src);
    impl.grid_clear_pipe = ComputePipeline::create(device, grid_src, "clear");
    impl.grid_insert_pipe = ComputePipeline::create(device, grid_src, "insert");
    impl.collide_pipe = ComputePipeline::create(device, collide_src);

    return sys;
}

void ParticleSystem::spawn(uint32_t count, Vec3 center, Vec3 extent, Vec3 velocity_range) {
    auto& impl = *impl_;
    uint32_t start = impl.active_count;
    uint32_t to_add = std::min(count, impl.config.max_particles - start);
    if (to_add == 0) {
        return;
    }

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

    std::vector<ParticleData> particles(to_add);
    for (auto& p : particles) {
        p.pos[0] = center.x + dist(rng) * extent.x;
        p.pos[1] = center.y + dist(rng) * extent.y;
        p.pos[2] = center.z + dist(rng) * extent.z;
        p.pos[3] = impl.config.particle_radius;
        p.vel[0] = dist(rng) * velocity_range.x;
        p.vel[1] = dist(rng) * velocity_range.y;
        p.vel[2] = dist(rng) * velocity_range.z;
        p.vel[3] = 1.0f; // inverse_mass
    }

    uint64_t offset = start * sizeof(ParticleData);
    impl.particle_buf->upload(
        impl.gpu->queue(), particles.data(), to_add * sizeof(ParticleData), offset);
    impl.active_count = start + to_add;
}

void ParticleSystem::step(float dt) {
    auto& impl = *impl_;
    if (impl.active_count == 0) {
        return;
    }

    auto device = impl.gpu->device();
    auto queue = impl.gpu->queue();

    // Upload params
    auto& cfg = impl.config;
    float cell_size =
        (cfg.bounds_max.x - cfg.bounds_min.x) / static_cast<float>(cfg.grid_resolution);
    GpuSimParams params{};
    params.dt = dt;
    params.gravity = cfg.gravity;
    params.count = impl.active_count;
    params.grid_res = cfg.grid_resolution;
    params.bounds_min[0] = cfg.bounds_min.x;
    params.bounds_min[1] = cfg.bounds_min.y;
    params.bounds_min[2] = cfg.bounds_min.z;
    params.bounds_max[0] = cfg.bounds_max.x;
    params.bounds_max[1] = cfg.bounds_max.y;
    params.bounds_max[2] = cfg.bounds_max.z;
    params.damping = cfg.damping;
    params.cell_size = cell_size;
    impl.params_buf->upload(queue, &params, sizeof(GpuSimParams));

    uint32_t particle_wg = div_ceil(impl.active_count, WORKGROUP_SIZE);
    uint32_t total_cells = cfg.grid_resolution * cfg.grid_resolution * cfg.grid_resolution;
    uint32_t grid_wg = div_ceil(total_cells, WORKGROUP_SIZE);

    // Pass 1: Integrate + boundary
    impl.integrate_pipe->dispatch(
        device, queue, {impl.particle_buf->handle(), impl.params_buf->handle()}, particle_wg);

    // Pass 2: Grid clear + insert
    impl.grid_clear_pipe->dispatch(device,
                                   queue,
                                   {impl.particle_buf->handle(),
                                    impl.params_buf->handle(),
                                    impl.grid_counts_buf->handle(),
                                    impl.grid_entries_buf->handle()},
                                   grid_wg);

    impl.grid_insert_pipe->dispatch(device,
                                    queue,
                                    {impl.particle_buf->handle(),
                                     impl.params_buf->handle(),
                                     impl.grid_counts_buf->handle(),
                                     impl.grid_entries_buf->handle()},
                                    particle_wg);

    // Pass 3: Collide
    impl.collide_pipe->dispatch(device,
                                queue,
                                {impl.particle_buf->handle(),
                                 impl.params_buf->handle(),
                                 impl.grid_counts_buf->handle(),
                                 impl.grid_entries_buf->handle()},
                                particle_wg);
}

std::vector<ParticleData> ParticleSystem::readback() {
    auto& impl = *impl_;
    if (impl.active_count == 0) {
        return {};
    }

    auto raw = impl.particle_buf->readback(impl.gpu->device(), impl.gpu->queue());
    uint32_t count = impl.active_count;
    std::vector<ParticleData> result(count);
    std::memcpy(result.data(), raw.data(), count * sizeof(ParticleData));
    return result;
}

uint32_t ParticleSystem::count() const {
    return impl_ ? impl_->active_count : 0;
}

} // namespace gg
