#include "features/lighting/NightVision.h"
#include "app/Runtime.h"
#include "app/Versions.h"
#include "ll/api/memory/Hook.h"
#include "mc/client/renderer/ptexture/BaseLightData.h"
#include "mc/client/renderer/ptexture/BaseLightTextureImageBuilder.h"
#include "mc/client/world/level/dimension/NetherLightTextureImageBuilder.h"

namespace lamium {
namespace {
LL_TYPE_INSTANCE_HOOK(BaseLightHook, ll::memory::HookPriority::Normal, BaseLightTextureImageBuilder,
    &BaseLightTextureImageBuilder::$createBaseLightTextureData, std::unique_ptr<BaseLightData>,
    IClientInstance* client, BaseLightData const& previous) {
    auto data = origin(client, previous);
    NightVision::instance().apply(data.get());
    return data;
}
#if !defined(LAMIUM_INPUT_UPDATE_PROBE) && !defined(LAMIUM_SDK_26_20_7)
LL_TYPE_INSTANCE_HOOK(NetherLightHook, ll::memory::HookPriority::Normal, NetherLightTextureImageBuilder,
    &NetherLightTextureImageBuilder::$createBaseLightTextureData, std::unique_ptr<BaseLightData>,
    IClientInstance* client, BaseLightData const& previous) {
    auto data = origin(client, previous);
    NightVision::instance().apply(data.get());
    return data;
}
#endif
#if defined(LAMIUM_SDK_26_20_7)
LL_TYPE_INSTANCE_HOOK(NightVisionImageHook, ll::memory::HookPriority::Normal, BaseLightTextureImageBuilder,
    &BaseLightTextureImageBuilder::$buildImage, bool, BaseLightData const& lightData, mce::Image* targetImage,
    uint imageLength, float a, float ambientBoost, bool clampToMinimum) {
    if (!NightVision::instance().enabled() || !versionSensitiveAllowed(capabilities::Id::NightVision))
        return origin(lightData, targetImage, imageLength, a, ambientBoost, clampToMinimum);
    BaseLightData adjusted{lightData};
    NightVision::instance().apply(&adjusted);
    return origin(adjusted, targetImage, imageLength, a, ambientBoost, clampToMinimum);
}
#endif
bool baseInstalled = false;
#if !defined(LAMIUM_INPUT_UPDATE_PROBE) && !defined(LAMIUM_SDK_26_20_7)
bool netherInstalled = false;
#endif
#if defined(LAMIUM_SDK_26_20_7)
bool imageInstalled = false;
#endif
}
NightVision& NightVision::instance() { static NightVision instance; return instance; }
bool NightVision::start() {
    if (running) return true;
    capabilities::begin(capabilities::Id::NightVision);
    try {
        if (!baseInstalled) baseInstalled = BaseLightHook::hook() == 0;
        if (!baseInstalled) { capabilities::hooks(capabilities::Id::NightVision, false); stop(); return false; }
#if !defined(LAMIUM_INPUT_UPDATE_PROBE) && !defined(LAMIUM_SDK_26_20_7)
        if (!netherInstalled) netherInstalled = NetherLightHook::hook() == 0;
        if (!netherInstalled) { capabilities::hooks(capabilities::Id::NightVision, false); stop(); return false; }
#endif
#if defined(LAMIUM_SDK_26_20_7)
        if (!imageInstalled) imageInstalled = NightVisionImageHook::hook() == 0;
        if (!imageInstalled) { capabilities::hooks(capabilities::Id::NightVision, false); stop(); return false; }
#endif
        running = true;
        capabilities::ready(capabilities::Id::NightVision);
        return true;
    } catch (std::exception const& error) {
        capabilities::hooks(capabilities::Id::NightVision,false);
        Runtime::instance().self().getLogger().error("Lighting initialization failed: {}", error.what());
        stop();
        return false;
    }
}
void NightVision::stop() {
    capabilities::stop(capabilities::Id::NightVision);
    running = false;
#if !defined(LAMIUM_INPUT_UPDATE_PROBE) && !defined(LAMIUM_SDK_26_20_7)
    if (netherInstalled && NetherLightHook::unhook()) netherInstalled = false;
#endif
#if defined(LAMIUM_SDK_26_20_7)
    if (imageInstalled && NightVisionImageHook::unhook()) imageInstalled = false;
#endif
    if (baseInstalled && BaseLightHook::unhook()) baseInstalled = false;
    if (baseInstalled
#if !defined(LAMIUM_INPUT_UPDATE_PROBE) && !defined(LAMIUM_SDK_26_20_7)
        || netherInstalled
#endif
#if defined(LAMIUM_SDK_26_20_7)
        || imageInstalled
#endif
    ) {
        capabilities::fail(capabilities::Id::NightVision,capabilities::Reason::RemovalFailed);
        Runtime::instance().self().getLogger().error("Could not remove a lighting hook");
    }
}
void NightVision::apply(BaseLightData* data) const {
    if (!data || !enabled() || !versionSensitiveAllowed(capabilities::Id::NightVision)) return;
    // Modify newly produced render data so the game's light-texture cache sees
    // the change on both enable and disable. No player effect is added.
    data->mNightvisionActive = true;
    data->mNightvisionScale = 1.0f;
    if (data->mUnderwaterVision) data->mUnderwaterScale = 1.0f;
    data->mDarkenWorldAmount = 0.0f;
    data->mPreviousDarkenWorldAmount = 0.0f;
    data->mDarknessFactor = 0.0f;
    data->mDarknessFactorPreviousFrame = 0.0f;
    // A low Brightness darkens the light curve's low end, which Night Vision
    // shows as dark rings around light sources (L-121). The texture is built
    // with full gamma; the Brightness option itself stays as the player set it.
    float gamma = data->mGamma;
    if (gamma != loggedGamma.exchange(gamma))
        try { Runtime::instance().self().getLogger().info("Night Vision: light gamma {} ({})", gamma, even.load() ? "even -> 1" : "vanilla"); } catch (...) {}
    if (even.load() && gamma < 1.0f) data->mGamma = 1.0f;
}
}
