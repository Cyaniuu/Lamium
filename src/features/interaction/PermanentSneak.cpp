#include "features/interaction/PermanentSneak.h"
#include "features/interaction/AutomationInput.h"
#include "features/camera/CameraSessions.h"
#include "app/Runtime.h"
#include "input/Actions.h"
#include "ui/SettingsScreen.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/service/TargetedBedrock.h"
#include "mc/client/input/ClientInputHandler.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/client/renderer/game/LevelRendererPlayer.h"
#include "mc/entity/components/MoveInputComponent.h"
#include "mc/entity/components/MobEffectsComponent.h"
#include "mc/deps/vanilla_components/ActorDataFlagComponent.h"
#include "mc/deps/vanilla_components/MovementAbilitiesComponent.h"
#include "mc/input/MoveInputState.h"
#include <stdexcept>

namespace lamium::interaction::sneak {
namespace {
AutomationInput intent;
AutomationInput sprintIntent;
bool installed = false;
bool dimensionInstalled = false;
unsigned observed = 0, matched = 0, rawSneak = 0;
struct RestoreRawFlags {
    MoveInputState& state;
    bool sneakDown;
    bool sprintDown;
    ~RestoreRawFlags() noexcept {
        auto& flags = state.mFlagValues;
        flags->set(static_cast<size_t>(MoveInputState::Flag::SneakDown), sneakDown);
        flags->set(static_cast<size_t>(MoveInputState::Flag::SprintDown), sprintDown);
    }
};
bool eligible(IClientInstance& client) {
    auto* player = client.getLocalPlayer();
    return Runtime::instance().enabled() && !ui::ownsInput()
        && gameplayScreen(client.getScreenName()) && player && player->isAlive()
        && !player->isSleeping() && !player->getVehicle()
        && !CameraSessions::instance().blocksLookInteraction(*player);
}
LL_TYPE_INSTANCE_HOOK(ApplyAutomationInput, ll::memory::HookPriority::Normal, ClientInputHandler,
    &ClientInputHandler::updatePlayerState, void,
    gsl::not_null<IClientInstance const*> client, MovementAbilitiesComponent const& abilities,
    MobEffectsComponent const& effects, ActorDataFlagComponent const& flags,
    MoveInputComponent& input, bool riding, bool inWater) {
    auto current = ll::service::getClientInstance();
    if (!current || &*current != client.get()) {
        origin(client, abilities, effects, flags, input, riding, inWater);
        return;
    }
    if ((intent.active() || sprintIntent.active()) && observed < 1000) ++observed;
    // Sneak and Sprint survive menus and focus changes; they only pause while
    // ineligible and end on death (dimension change and world exit cancel
    // them elsewhere).
    auto* player = current ? current->getLocalPlayer() : nullptr;
    if (player && !player->isAlive()) { intent.cancel(); sprintIntent.cancel(); }
    bool ready = current && eligible(*current);
    bool sneak = intent.active() && ready;
    bool sprint = sprintIntent.active() && ready;
    if (!sneak && !sprint) {
        origin(client, abilities, effects, flags, input, riding, inWater);
        return;
    }
    auto& raw = *input.mRawInputState;
    auto& rawFlags = raw.mFlagValues;
    RestoreRawFlags restore{
        raw,
        rawFlags->test(static_cast<size_t>(MoveInputState::Flag::SneakDown)),
        rawFlags->test(static_cast<size_t>(MoveInputState::Flag::SprintDown))
    };
    if (sneak) {
        if (matched < 1000) ++matched;
        rawFlags->set(static_cast<size_t>(MoveInputState::Flag::SneakDown));
    }
    if (sprint)
        rawFlags->set(static_cast<size_t>(MoveInputState::Flag::SprintDown));
    origin(client, abilities, effects, flags, input, riding, inWater);
    if (sneak && input.checkFlag(MoveInputComponent::Flag::Sneaking) && rawSneak < 1000) ++rawSneak;
}
LL_TYPE_INSTANCE_HOOK(SneakDimensionChange, ll::memory::HookPriority::Normal, LevelRendererPlayer,
    &LevelRendererPlayer::$onWillChangeDimension, void, Player& player) {
    intent.cancel();
    sprintIntent.cancel();
    origin(player);
}
}
void cancel() {
    if (intent.active()) Runtime::instance().self().getLogger().info(
        "Permanent Sneak stopped: updateCalls={} injectedUpdates={} sneakingSamples={}", observed, matched, rawSneak);
    intent.cancel();
}
void toggle(IClientInstance&) {
#ifdef LAMIUM_INPUT_UPDATE_PROBE
    Runtime::instance().self().getLogger().warn("Permanent Sneak is disabled in the input update probe build");
#else
    if (intent.active()) cancel();
    else { observed = matched = rawSneak = 0; intent.arm(); }
    Runtime::instance().self().getLogger().info("Permanent Sneak: {}", intent.active() ? "on" : "off");
#endif
}
bool active(IClientInstance& client) { return intent.active() && eligible(client); }
bool armed() {
#ifdef LAMIUM_INPUT_UPDATE_PROBE
    return false;
#else
    return intent.active();
#endif
}
void start() {
    try {
        if (!installed) {
            if (ApplyAutomationInput::hook(true) != 0) throw std::runtime_error("Could not install automation input hook");
            installed = true;
        }
        if (!dimensionInstalled) {
            if (SneakDimensionChange::hook(true) != 0) throw std::runtime_error("Could not install sneak dimension hook");
            dimensionInstalled = true;
        }
    } catch (...) { stop(); throw; }
}
void stop() {
    cancel();
    sprint::cancel();
    if (dimensionInstalled && SneakDimensionChange::unhook(true)) dimensionInstalled = false;
    if (installed && ApplyAutomationInput::unhook(true)) installed = false;
}
}
namespace lamium::interaction::sprint {
void cancel() {
    if (sneak::sprintIntent.active()) Runtime::instance().self().getLogger().info("Permanent Sprint stopped");
    sneak::sprintIntent.cancel();
}
void toggle(IClientInstance&) {
#ifdef LAMIUM_INPUT_UPDATE_PROBE
    Runtime::instance().self().getLogger().warn("Permanent Sprint is disabled in the input update probe build");
#else
    if (sneak::sprintIntent.active()) cancel();
    else sneak::sprintIntent.arm();
    Runtime::instance().self().getLogger().info("Permanent Sprint: {}", sneak::sprintIntent.active() ? "on" : "off");
#endif
}
bool active(IClientInstance& client) {
#ifdef LAMIUM_INPUT_UPDATE_PROBE
    (void)client;
    return false;
#else
    return sneak::sprintIntent.active() && sneak::eligible(client);
#endif
}
bool armed() {
#ifdef LAMIUM_INPUT_UPDATE_PROBE
    return false;
#else
    return sneak::sprintIntent.active();
#endif
}
}
