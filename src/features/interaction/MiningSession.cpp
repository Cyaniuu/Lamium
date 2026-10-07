#include "features/interaction/MiningSessionHooks.h"
#include "features/interaction/BreakingRestriction.h"
#include "features/interaction/ToolGuard.h"
#include "features/inventory/ToolSwitch.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/service/TargetedBedrock.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/world/gamemode/GameMode.h"
#include <stdexcept>

namespace lamium::interaction::mining {
namespace {
// Touched only for the client's own player, on the client thread.
Session session;
bool restarting = false, pausing = false;
// In a local world the integrated server's player runs the same GameMode
// calls, possibly on another thread; it is left to vanilla.
bool clientPlayer(Player const& player) {
    auto client = ll::service::getClientInstance();
    return client && client->getLocalPlayer() == &player;
}
LL_TYPE_INSTANCE_HOOK(SessionStart, ll::memory::HookPriority::Highest, GameMode,
    &GameMode::$startDestroyBlock, bool, BlockPos const& pos, uchar face, bool& destroyed) {
    if (!clientPlayer(mPlayer)) return origin(pos, face, destroyed);
    auto gate = breaking::startGate(mPlayer, pos, face);
    if (gate == Gate::Proceed) gate = toolGuard::startGate(mPlayer, !restarting);
    if (gate == Gate::Proceed) gate = inventory::tools::startGate(mPlayer, pos);
    if (gate == Gate::End) { destroyed = false; return false; }
    session.started();
    bool result = origin(pos, face, destroyed);
    inventory::tools::mined(destroyed);
    return result;
}
LL_TYPE_INSTANCE_HOOK(SessionContinue, ll::memory::HookPriority::Highest, GameMode,
    &GameMode::$continueDestroyBlock, bool, BlockPos const& pos, uchar face, Vec3 const& playerPos, bool& destroyed) {
    if (!clientPlayer(mPlayer)) return origin(pos, face, playerPos, destroyed);
    auto gate = breaking::continueGate(mPlayer, pos, face);
    if (gate == Gate::Proceed) gate = toolGuard::continueGate(mPlayer);
    if (gate == Gate::Proceed) gate = inventory::tools::continueGate(mPlayer, pos);
    switch (session.next(gate, static_cast<float const&>(mDestroyProgress) > 0.f)) {
    case Step::Vanilla: {
        bool result = origin(pos, face, playerPos, destroyed);
        inventory::tools::mined(destroyed);
        return result;
    }
    case Step::Keep: destroyed = false; return true;
    case Step::StopAndKeep:
        destroyed = false;
        pausing = true;
        stopDestroyBlock(static_cast<BlockPos const&>(mDestroyBlockPos));
        pausing = false;
        return true;
    case Step::End: destroyed = false; return false;
    case Step::Start: {
        restarting = true;
        bool result = startDestroyBlock(pos, face, destroyed);
        restarting = false;
        return result;
    }
    }
    return origin(pos, face, playerPos, destroyed);
}
// A pause stops through vanilla too; only other stops end the session.
LL_TYPE_INSTANCE_HOOK(SessionStop, ll::memory::HookPriority::Highest, GameMode,
    &GameMode::$stopDestroyBlock, void, BlockPos const& pos) {
    if (!pausing && clientPlayer(mPlayer)) {
        session.reset();
        inventory::tools::stopped();
    }
    origin(pos);
}
struct Hook { int (*install)(bool); bool (*remove)(bool); bool installed = false; };
Hook hooks[] = {{SessionStart::hook, SessionStart::unhook}, {SessionContinue::hook, SessionContinue::unhook},
    {SessionStop::hook, SessionStop::unhook}};
}
void start() {
    for (auto& hook : hooks) if (!hook.installed) {
        if (hook.install(true) != 0) { stop(); throw std::runtime_error("Could not install mining session hook"); }
        hook.installed = true;
    }
}
void stop() {
    for (auto it = std::rbegin(hooks); it != std::rend(hooks); ++it)
        if (it->installed && it->remove(true)) it->installed = false;
    session.reset();
    restarting = pausing = false;
}
}
