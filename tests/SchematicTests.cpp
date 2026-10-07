#include "features/schematic/Structure.h"
#include "features/schematic/Verify.h"
#include "features/schematic/PlacementStore.h"
#include "features/schematic/Verification.h"
#include "features/schematic/SaveArea.h"
#include "ui/SavePromptLayout.h"
#include <set>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
void check(bool, char const*);
namespace {
using namespace lamium::schematic;
std::span<std::uint8_t const> span(std::string const& bytes) {
    return {reinterpret_cast<std::uint8_t const*>(bytes.data()), bytes.size()};
}
bool throws(std::string const& bytes) {
    try { parseStructure(span(bytes)); } catch (std::runtime_error const&) { return true; }
    return false;
}

Structure sample() {
    Structure s;
    s.size = {2, 3, 4};
    s.worldOrigin = {100, 64, -20};
    nbt::Compound stairs;
    stairs.set("weirdo_direction", {std::int32_t{1}});
    stairs.set("upside_down_bit", {std::int8_t{0}});
    s.palette = {{"minecraft:air", {}, 18168865}, {"minecraft:stone", {}, 18168865},
                 {"minecraft:oak_stairs", stairs, 18168865}, {"minecraft:chest", {}, 18168865},
                 {"minecraft:water", {}, 18168865}};
    s.blocks.assign(s.cells(), 0);
    s.blocks[s.cell(0, 0, 0)] = 1;
    s.blocks[s.cell(1, 2, 3)] = 2;
    s.blocks[s.cell(0, 1, 2)] = 3;
    s.blocks[s.cell(1, 0, 1)] = voidCell;
    s.liquids.assign(s.cells(), voidCell);
    s.liquids[s.cell(1, 2, 3)] = 4;
    nbt::Compound chest;
    chest.set("id", {std::string{"Chest"}});
    chest.set("Items", {nbt::List{nbt::Type::Compound, {}}});
    s.blockEntities[s.cell(0, 1, 2)] = chest;
    nbt::Compound stand;
    stand.set("identifier", {std::string{"minecraft:armor_stand"}});
    nbt::List pos{nbt::Type::Float, {{101.5f}, {64.f}, {-17.5f}}};
    stand.set("Pos", {pos});
    s.entities.push_back({"minecraft:armor_stand", 1.5, 0, 2.5, stand});
    return s;
}

void nbtBasics() {
    nbt::Root root{"", {}};
    root.compound.set("b", {std::int8_t{-3}});
    root.compound.set("s", {std::string{"hé"}});
    root.compound.set("l", {nbt::List{nbt::Type::Short, {{std::int16_t{7}}, {std::int16_t{-8}}}}});
    root.compound.set("a", {std::vector<std::int32_t>{1, 2, 3}});
    auto bytes = nbt::write(root);
    check(bytes[0] == 10 && bytes[1] == 0 && bytes[2] == 0, "the root is an unnamed compound");
    auto back = nbt::read(span(bytes));
    check(nbt::text({back.compound}) == R"({b:-3b,s:"hé",l:[7s,-8s],a:[1,2,3]})",
          "NBT round-trips little-endian with entry order kept");
    std::int64_t value = 0;
    check(back.compound.find("b")->integer(value) && value == -3 && !back.compound.find("s")->integer(value),
          "integers of any width read as integers; strings do not");
    bool truncated = false;
    try { nbt::read(span(bytes.substr(0, bytes.size() - 3))); } catch (std::runtime_error const&) { truncated = true; }
    check(truncated, "truncated NBT is rejected");
    std::string huge = bytes.substr(0, 3) + std::string("\x0b\x01\x00\x61\xff\xff\xff\x7f", 8);
    bool tooLong = false;
    try { nbt::read(span(huge)); } catch (std::runtime_error const&) { tooLong = true; }
    check(tooLong, "an array length past the end is rejected before allocating");
}

void structureRoundTrip() {
    auto original = sample();
    check(original.cell(0, 0, 1) == 1 && original.cell(0, 1, 0) == 4 && original.cell(1, 0, 0) == 12,
          "cells run z fastest, then y, then x");
    auto at = original.position(original.cell(1, 2, 3));
    check(at[0] == 1 && at[1] == 2 && at[2] == 3, "a cell index maps back to its position");
    auto parsed = parseStructure(span(writeStructure(original)));
    check(parsed.size == original.size && parsed.worldOrigin == original.worldOrigin, "size and origin survive");
    check(parsed.blocks == original.blocks && parsed.liquids == original.liquids, "both block layers survive");
    check(parsed.palette.size() == 5 && parsed.palette[2].key() == "minecraft:oak_stairs[upside_down_bit=0b,weirdo_direction=1]",
          "palette keys sort states by name");
    check(parsed.palette[0].isAir() && !parsed.palette[1].isAir(), "air is recognized by name");
    check(parsed.blockEntities.size() == 1 && parsed.blockEntities.contains(original.cell(0, 1, 2))
          && *parsed.blockEntities.at(original.cell(0, 1, 2)).find("id")->as<std::string>() == "Chest",
          "block entity data is keyed by cell");
    check(parsed.entities.size() == 1 && parsed.entities[0].identifier == "minecraft:armor_stand"
          && parsed.entities[0].x == 1.5 && parsed.entities[0].y == 0 && parsed.entities[0].z == 2.5,
          "entity positions become relative to the structure corner");

    auto dry = original;
    dry.liquids.clear();
    check(parseStructure(span(writeStructure(dry))).liquids.empty(), "an all-void second layer reads as empty");

    // Older exports store each layer as a list of ints.
    nbt::Root legacy = nbt::read(span(writeStructure(dry)));
    auto& body = *legacy.compound.entries[2].tag.as<nbt::Compound>();
    nbt::List layers{nbt::Type::List, {}};
    nbt::List ints{nbt::Type::Int, {}};
    for (auto index : dry.blocks) ints.items.push_back({index});
    layers.items.push_back({ints});
    body.set("block_indices", {layers});
    check(parseStructure(span(nbt::write(legacy))).blocks == dry.blocks, "list-of-int layers read too");
}

void structureRejects() {
    check(throws("") && throws("\x0a\x00\x00\x00"), "empty input and an empty compound are rejected");
    auto bad = sample();
    bad.blocks[0] = 9;
    bool caught = false;
    try { parseStructure(span(writeStructure(bad))); } catch (std::runtime_error const&) { caught = true; }
    check(caught, "a block index outside the palette is rejected");
    auto mismatched = sample();
    mismatched.blocks.pop_back();
    caught = false;
    try { writeStructure(mismatched); } catch (std::runtime_error const&) { caught = true; }
    check(caught, "writing a layer that does not match the size is refused");
}

// Optional: LAMIUM_SAMPLE_STRUCTURES names a folder of real exports to parse.
void sampleFiles() {
    char* folder = nullptr;
    size_t length = 0;
    if (_dupenv_s(&folder, &length, "LAMIUM_SAMPLE_STRUCTURES") || !folder) return;
    std::unique_ptr<char, decltype(&std::free)> owned(folder, &std::free);
    for (auto const& entry : std::filesystem::directory_iterator(folder)) {
        if (entry.path().extension() != ".mcstructure") continue;
        std::ifstream file(entry.path(), std::ios::binary);
        std::string bytes{std::istreambuf_iterator<char>(file), {}};
        auto parsed = parseStructure(span(bytes));
        check(parsed.blocks.size() == parsed.cells() && !parsed.palette.empty(), "a real export parses");
        auto again = parseStructure(span(writeStructure(parsed)));
        check(again.blocks == parsed.blocks && again.palette.size() == parsed.palette.size()
              && again.blockEntities.size() == parsed.blockEntities.size() && again.entities.size() == parsed.entities.size(),
              "a real export survives writing back");
    }
}

void placementTransforms() {
    Size size{3, 2, 5};
    for (int rotation = 0; rotation < 4; ++rotation)
        for (auto mirror : {Mirror::None, Mirror::X, Mirror::Z}) {
            Placement placement{{10, 64, -7}, rotation, mirror};
            Size placed = placedSize(size, rotation);
            std::set<std::tuple<int, int, int>> seen;
            bool inside = true, inverse = true;
            for (int x = 0; x < size.x; ++x) for (int y = 0; y < size.y; ++y) for (int z = 0; z < size.z; ++z) {
                auto world = toWorld(size, placement, {x, y, z});
                int dx = world.x - 10, dy = world.y - 64, dz = world.z + 7;
                inside = inside && dx >= 0 && dy >= 0 && dz >= 0 && dx < placed.x && dy < placed.y && dz < placed.z;
                seen.insert({world.x, world.y, world.z});
                auto back = toLocal(size, placement, world);
                inverse = inverse && back && *back == Point{x, y, z};
            }
            check(inside && seen.size() == static_cast<size_t>(size.x * size.y * size.z),
                  "every turn and mirror fills exactly the placed box");
            check(inverse, "toLocal undoes toWorld");
        }
    Placement turned{{0, 0, 0}, 1, Mirror::None};
    // A box 3 wide (x) and 5 deep (z): after one clockwise turn it is 5 wide and 3 deep,
    // and its north-east corner moves to the south-east.
    check(placedSize(size, 1) == Size{5, 2, 3} && toWorld(size, turned, {2, 0, 0}) == Point{4, 0, 2},
          "a clockwise turn takes the north-east corner to the south-east");
    check(toWorld(size, {{0, 0, 0}, 0, Mirror::X}, {0, 0, 0}) == Point{2, 0, 0}
          && toWorld(size, {{0, 0, 0}, 0, Mirror::Z}, {0, 0, 0}) == Point{0, 0, 4},
          "mirror X flips east-west and mirror Z flips north-south");
    check(!toLocal(size, turned, {5, 0, 0}) && !toLocal(size, turned, {0, 2, 0}) && !toLocal(size, turned, {-1, 0, 0}),
          "cells outside the placed box have no local cell");
    check(quarterTurns(-1) == 3 && quarterTurns(5) == 1, "turn counts wrap");

    bool centers = true;
    for (int rotation = 0; rotation < 4; ++rotation)
        for (auto mirror : {Mirror::None, Mirror::X, Mirror::Z}) {
            Placement placement{{10, 64, -7}, rotation, mirror};
            for (int x = 0; x < size.x; ++x) for (int z = 0; z < size.z; ++z) {
                auto cell = toWorld(size, placement, Point{x, 1, z});
                auto free = toWorldPosition(size, placement, Position{x + .5, 1.25, z + .5});
                centers = centers && free == Position{cell.x + .5, cell.y + .25, cell.z + .5};
            }
        }
    check(centers, "an entity in a cell's center stays in that cell's center after any turn and mirror");
}

void saveRules() {
    Area area{{5, 70, -2}, {3, 64, 1}};
    check(area.low() == Point{3, 64, -2} && area.size() == Size{3, 7, 4} && area.cells() == 84,
          "an area spans both corner blocks in any order");
    check(schematicFileName("hut") == "hut.mcstructure" && schematicFileName("hut.mcstructure") == "hut.mcstructure"
          && schematicFileName("  a/b:c?. ") == "abc.mcstructure" && schematicFileName(" .. ").empty() && schematicFileName("").empty(),
          "file names drop characters Windows forbids and do not double the extension");
    check(schematicFileName("小屋") == "小屋.mcstructure", "file names keep non-ASCII text");
    auto longName = schematicFileName(std::string(70, 'a'));
    auto longJapanese = schematicFileName(std::string(30, 'x') + "あいうえおかきくけこさしすせそ");
    check(longName.size() == maxSaveName + 12 && longJapanese.size() <= maxSaveName + 12
          && (static_cast<unsigned char>(longJapanese[longJapanese.size() - 13]) & 0xc0) != 0xc0,
          "long names are cut without splitting a character");

    check(chunkOf(0) == 0 && chunkOf(15) == 0 && chunkOf(16) == 1 && chunkOf(-1) == -1 && chunkOf(-16) == -1 && chunkOf(-17) == -2,
          "blocks map to chunks with floor division");
    Area wide{{-20, 60, 5}, {17, 62, 40}};
    auto columns = chunkColumns(wide);
    std::uint64_t covered = 0;
    for (auto const& c : columns) covered += c.cells(wide.size().y);
    check(columns.size() == 4 * 3 && covered == wide.cells() && columns.front().lowX == -20 && columns.front().highX == -17
          && columns.back().lowX == 16 && columns.back().highX == 17 && columns.back().highZ == 40,
          "chunk columns cover the area exactly, cut at chunk borders");

    StructureBuilder builder({2, 1, 2}, {10, 64, 20});
    PaletteBlock stone{"minecraft:stone", {}, 1}, air{"minecraft:air", {}, 1}, water{"minecraft:water", {}, 1};
    nbt::Compound west;
    west.set("weirdo_direction", {std::int32_t{1}});
    PaletteBlock stairs{"minecraft:oak_stairs", west, 1};
    builder.setBlock(0, stone);
    builder.setBlock(1, air);
    builder.setBlock(2, stone);
    builder.setBlock(3, stairs);
    builder.setLiquid(3, water);
    nbt::Compound standData;
    standData.set("identifier", {std::string("minecraft:armor_stand")});
    nbt::List pos{nbt::Type::Float, {}};
    for (float v : {11.5f, 64.f, 21.5f}) pos.items.push_back({v});
    standData.set("Pos", {pos});
    builder.addEntity({"minecraft:armor_stand", 1.5, 0, 1.5, standData});
    auto const& built = builder.structure();
    check(built.palette.size() == 4 && built.blocks[0] == built.blocks[2] && built.liquids.size() == 4 && built.liquids[0] == voidCell,
          "equal blocks share a palette entry and the liquid layer is filled only where set");
    auto parsed = parseStructure(span(writeStructure(built)));
    check(parsed.size == built.size && parsed.blocks == built.blocks && parsed.liquids == built.liquids
          && parsed.palette[2].key() == stairs.key() && parsed.entities.size() == 1 && parsed.entities[0].x == 1.5
          && parsed.entities[0].z == 1.5, "a saved area reads back with its blocks, states, water and entities");
}

void savePromptHits() {
    auto l = lamium::ui::SavePromptLayout::at(480, 270);
    using Part = lamium::ui::SavePromptLayout::Part;
    auto minus = l.hit(l.cellX(2) + 2, l.cornerY(1) + 2), plus = l.hit(l.cellX(0) + l.cellWidth() - 2, l.cornerY(0) + 2);
    check(minus.part == Part::Minus && minus.corner == 1 && minus.axis == 2 && plus.part == Part::Plus && plus.corner == 0
          && plus.axis == 0, "the save prompt's steppers name their corner and axis");
    check(l.hit(l.saveX() + 1, l.buttonY() + 1).part == Part::Save && l.hit(l.cancelX() + 1, l.buttonY() + 1).part == Part::Cancel
          && l.hit(l.left + 20, l.fieldY() + 2).part == Part::Field && l.hit(l.clearX() + 1, l.buttonY() + 1).part == Part::Clear
          && l.clearX() + lamium::ui::SavePromptLayout::buttonWidth < l.saveX() && l.keysY() + 10 <= l.top + l.height(),
          "the save prompt's buttons and name field are where they are drawn, inside the panel");
}

void entityRules() {
    std::vector<EntitySpot> expected{{"minecraft:armor_stand", 1.5, 64, 1.5}, {"minecraft:armor_stand", 3.5, 64, 1.5},
                                     {"minecraft:pig", 5.5, 64, 5.5}};
    std::vector<EntitySpot> actual{{"minecraft:armor_stand", 1.8, 64, 1.4}, {"minecraft:pig", 9, 64, 9},
                                   {"minecraft:cow", 5.5, 64, 5.5}};
    auto placed = matchEntities(expected, actual);
    check(placed == std::vector<bool>{true, false, false}, "an entity counts near its spot, by type, and only once");
    std::vector<EntitySpot> high{{"minecraft:pig", 5.5, 66, 5.5}};
    check(matchEntities(std::span(expected).subspan(2), high) == std::vector<bool>{false}, "an entity two blocks up is not near");
    check(entityNameKey("minecraft:armor_stand") == "entity.armor_stand.name" && entityNameKey("mod:thing") == "entity.mod:thing.name",
          "entity name keys drop the vanilla namespace only");

    std::vector<MaterialLine> lines(3);
    lines[0] = {"minecraft:armor_stand", "Armor Stand", "", 2, 0, true};
    lines[1] = {"minecraft:stone", "Stone", "", 1, 1, false};
    lines[2] = {"minecraft:dirt", "Dirt", "", 5, 0, false};
    sortMaterials(lines);
    check(lines[0].name == "Dirt" && lines[1].name == "Stone" && lines[2].entity, "entity lines come after all block lines");
}

void layerRules() {
    Size placed{4, 6, 3};
    Layers layers;
    check(layerShown(layers, placed, {3, 5, 2}), "all layers show everything");
    layers = {LayerAxis::UpFromBottom, LayerMode::Only, 2};
    check(layerShown(layers, placed, {0, 2, 0}) && !layerShown(layers, placed, {0, 3, 0}), "only one height layer");
    layers = {LayerAxis::DownFromTop, LayerMode::UpTo, 1};
    check(layerShown(layers, placed, {0, 5, 0}) && layerShown(layers, placed, {0, 4, 0}) && !layerShown(layers, placed, {0, 3, 0}),
          "from the top, up to the second layer");
    layers = {LayerAxis::WestFromEast, LayerMode::Only, 0};
    check(layerShown(layers, placed, {3, 0, 0}) && !layerShown(layers, placed, {0, 0, 0}), "side layers count from the chosen side");
    check(layerCount(placed, LayerAxis::SouthFromNorth) == 3 && layerCount(placed, LayerAxis::EastFromWest) == 4,
          "layer counts follow the axis");
}

void verifyRules() {
    PaletteBlock air{"minecraft:air", {}, 0}, stone{"minecraft:stone", {}, 0};
    nbt::Compound east;
    east.set("weirdo_direction", {std::int32_t{0}});
    PaletteBlock stairs{"minecraft:oak_stairs", east, 0};
    auto key = stairs.key();
    WorldBlock worldAir{"minecraft:air", "minecraft:air"}, worldStone{"minecraft:stone", "minecraft:stone"};
    WorldBlock worldStairs{"minecraft:oak_stairs", key}, turnedStairs{"minecraft:oak_stairs", "minecraft:oak_stairs[weirdo_direction=2]"};
    check(classify(nullptr, "", worldStone, true) == CellState::Ignored, "structure void is never checked");
    check(classify(&stone, stone.key(), std::nullopt, true) == CellState::Unknown, "unloaded cells are unknown");
    check(classify(&stone, stone.key(), worldStone, true) == CellState::Correct
          && classify(&stone, stone.key(), worldAir, true) == CellState::Missing
          && classify(&stone, stone.key(), worldStairs, true) == CellState::Wrong,
          "correct, missing and wrong blocks");
    check(classify(&stairs, key, worldStairs, true) == CellState::Correct
          && classify(&stairs, key, turnedStairs, true) == CellState::State, "the same block facing another way is a state mismatch");
    check(classify(&air, air.key(), worldStone, true) == CellState::Extra
          && classify(&air, air.key(), worldStone, false) == CellState::Ignored
          && classify(&air, air.key(), worldAir, true) == CellState::Correct,
          "extra blocks count only when the placement counts them");

    Tally tally;
    tally.add(CellState::Correct, true);
    tally.add(CellState::Correct, false);
    tally.add(CellState::Missing, true);
    tally.add(CellState::Wrong, true);
    tally.add(CellState::State, true);
    tally.add(CellState::Extra, false);
    tally.add(CellState::Unknown, true);
    check(tally.correct == 1 && tally.total() == 5 && tally.mistakes() == 3,
          "correct air is not counted; unknown cells stay in the total but are not placed");

    Materials materials;
    addMaterial(materials, stone, CellState::Correct);
    addMaterial(materials, stone, CellState::Missing);
    addMaterial(materials, air, CellState::Correct);
    addMaterial(materials, stairs, CellState::State);
    check(materials.size() == 2 && materials["minecraft:stone"].needed == 2 && materials["minecraft:stone"].remaining() == 1
          && materials["minecraft:oak_stairs"].placed == 0, "materials count needed and correctly placed blocks, not air");
}

void placementDocuments() {
    PlacementSet set;
    SavedPlacement hut;
    hut.name = "hut";
    hut.file = "small/hut.mcstructure";
    hut.dimension = 1;
    hut.placement = {{124, 64, -38}, 3, Mirror::Z};
    hut.layers = {LayerAxis::WestFromEast, LayerMode::UpTo, 2};
    hut.visible = false;
    hut.countExtras = false;
    set.placements.push_back(hut);
    set.selected = 0;
    auto back = decodePlacements(encodePlacements(set));
    auto const& p = back.placements.at(0);
    check(back.selected == 0 && p.name == "hut" && p.file == "small/hut.mcstructure" && p.dimension == 1
          && p.placement.origin == Point{124, 64, -38} && p.placement.rotation == 3 && p.placement.mirror == Mirror::Z
          && p.layers == hut.layers && !p.visible && !p.countExtras && p.entities,
          "placements round-trip");
    auto tolerant = decodePlacements(R"({"version":1,"placements":[{"file":"a.mcstructure"},{"file":"../x.mcstructure"},
        {"file":"C:/x.mcstructure"},{"name":"no file"}],"selected":7})");
    check(tolerant.placements.size() == 1 && tolerant.placements[0].name == "a.mcstructure" && tolerant.placements[0].visible
          && tolerant.placements[0].countExtras && tolerant.selected == -1,
          "missing fields take defaults, unsafe paths are dropped, a stale selection clears");
    bool rejected = false;
    try { decodePlacements(R"({"version":2})"); } catch (std::exception const&) { rejected = true; }
    check(rejected, "other document versions are rejected");
    check(safeSchematicPath("farms/iron.mcstructure") && !safeSchematicPath("") && !safeSchematicPath("/abs")
          && !safeSchematicPath("a//b") && !safeSchematicPath("a/../b") && !safeSchematicPath(R"(a\b)"),
          "schematic paths stay inside the schematics folder");
}

void verificationOrder() {
    std::vector<Mismatch> list{{CellState::Missing, {1, 0, 0}}, {CellState::Wrong, {9, 0, 0}}, {CellState::State, {2, 0, 0}},
                               {CellState::Missing, {0, 0, 0}}};
    sortMismatches(list, 0, 0, 0);
    check(list[0].state == CellState::State && list[1].state == CellState::Wrong && list[2].position == Point{0, 0, 0}
          && list[3].position == Point{1, 0, 0}, "mistakes come before missing blocks, each nearest first");
    std::vector<MaterialLine> lines{{"a", "Stone", "", 10, 10}, {"b", "Planks", "", 5, 1}, {"c", "Glass", "", 9, 1}};
    sortMaterials(lines);
    check(lines[0].name == "Glass" && lines[1].name == "Planks" && lines[2].name == "Stone" && lines[2].remaining() == 0,
          "materials list the most remaining first and finished lines last");
    check(itemsPerBlock("minecraft:oak_double_slab", false) == 2 && itemsPerBlock("minecraft:wooden_door", true) == 0
          && itemsPerBlock("minecraft:stone", false) == 1, "double slabs need two items; second halves none");
}
}
void schematicTests() {
    verificationOrder();
    placementDocuments();
    placementTransforms();
    entityRules();
    saveRules();
    savePromptHits();
    layerRules();
    verifyRules();
    nbtBasics();
    structureRoundTrip();
    structureRejects();
    sampleFiles();
}
