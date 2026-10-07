#include "features/schematic/Selection.h"
#include <mutex>

namespace lamium::schematic::selection {
namespace {
std::mutex mutex;
State state;
}
State current() {
    std::lock_guard lock(mutex);
    return state;
}
void setCorner(int which, Point at, int dimension) {
    std::lock_guard lock(mutex);
    if (state.dimension != dimension) state = {};
    state.dimension = dimension;
    (which == 0 ? state.first : state.second) = at;
}
void setArea(Area area, int dimension) {
    std::lock_guard lock(mutex);
    state = {area.a, area.b, dimension};
}
void clear() {
    std::lock_guard lock(mutex);
    state = {};
}
}
