#pragma once
#include "features/interaction/MiningSession.h"
namespace lamium::interaction::mining {
// Installs the GameMode mining hooks that ask Breaking Restriction, Tool
// Protection and Tool Switch in order (MiningSession.h).
void start();
void stop();
}
