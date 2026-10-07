#pragma once
#include "features/inventory/DeathLayout.h"
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

// The death layout document (BACKLOG L-109): one per world, beside its
// waypoints, so a long trip back still restores after rejoining.
namespace lamium::inventory::death {
std::string encodeLayout(Layout const&);
// Rejects other versions and broken documents; ignores slots out of range.
Layout decodeLayout(std::string_view);
std::optional<Layout> readLayout(std::filesystem::path const&);
// Writes the layout, or removes the file for none.
void writeLayout(std::filesystem::path const&, std::optional<Layout> const&);
}
