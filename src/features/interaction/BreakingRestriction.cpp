#include "features/interaction/BreakingRestriction.h"
#include "features/interaction/PeriodicInput.h"
#include "app/Runtime.h"
#include "ui/SettingsScreen.h"
#include "input/Actions.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/service/TargetedBedrock.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/client/ClientExitLevelEvent.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/client/renderer/game/LevelRendererPlayer.h"
#include "mc/world/gamemode/GameMode.h"
#include <cmath>
#include <format>
#include <mutex>
#include <string>

namespace lamium::interaction::breaking {
namespace {
std::mutex mutex;
PressAnchor anchor;
ll::event::ListenerPtr exitListener;
#ifdef LAMIUM_RESEARCH_TRACE
// L-36: record how vanilla drives a held attack across a rejected block.
// Repeated identical calls are collapsed; the first 400 changes are logged.
std::mutex traceMutex;
std::string lastTrace;
unsigned traceLines = 0, traceRepeats = 0;
void traceBreak(char const* phase, Player& player, BlockPos const& pos, int allowed, int result) noexcept {
    try {
        auto client = ll::service::getClientInstance();
        if (!client || client->getLocalPlayer() != &player) return;
        auto line = std::format("{} pos={},{},{} allowed={} result={}", phase, pos.x, pos.y, pos.z, allowed, result);
        std::lock_guard lock(traceMutex);
        if (line == lastTrace) { ++traceRepeats; return; }
        if (traceLines >= 400) return;
        ++traceLines;
        Runtime::instance().self().getLogger().info("research L-36 {} (previous repeated {}x)", line, traceRepeats);
        lastTrace = std::move(line);
        traceRepeats = 0;
    } catch (...) {} // Diagnostics must not replace the vanilla result.
}
#else
void traceBreak(char const*, Player&, BlockPos const&, int, int) noexcept {}
#endif
// In a local world the integrated server's player runs the same GameMode
// calls; only the client's own player is restricted or anchors.
bool restricted(Player const& player) {
    auto& runtime = Runtime::instance();
    if (!runtime.enabled() || !runtime.snapshot()->interaction.breaking) return false;
    auto client = ll::service::getClientInstance();
    return client && client->getLocalPlayer() == &player;
}
bool gameplayInput() {
    auto client = ll::service::getClientInstance();
    return client && !ui::ownsInput() && gameplayScreen(client->getScreenName());
}
RestrictionRegion regionAt(Player& player, BlockPos const& pos, unsigned char face) {
    auto settings = Runtime::instance().snapshot();
    auto mode = settings->interaction.breakingMode;
    // Bedrock face IDs: down/up, north/south, west/east.
    Axis axis = face < 2 || face > 5 ? Axis::Y : face < 4 ? Axis::Z : Axis::X;
    if (mode == RestrictionMode::HeightBand) {
        int feet = static_cast<int>(std::floor(player.getFeetPos().y + .01f));
        return {mode, {pos.x, feet, pos.z}, Axis::Y, static_cast<int>(settings->interaction.breakingBand)};
    }
    return {mode, {pos.x, pos.y, pos.z}, axis};
}
// Anchors on the first block of the press in progress.
bool allowsAnchoring(Player& player, BlockPos const& pos, unsigned char face) {
    auto button = periodic::attackButton();
    std::lock_guard lock(mutex);
    return anchor.allows({pos.x, pos.y, pos.z}, button.held, button.press,
                         [&] { return regionAt(player, pos, face); });
}
LL_TYPE_INSTANCE_HOOK(FinishBreak, ll::memory::HookPriority::Highest, GameMode,
    &GameMode::$destroyBlock, bool, BlockPos const& pos, uchar face) {
    if (!allows(mPlayer,pos)) { traceBreak("destroy", mPlayer, pos, 0, 0); return false; }
    bool result = origin(pos,face);
    traceBreak("destroy", mPlayer, pos, 1, result);
    return result;
}
LL_TYPE_INSTANCE_HOOK(StopBreak, ll::memory::HookPriority::Normal, GameMode,
    &GameMode::$stopDestroyBlock, void, BlockPos const& pos) {
    traceBreak("stop", mPlayer, pos, -1, -1);
    origin(pos);
}
LL_TYPE_INSTANCE_HOOK(ChangeDimension, ll::memory::HookPriority::Normal, LevelRendererPlayer,
    &LevelRendererPlayer::$onWillChangeDimension, void, Player& player) {
    reset();
    origin(player);
}
struct Hook { int (*install)(bool); bool (*remove)(bool); bool installed = false; };
Hook hooks[]{{FinishBreak::hook,FinishBreak::unhook},{ChangeDimension::hook,ChangeDimension::unhook},
#ifdef LAMIUM_RESEARCH_TRACE
             {StopBreak::hook,StopBreak::unhook},
#endif
};
}
void reset() { std::lock_guard lock(mutex); anchor.reset(); }
std::optional<RestrictionRegion> region() {
    auto button = periodic::attackButton();
    std::lock_guard lock(mutex);
    anchor.follow(button.held, button.press);
    return anchor.region();
}
mining::Gate startGate(Player& player, BlockPos const& pos, unsigned char face) {
    if (!restricted(player)) return mining::Gate::Proceed;
    bool allowed = gameplayInput() && allowsAnchoring(player, pos, face);
    traceBreak("start", player, pos, allowed, -1);
    return allowed ? mining::Gate::Proceed : mining::Gate::End;
}
mining::Gate continueGate(Player& player, BlockPos const& pos, unsigned char face) {
    if (!restricted(player)) return mining::Gate::Proceed;
    // Ending here makes vanilla stop the session, and a held button never
    // restarts it (L-36). Skip a block outside the region but keep the
    // session, so an allowed block reached later continues breaking. A menu
    // or settings screen still ends it.
    bool gameplay = gameplayInput();
    bool allowed = gameplay && allowsAnchoring(player, pos, face);
    traceBreak("continue", player, pos, allowed, gameplay);
    if (allowed) return mining::Gate::Proceed;
    return gameplay ? mining::Gate::Pause : mining::Gate::End;
}
bool allows(Player& player, BlockPos const& pos) {
    if (!restricted(player)) return true;
    if (!gameplayInput()) return false;
    auto current = region();
    return !current || current->contains({pos.x,pos.y,pos.z});
}
void start() {
    try {
        for (auto& hook : hooks) if (!hook.installed) {
            if (hook.install(true) != 0) throw std::runtime_error("Could not install breaking restriction hook");
            hook.installed = true;
        }
        if (!exitListener) exitListener = ll::event::EventBus::getInstance().emplaceListener<ll::event::ClientExitLevelEvent>([](auto&) { reset(); });
    } catch (...) { stop(); throw; }
}
void stop() {
    reset();
    if (exitListener) { ll::event::EventBus::getInstance().removeListener(exitListener); exitListener.reset(); }
    for (auto& hook : hooks) if (hook.installed && hook.remove(true)) hook.installed = false;
}
}
