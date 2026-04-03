#pragma once

#include "ecs/components.h"

#include <cmath>
#include <cstring>

namespace gg {

struct Mat4 {
    float data[16]; // column-major: data[col*4 + row]

    static Mat4 identity() {
        Mat4 m{};
        std::memset(m.data, 0, sizeof(m.data));
        m.data[0] = 1.0f;
        m.data[5] = 1.0f;
        m.data[10] = 1.0f;
        m.data[15] = 1.0f;
        return m;
    }

    static Mat4 perspective(float fov_rad, float aspect, float near, float far) {
        Mat4 m{};
        std::memset(m.data, 0, sizeof(m.data));
        float t = std::tan(fov_rad / 2.0f);
        m.data[0] = 1.0f / (aspect * t);
        m.data[5] = 1.0f / t;
        m.data[10] = far / (near - far);
        m.data[11] = -1.0f;
        m.data[14] = (far * near) / (near - far);
        return m;
    }

    static Mat4 translation(const Vec3& offset) {
        Mat4 m = identity();
        m.data[12] = offset.x;
        m.data[13] = offset.y;
        m.data[14] = offset.z;
        return m;
    }

    static Mat4 scale(const Vec3& scale) {
        Mat4 m = identity();
        m.data[0] = scale.x;
        m.data[5] = scale.y;
        m.data[10] = scale.z;
        return m;
    }

    static Mat4 rotation(const Quat& q) {
        Mat4 m = identity();

        const float xx = q.x * q.x;
        const float yy = q.y * q.y;
        const float zz = q.z * q.z;
        const float xy = q.x * q.y;
        const float xz = q.x * q.z;
        const float yz = q.y * q.z;
        const float wx = q.w * q.x;
        const float wy = q.w * q.y;
        const float wz = q.w * q.z;

        m.data[0] = 1.0f - 2.0f * (yy + zz);
        m.data[1] = 2.0f * (xy + wz);
        m.data[2] = 2.0f * (xz - wy);

        m.data[4] = 2.0f * (xy - wz);
        m.data[5] = 1.0f - 2.0f * (xx + zz);
        m.data[6] = 2.0f * (yz + wx);

        m.data[8] = 2.0f * (xz + wy);
        m.data[9] = 2.0f * (yz - wx);
        m.data[10] = 1.0f - 2.0f * (xx + yy);

        return m;
    }

    static Mat4 from_transform(const Transform& transform) {
        return translation(transform.position) * rotation(transform.rotation) *
               scale(transform.scale);
    }

    static Mat4 look_at(const Vec3& eye, const Vec3& target, const Vec3& up) {
        float fx = target.x - eye.x;
        float fy = target.y - eye.y;
        float fz = target.z - eye.z;
        float fl = std::sqrt(fx * fx + fy * fy + fz * fz);
        if (fl > 0.0f) {
            fx /= fl;
            fy /= fl;
            fz /= fl;
        }

        float rx = fy * up.z - fz * up.y;
        float ry = fz * up.x - fx * up.z;
        float rz = fx * up.y - fy * up.x;
        float rl = std::sqrt(rx * rx + ry * ry + rz * rz);
        if (rl > 0.0f) {
            rx /= rl;
            ry /= rl;
            rz /= rl;
        }

        float ux = ry * fz - rz * fy;
        float uy = rz * fx - rx * fz;
        float uz = rx * fy - ry * fx;

        Mat4 m{};
        m.data[0] = rx;
        m.data[1] = ux;
        m.data[2] = -fx;
        m.data[3] = 0.0f;
        m.data[4] = ry;
        m.data[5] = uy;
        m.data[6] = -fy;
        m.data[7] = 0.0f;
        m.data[8] = rz;
        m.data[9] = uz;
        m.data[10] = -fz;
        m.data[11] = 0.0f;
        m.data[12] = -(rx * eye.x + ry * eye.y + rz * eye.z);
        m.data[13] = -(ux * eye.x + uy * eye.y + uz * eye.z);
        m.data[14] = (fx * eye.x + fy * eye.y + fz * eye.z);
        m.data[15] = 1.0f;
        return m;
    }

    Mat4 operator*(const Mat4& rhs) const {
        Mat4 result{};
        for (int col = 0; col < 4; ++col) {
            for (int row = 0; row < 4; ++row) {
                float sum = 0.0f;
                for (int k = 0; k < 4; ++k) {
                    sum += data[k * 4 + row] * rhs.data[col * 4 + k];
                }
                result.data[col * 4 + row] = sum;
            }
        }
        return result;
    }
};

} // namespace gg
