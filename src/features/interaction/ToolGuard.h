#pragma once
#include "features/interaction/MiningSession.h"
class Player;
namespace lamium::interaction::toolGuard {
// Before a held tool (or worn elytra) breaks, swap in the same item from the
// inventory; without one, stop held mining (BACKLOG L-62).
void start();
void stop();
// Mining-session gates (MiningSession.cpp). A restart the session makes is
// not a new press.
mining::Gate startGate(Player&, bool newPress);
mining::Gate continueGate(Player&);
}
