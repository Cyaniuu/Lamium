#pragma once
#include <array>
#include <string_view>
namespace lamium::interaction {
// Saved by name; append only. Height band is a breaking mode (L-15); placement
// offers the first four.
enum class RestrictionMode { Plane, Line, Column, Layer, HeightBand };
inline constexpr std::array<std::string_view,5> restrictionNames{"plane","line","column","layer","heightBand"};
inline constexpr std::array<std::string_view,5> restrictionLabels{"mode.plane","mode.line","mode.column","mode.layer","mode.heightBand"};
inline constexpr std::array<std::string_view,4> placementLabels{"mode.plane","mode.line","mode.column","mode.layer"};
}
