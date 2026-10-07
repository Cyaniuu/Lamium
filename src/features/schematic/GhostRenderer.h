#pragma once
// Draws schematic placements as ghost blocks in the world (BACKLOG L-93),
// using the path found by the ghost probe: a private BlockTessellator,
// in-world tessellation per section, tinted vertex colors and the
// moving-block renderer's materials, lit as if fully bright.
#include "features/schematic/SaveArea.h"
#include "features/schematic/Verification.h"
#include <filesystem>
#include <memory>
#include <optional>
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

// Saving an area: the world render reads it in bounded steps (it needs the
// world's blocks) and writes the file when done. One save at a time.
struct SaveRequest {
    Area area;
    int dimension = 0;
    bool entities = false;
    std::filesystem::path path;
    std::string file; // the name shown in messages
};
// False while another save runs.
bool save(SaveRequest request);
// A finished save's message for a toast, handed out once.
std::optional<std::string> takeSaveMessage();
}
