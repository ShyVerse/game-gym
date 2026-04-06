#include "math/vec3.h"

#include <cmath>

namespace gg {

Vec3 vec3_add(const Vec3& a, const Vec3& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3 vec3_sub(const Vec3& a, const Vec3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 vec3_scale(const Vec3& v, float s) {
    return {v.x * s, v.y * s, v.z * s};
}

float vec3_dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 vec3_cross(const Vec3& a, const Vec3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

float vec3_length(const Vec3& v) {
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

Vec3 vec3_normalize(const Vec3& v) {
    float len = vec3_length(v);
    if (len < 1e-8f) {
        return {0.0f, 0.0f, 0.0f};
    }
    return vec3_scale(v, 1.0f / len);
}

} // namespace gg
