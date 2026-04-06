#pragma once

#include "ecs/components.h"

namespace gg {

Vec3 vec3_add(const Vec3& a, const Vec3& b);
Vec3 vec3_sub(const Vec3& a, const Vec3& b);
Vec3 vec3_scale(const Vec3& v, float s);
float vec3_dot(const Vec3& a, const Vec3& b);
Vec3 vec3_cross(const Vec3& a, const Vec3& b);
float vec3_length(const Vec3& v);
Vec3 vec3_normalize(const Vec3& v);

} // namespace gg
