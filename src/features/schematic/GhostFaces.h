#pragma once
#include <cmath>
#include <span>

// Which side of its cell a ghost quad lies on (BACKLOG L-93). A quad flat on
// a cell side that touches an opaque ghost is never seen from outside, and
// drawing it fought with the neighbor's face; the glue drops such quads.
namespace lamium::schematic::faces {
struct Vertex { float x, y, z; };
// Sides: 0 west (-x), 1 east (+x), 2 down (-y), 3 up (+y), 4 north (-z), 5 south (+z).
inline constexpr int offsets[6][3] = {{-1,0,0},{1,0,0},{0,-1,0},{0,1,0},{0,0,-1},{0,0,1}};
// The side every vertex of the quad lies on, or -1 (inside the cell, slanted).
inline int sideOf(std::span<Vertex const> quad, int x, int y, int z, float epsilon = 1e-3f) {
    if (quad.empty()) return -1;
    float low[3]{static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)};
    for (int side = 0; side < 6; ++side) {
        int axis = side / 2;
        float plane = low[axis] + (side % 2 ? 1.f : 0.f);
        bool flat = true;
        for (auto const& v : quad) {
            float c = axis == 0 ? v.x : axis == 1 ? v.y : v.z;
            if (std::abs(c - plane) > epsilon) { flat = false; break; }
        }
        if (flat) return side;
    }
    return -1;
}
}
