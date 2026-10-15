#include "ui/Animations.h"
#include "app/Runtime.h"
#include "mc/client/game/IClientInstance.h"
#if !defined(LAMIUM_INPUT_UPDATE_PROBE) && !defined(LAMIUM_SDK_26_20_7)
#include "mc/client/options/IOptionRegistry.h"
#endif

namespace lamium::ui {
bool animationsOn(IClientInstance& client) {
    auto mode = Runtime::instance().snapshot()->ui.animations;
    if (mode == 1) return true;
    if (mode == 2) return false;
#if defined(LAMIUM_INPUT_UPDATE_PROBE) || defined(LAMIUM_SDK_26_20_7)
    (void)client;
    return true;
#else
    try { return client.getOptions().getScreenAnimations(); } catch (...) { return true; }
#endif
}
}
