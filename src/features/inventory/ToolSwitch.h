#pragma once
#include "features/interaction/MiningSession.h"
class Player;
class BlockPos;
namespace lamium::inventory::tools {
void start();
void stop();
// Mining-session gates and events for the client's own player
// (MiningSession.cpp).
interaction::mining::Gate startGate(Player&, BlockPos const&);
interaction::mining::Gate continueGate(Player&, BlockPos const&);
void mined(bool destroyed);
void stopped();
}
