#include "renderer/camera.h"

#include <algorithm>
#include <cmath>

namespace gg {

std::unique_ptr<Camera> Camera::create() {
    return std::unique_ptr<Camera>(new Camera());
}

Vec3 Camera::forward_direction() const {
    float cos_p = std::cos(pitch_);
    float sin_p = std::sin(pitch_);
    float cos_y = std::cos(yaw_);
    float sin_y = std::sin(yaw_);

    return vec3_normalize({
        -cos_p * sin_y,
        -sin_p,
        -cos_p * cos_y,
    });
}

Vec3 Camera::right_direction() const {
    return vec3_normalize(vec3_cross(forward_direction(), {0.0f, 1.0f, 0.0f}));
}

void Camera::look(float dx, float dy) {
    constexpr float sensitivity = 0.01f;
    yaw_ -= dx * sensitivity;
    pitch_ += dy * sensitivity;
    constexpr float max_pitch = 1.5f;
    pitch_ = std::clamp(pitch_, -max_pitch, max_pitch);
}

void Camera::orbit(float dx, float dy) {
    if (mode_ != CameraMode::Orbit) {
        return;
    }
    look(dx, dy);
}

void Camera::zoom(float delta) {
    if (mode_ != CameraMode::Orbit) {
        return;
    }

    constexpr float zoom_speed = 0.5f;
    orbit_distance_ -= delta * zoom_speed;
    orbit_distance_ = std::clamp(orbit_distance_, 0.5f, 100.0f);
}

void Camera::set_fly_mode(bool enabled) {
    if (enabled && mode_ == CameraMode::Fly) {
        return;
    }
    if (!enabled && mode_ == CameraMode::Orbit) {
        return;
    }

    const Vec3 forward = forward_direction();
    if (enabled) {
        position_ = eye_position();
        mode_ = CameraMode::Fly;
        return;
    }

    mode_ = CameraMode::Orbit;
    orbit_target_ = vec3_add(position_, vec3_scale(forward, orbit_distance_));
}

void Camera::move_local(float right, float /*up*/, float forward, float dt) {
    if (mode_ != CameraMode::Fly) {
        return;
    }

    constexpr float move_speed = 5.0f;
    const Vec3 delta = vec3_add(
        vec3_scale(right_direction(), right * move_speed * dt),
        vec3_scale(forward_direction(), -forward * move_speed * dt));
    position_ = vec3_add(position_, delta);
}

void Camera::set_aspect(float aspect) {
    aspect_ = aspect;
}

bool Camera::is_fly_mode() const {
    return mode_ == CameraMode::Fly;
}

Mat4 Camera::view_matrix() const {
    const Vec3 eye = eye_position();
    return Mat4::look_at(eye, vec3_add(eye, forward_direction()), {0.0f, 1.0f, 0.0f});
}

Mat4 Camera::projection_matrix() const {
    return Mat4::perspective(fov_, aspect_, near_, far_);
}

Mat4 Camera::view_projection_matrix() const {
    return projection_matrix() * view_matrix();
}

Vec3 Camera::eye_position() const {
    if (mode_ == CameraMode::Fly) {
        return position_;
    }
    return vec3_sub(orbit_target_, vec3_scale(forward_direction(), orbit_distance_));
}

float Camera::fov() const {
    return fov_;
}

} // namespace gg
