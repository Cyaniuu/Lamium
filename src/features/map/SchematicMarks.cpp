#include "features/map/SchematicMarks.h"
#include "features/schematic/SchematicSession.h"
#include "app/Runtime.h"

namespace lamium::map {
std::vector<PlacementMark> placementMarks(int dimension) {
    std::vector<PlacementMark> marks;
    auto& runtime = Runtime::instance();
    if (!runtime.enabled() || !runtime.snapshot()->schematic.enabled) return marks;
    auto shown = schematic::session::snapshot();
    for (size_t i = 0; i < shown.placements.size(); ++i) {
        auto const& [saved, structure] = shown.placements[i];
        if (!structure || saved.dimension != dimension) continue;
        marks.push_back({schematic::footprint(structure->size, saved.placement), saved.name, saved.visible,
                         static_cast<int>(i) == shown.selected});
    }
    return marks;
}
}
