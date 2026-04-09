#pragma once
#include "math/mat4.h"

#include <cstdint>
#include <memory>
#include <numbers>

namespace gg {

enum class CameraMode : uint8_t { Orbit, Fly };

class Camera {
public:
    static std::unique_ptr<Camera> create();
    ~Camera() = default;
    Camera(const Camera&) = delete;
    Camera& operator=(const Camera&) = delete;

    void orbit(float dx, float dy);
    void look(float dx, float dy);
    void zoom(float delta);
    void set_fly_mode(bool enabled);
    void move_local(float right, float up, float forward, float dt);
    void set_aspect(float aspect);
    [[nodiscard]] bool is_fly_mode() const;

    [[nodiscard]] Mat4 view_matrix() const;
    [[nodiscard]] Mat4 projection_matrix() const;
    [[nodiscard]] Mat4 view_projection_matrix() const;
    [[nodiscard]] Vec3 eye_position() const;
    [[nodiscard]] float fov() const;

private:
    Camera() = default;
    [[nodiscard]] Vec3 forward_direction() const;
    [[nodiscard]] Vec3 right_direction() const;

    float yaw_ = 0.0f;
    float pitch_ = 0.3f;
    float orbit_distance_ = 5.0f;
    Vec3 orbit_target_ = {0.0f, 0.0f, 0.0f};
    Vec3 position_ = {0.0f, 0.0f, 5.0f};
    CameraMode mode_ = CameraMode::Orbit;
    float fov_ = 45.0f * (std::numbers::pi_v<float> / 180.0f);
    float aspect_ = 16.0f / 9.0f;
    float near_ = 0.1f;
    float far_ = 1000.0f;
};

} // namespace gg
