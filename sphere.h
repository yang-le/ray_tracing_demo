#pragma once

#include "hittable.h"
#include "vec3.h"

class sphere : public hittable {
public:
    sphere() {}
    sphere(point3 cen, double r, shared_ptr<material> m)
        : center(cen), radius(r), mat_ptr(m) {};

    virtual bool hit(const ray& r, double tmin, double tmax, hit_record& rec) const;

public:
    point3 center;
    double radius;
    shared_ptr<material> mat_ptr;
};
