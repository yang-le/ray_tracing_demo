#pragma once

#include "rtweekend.h"

class camera {
public:
    camera(
        point3 lookfrom, point3 lookat, vec3 vup,
        double vfov, // vertical field-of-view in degrees
        double aspect_ratio, double aperture, double focus_dist
    ) {
        origin = lookfrom;
        lens_radius = aperture / 2;

        auto theta = degrees_to_radians(vfov);
        auto half_height = tan(theta / 2);
        auto half_width = aspect_ratio * half_height;
        w = unit_vector(lookfrom - lookat);
        u = unit_vector(cross(vup, w));
        v = cross(w, u);

        lower_left_corner = origin
            - half_width * focus_dist * u
            - half_height * focus_dist * v
            - focus_dist * w;

        horizontal = 2 * half_width * focus_dist * u;
        vertical = 2 * half_height * focus_dist * v;
    }

    ray get_ray(double s, double t) const {
        vec3 rd = lens_radius * vec3::random_in_unit_disk();
        vec3 offset = u * rd.x() + v * rd.y();

        return ray(
            origin + offset,
            lower_left_corner + s * horizontal + t * vertical - origin - offset
        );
    }

private:
    point3 origin;
    point3 lower_left_corner;
    vec3 horizontal;
    vec3 vertical;
    vec3 u, v, w;
    double lens_radius;
};
