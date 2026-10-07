#pragma once
// Draws schematic placements as ghost blocks in the world (BACKLOG L-93),
// using the path found by the ghost probe: a private BlockTessellator,
// in-world tessellation per section, tinted vertex colors and the
// moving-block renderer's materials, lit as if fully bright.
#include "features/schematic/Verification.h"
#include <memory>
#include <string>
#include <utility>
#include <vector>
namespace lamium::schematic::ghosts {
void start();
void stop();
// The latest finished verification of the selected placement (never null).
std::shared_ptr<Verification const> verification();
// Names over the frames of missing entities, at world positions; the HUD
// draws them.
std::vector<std::pair<Position, std::string>> entityLabels();
// Marks a cell in the world for a while ("Show in world").
void point(Point cell);
}
