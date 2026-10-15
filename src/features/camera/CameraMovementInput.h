#pragma once
#include "features/camera/DetachedCameraMotion.h"
#include <cstddef>
struct RawMoveInputComponent;
struct MoveInputComponent;

namespace lamium::camera {
template <class InputFlags, class... Indices>
void clearMovementFlags(InputFlags& flags, Indices... indices) {
    (flags.set(static_cast<size_t>(indices), false), ...);
}
bool freecameraSprintHeld(RawMoveInputComponent const&);
// Call only for the detached session's local owner, after vanilla input update.
// Returns right/up/forward axes and removes movement from processed player
// output while preserving physical device state for continuous input.
DetachedCameraMotion::Vector consumeMovement(RawMoveInputComponent&);
DetachedCameraMotion::Vector freecameraInputAxes(MoveInputComponent const&);
DetachedCameraMotion::Vector consumeMovement(MoveInputComponent&);
// Stash source for the FreeCamera adapter. Keyboard state may not have reached
// the extracted analog vector yet, so WASD-equivalent direction flags are read
// first with the analog vector as a controller fallback; axes are unit-clamped.
// Axis signs follow vanilla (Up/W is forward).
DetachedCameraMotion::Vector freecameraInputAxes(RawMoveInputComponent const& raw);
}
