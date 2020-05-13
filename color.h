#pragma once

#include "vec3.h"
#include <iostream>

using color = vec3;

inline void write_color(std::ostream& out, color pixel_color) {
    // Write the translated [0,255] value of each color component.
    out << static_cast<int>(255.999 * pixel_color.x()) << ' '
        << static_cast<int>(255.999 * pixel_color.y()) << ' '
        << static_cast<int>(255.999 * pixel_color.z()) << '\n';
}

inline void write_color(unsigned char* out, color pixel_color) {
    out[0] = static_cast<int>(255.999 * pixel_color.x());
    out[1] = static_cast<int>(255.999 * pixel_color.y());
    out[2] = static_cast<int>(255.999 * pixel_color.z());
}

void write_color(std::ostream& out, color pixel_color, int samples_per_pixel);
void write_color(unsigned char* out, color pixel_color, int samples_per_pixel);
