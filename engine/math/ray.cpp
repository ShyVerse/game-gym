#include "math/ray.h"

#include <cmath>

namespace gg {

Ray ray_from_screen(
    float screen_x, float screen_y, uint32_t fb_width, uint32_t fb_height, const Mat4& inverse_vp) {
    float ndc_x = (2.0f * screen_x / float(fb_width)) - 1.0f;
    float ndc_y = 1.0f - (2.0f * screen_y / float(fb_height));

    Vec3 near_ndc{ndc_x, ndc_y, 0.0f};
    Vec3 far_ndc{ndc_x, ndc_y, 1.0f};

    Vec3 near_world = inverse_vp.transform_point(near_ndc);
    Vec3 far_world = inverse_vp.transform_point(far_ndc);

    Vec3 dir = vec3_normalize(vec3_sub(far_world, near_world));
    return {near_world, dir};
}

float ray_axis_distance(const Ray& ray,
                        const Vec3& axis_origin,
                        const Vec3& axis_dir,
                        float& out_t) {
    Vec3 w0 = vec3_sub(ray.origin, axis_origin);
    float a = vec3_dot(ray.direction, ray.direction);
    float b = vec3_dot(ray.direction, axis_dir);
    float c = vec3_dot(axis_dir, axis_dir);
    float d = vec3_dot(ray.direction, w0);
    float e = vec3_dot(axis_dir, w0);
    float denom = a * c - b * b;

    float s;
    if (std::abs(denom) < 1e-8f) {
        s = 0.0f;
        out_t = e / c;
    } else {
        s = (b * e - c * d) / denom;
        out_t = (a * e - b * d) / denom;
    }

    if (s < 0.0f) {
        s = 0.0f;
        out_t = e / c;
    }

    Vec3 p1 = vec3_add(ray.origin, vec3_scale(ray.direction, s));
    Vec3 p2 = vec3_add(axis_origin, vec3_scale(axis_dir, out_t));

    return vec3_length(vec3_sub(p1, p2));
}

bool ray_plane_intersect(const Ray& ray,
                         const Vec3& plane_point,
                         const Vec3& plane_normal,
                         float& out_t) {
    float denom = vec3_dot(plane_normal, ray.direction);
    if (std::abs(denom) < 1e-8f) {
        return false; // parallel
    }
    Vec3 to_plane = vec3_sub(plane_point, ray.origin);
    out_t = vec3_dot(to_plane, plane_normal) / denom;
    return out_t >= 0.0f;
}

} // namespace gg
