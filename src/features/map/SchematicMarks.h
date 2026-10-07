#pragma once
#include "features/map/Waypoints.h"
#include "features/schematic/Placement.h"
#include <cstdint>
#include <string>
#include <vector>

// Schematic placements on the minimap and world map (BACKLOG L-93): their
// footprints seen from above, drawn under waypoints.
namespace lamium::map {
struct PlacementMark {
    schematic::Footprint area;
    std::string name;
    bool visible = true, selected = false;
};
// The placements in this dimension while Schematics are on; empty otherwise.
std::vector<PlacementMark> placementMarks(int dimension);
// The map palette's cyan; the outline shape tells it from a waypoint.
inline constexpr std::uint32_t placementColor = waypointColors[5];
}
