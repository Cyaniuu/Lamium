#include "ui/VanillaHudVisibility.h"
#include "mc/client/gui/GuiData.h"
#include "mc/util/HudElement.h"

namespace lamium::ui {
bool vanillaHudHidden(GuiData const& gui) {
    for (int i = 0; i < static_cast<int>(HudElement::Count); ++i)
        if (gui.isHudElementVisible(static_cast<HudElement>(i))) return false;
    return true;
}
}
