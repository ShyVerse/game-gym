#pragma once
#include "math/vec3.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace gg {

class GpuContext;

struct ParticleData {
    float pos[4]; // xyz = position, w = radius
    float vel[4]; // xyz = velocity, w = inverse_mass
};

struct ParticleSystemConfig {
    uint32_t max_particles = 100000;
    float gravity = -9.81f;
    float damping = 0.8f;
    float particle_radius = 0.05f;
    Vec3 bounds_min = {-10.0f, 0.0f, -10.0f};
    Vec3 bounds_max = {10.0f, 20.0f, 10.0f};
    uint32_t grid_resolution = 64;
};

class ParticleSystem {
public:
    static std::unique_ptr<ParticleSystem> create(GpuContext& gpu,
                                                  const ParticleSystemConfig& config = {});
    ~ParticleSystem();

    ParticleSystem(const ParticleSystem&) = delete;
    ParticleSystem& operator=(const ParticleSystem&) = delete;

    /// Spawn particles at center with random spread
    void spawn(uint32_t count, Vec3 center, Vec3 extent, Vec3 velocity_range);

    /// Run one simulation step (3 compute passes)
    void step(float dt);

    /// Read particle data back to CPU (synchronous, for testing)
    [[nodiscard]] std::vector<ParticleData> readback();

    /// Current active particle count
    [[nodiscard]] uint32_t count() const;

private:
    ParticleSystem() = default;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace gg
