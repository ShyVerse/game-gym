#include "renderer/camera.h"

#include <algorithm>
#include <cmath>

namespace gg {

std::unique_ptr<Camera> Camera::create() {
    return std::unique_ptr<Camera>(new Camera());
}

void Camera::orbit(float dx, float dy) {
    constexpr float sensitivity = 0.01f;
    yaw_ += dx * sensitivity;
    pitch_ += dy * sensitivity;
    constexpr float max_pitch = 1.5f;
    pitch_ = std::clamp(pitch_, -max_pitch, max_pitch);
}

void Camera::zoom(float delta) {
    constexpr float zoom_speed = 0.5f;
    distance_ -= delta * zoom_speed;
    distance_ = std::clamp(distance_, 0.5f, 100.0f);
}

void Camera::set_aspect(float aspect) {
    aspect_ = aspect;
}

Mat4 Camera::view_matrix() const {
    float cos_p = std::cos(pitch_);
    float sin_p = std::sin(pitch_);
    float cos_y = std::cos(yaw_);
    float sin_y = std::sin(yaw_);

    Vec3 eye{
        target_.x + distance_ * cos_p * sin_y,
        target_.y + distance_ * sin_p,
        target_.z + distance_ * cos_p * cos_y,
    };
    return Mat4::look_at(eye, target_, {0.0f, 1.0f, 0.0f});
}

Mat4 Camera::projection_matrix() const {
    return Mat4::perspective(fov_, aspect_, near_, far_);
}

Mat4 Camera::view_projection_matrix() const {
    return projection_matrix() * view_matrix();
}

} // namespace gg
