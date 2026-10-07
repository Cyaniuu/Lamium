#pragma once
#include "features/interaction/MiningSession.h"
#include "features/interaction/RestrictionRegion.h"
class Player;
class BlockPos;
namespace lamium::interaction::breaking {
void start();
void stop();
void reset();
// Mining-session gates for the client's own player (MiningSession.cpp). The
// first block of an attack press anchors the region (L-15).
mining::Gate startGate(Player&, BlockPos const&, unsigned char face);
mining::Gate continueGate(Player&, BlockPos const&, unsigned char face);
bool allows(Player&, BlockPos const&);
// The region of the press in progress, if any.
std::optional<RestrictionRegion> region();
}
