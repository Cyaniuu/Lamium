#pragma once
#include "features/schematic/Placement.h"
#include "features/schematic/Structure.h"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <string>
#include <string_view>

// Saving an area of the world as a .mcstructure (BACKLOG L-93): the area
// between two corner blocks, the file name typed for it, and a builder that
// collects blocks cell by cell. Reading the world is glue.
namespace lamium::schematic {
struct Area {
    Point a, b; // the two corner blocks, both inside the area
    Point low() const { return {std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)}; }
    Size size() const { return {std::abs(a.x - b.x) + 1, std::abs(a.y - b.y) + 1, std::abs(a.z - b.z) + 1}; }
    std::uint64_t cells() const {
        auto s = size();
        return static_cast<std::uint64_t>(s.x) * s.y * s.z;
    }
};
inline constexpr size_t maxSaveName = 64; // bytes of the name, without the extension

// A typed name as a file name in the schematics folder: characters a Windows
// file name cannot hold are dropped, surrounding spaces and trailing dots
// trimmed, a typed ".mcstructure" not doubled. Empty when nothing usable is left.
inline std::string schematicFileName(std::string_view typed) {
    constexpr std::string_view extension = ".mcstructure";
    if (typed.size() >= extension.size() && typed.substr(typed.size() - extension.size()) == extension)
        typed.remove_suffix(extension.size());
    std::string name;
    for (char c : typed) {
        auto u = static_cast<unsigned char>(c);
        if (u < 32 || std::string_view("<>:\"/\\|?*").find(c) != std::string_view::npos) continue;
        name += c;
    }
    auto first = name.find_first_not_of(' ');
    if (first == std::string::npos) return {};
    name.erase(0, first);
    if (name.size() > maxSaveName) {
        size_t cut = maxSaveName;
        // Do not split a UTF-8 sequence.
        while (cut > 0 && (static_cast<unsigned char>(name[cut]) & 0xc0) == 0x80) --cut;
        name.resize(cut);
    }
    while (!name.empty() && (name.back() == ' ' || name.back() == '.')) name.pop_back();
    return name.empty() ? std::string{} : name + std::string(extension);
}

// Fills a structure cell by cell. Equal block states share one palette entry;
// cells not set stay structure void.
class StructureBuilder {
public:
    StructureBuilder(Size size, Point worldOrigin) {
        out.size = size;
        out.worldOrigin = {worldOrigin.x, worldOrigin.y, worldOrigin.z};
        out.blocks.assign(out.cells(), voidCell);
    }
    void setBlock(std::int32_t cell, PaletteBlock const& block) { out.blocks[static_cast<size_t>(cell)] = paletteIndex(block); }
    // The second layer: water in a waterlogged block.
    void setLiquid(std::int32_t cell, PaletteBlock const& block) {
        if (out.liquids.empty()) out.liquids.assign(out.cells(), voidCell);
        out.liquids[static_cast<size_t>(cell)] = paletteIndex(block);
    }
    void addEntity(EntityRecord entity) { out.entities.push_back(std::move(entity)); }
    Structure const& structure() const { return out; }

private:
    std::int32_t paletteIndex(PaletteBlock const& block) {
        auto [found, added] = index.try_emplace(block.key(), static_cast<std::int32_t>(out.palette.size()));
        if (added) out.palette.push_back(block);
        return found->second;
    }
    Structure out;
    std::map<std::string, std::int32_t> index;
};
}
