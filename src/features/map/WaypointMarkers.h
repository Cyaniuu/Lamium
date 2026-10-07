#pragma once
#include "features/map/Waypoints.h"
#include "settings/Settings.h"
#include <optional>
class MinecraftUIRenderContext;
namespace lamium::map::markers {
// Waypoint markers in the world (BACKLOG L-60 step 5b): a diamond in the
// waypoint's color with its distance, the name when the crosshair is near.
// Drawn on the HUD from the camera copied at the last world render.
void draw(MinecraftUIRenderContext&, float width, float height, Settings::Map const&);
// The camera of the last world render, in world space; other HUD labels
// in the world (schematic entities) project with it.
std::optional<CameraView> lastCamera();
// The hold key hides them while held.
void setHidden(bool held);
void start();
void stop();
}
