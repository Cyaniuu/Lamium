#pragma once
#include "features/schematic/SaveArea.h"
#include <optional>

// The area chosen for saving as a schematic (BACKLOG L-93): two corner
// blocks set with keys on the looked-at block. Thread-safe: keys set it on the
// client thread, the world render draws its frame.
namespace lamium::schematic::selection {
struct State {
    std::optional<Point> first, second;
    int dimension = 0;
    std::optional<Area> area() const {
        if (!first || !second) return std::nullopt;
        return Area{*first, *second};
    }
};
State current();
// Corner 0 or 1. A corner in another dimension than the other one drops it.
void setCorner(int which, Point at, int dimension);
// Replaces both corners (the save prompt's adjusted numbers).
void setArea(Area area, int dimension);
void clear();
}
