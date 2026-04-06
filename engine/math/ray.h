#pragma once

#include "ecs/components.h"
#include "math/mat4.h"
#include "math/vec3.h"

#include <cstdint>

namespace gg {

struct Ray {
    Vec3 origin;
    Vec3 direction; // normalized
};

Ray ray_from_screen(
    float screen_x, float screen_y, uint32_t fb_width, uint32_t fb_height, const Mat4& inverse_vp);

float ray_axis_distance(const Ray& ray,
                        const Vec3& axis_origin,
                        const Vec3& axis_dir,
                        float& out_t);

/// Intersect ray with a plane defined by a point and normal.
/// Returns true if intersection exists (ray not parallel to plane).
/// out_t: parameter along ray, hit_point = ray.origin + ray.direction * out_t
bool ray_plane_intersect(const Ray& ray,
                         const Vec3& plane_point,
                         const Vec3& plane_normal,
                         float& out_t);

} // namespace gg
