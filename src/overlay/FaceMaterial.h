#pragma once
// The unlit, alpha-blended material for overlay faces in the current
// graphics mode (shapes, schematic entity ghosts without a skin).
#include "mc/deps/minecraft_renderer/renderer/MaterialPtr.h"

class IClientInstance;
namespace lamium::overlay {
// Faces need an unlit, alpha-blended material, chosen per graphics mode:
// - Fancy: the hologram pointer material (vertex color, alpha blending,
//   depth-tested without depth writes). It culls, so faces get both windings.
// - Simple: that material is not loaded (its pointer resolves but draws
//   nothing); use the two-sided, additive lightning material, fainter.
// - Vibrant Visuals / ray tracing: the deferred pipeline does not show the
//   hologram material here. Try lightning and draw full-strength outlines so
//   the shape stays readable even if faces are not shown.
// The block selection overlay was unsuitable: it multiplies the scene color.
struct FaceMaterial {
    mce::MaterialPtr material;
    int variant;
    bool twoSided;
    float alpha;
    bool strongLines;
};
FaceMaterial faceMaterial(IClientInstance& client);
} // namespace lamium::overlay
