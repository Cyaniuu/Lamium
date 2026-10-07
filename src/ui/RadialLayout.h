#pragma once
#include <algorithm>
#include <cmath>
#include <numbers>

namespace lamium::ui {
// A ring of `count` items around a center panel, the first at the top and
// going clockwise (the schematic menu, BACKLOG L-93). Centered at full size,
// or small in the lower right so the view stays free. GUI units; pure.
struct RadialLayout {
    static constexpr float itemWidth = 86, itemHeight = 22, centerWidth = 130, centerHeight = 46;
    float cx = 0, cy = 0, rx = 0, ry = 0, scale = 1;
    int count = 0;
    static RadialLayout at(float screenW, float screenH, int count, bool small) {
        RadialLayout l;
        l.count = std::max(1, count);
        l.scale = small ? .7f : 1.f;
        // The ring must clear its own items: wider for more of them.
        float spread = std::clamp(70.f + 9.f * l.count, 100.f, 150.f);
        l.rx = spread * 1.15f * l.scale;
        l.ry = spread * .78f * l.scale;
        float halfW = l.rx + itemWidth * l.scale / 2 + 4, halfH = l.ry + itemHeight * l.scale / 2 + 4;
        if (small) { l.cx = screenW - halfW - 8; l.cy = screenH - halfH - 40; }
        else { l.cx = screenW / 2; l.cy = screenH / 2; }
        l.cx = std::max(halfW, l.cx);
        l.cy = std::max(halfH, l.cy);
        return l;
    }
    float angle(int i) const { return static_cast<float>(i) * 2 * std::numbers::pi_v<float> / count - std::numbers::pi_v<float> / 2; }
    float itemX(int i) const { return cx + std::cos(angle(i)) * rx; }
    float itemY(int i) const { return cy + std::sin(angle(i)) * ry; }
    // The item the pointer points at, by direction from the center; -1 near
    // the center (where nothing is chosen).
    int hit(float x, float y) const {
        float dx = (x - cx) / rx, dy = (y - cy) / ry;
        if (std::hypot(dx, dy) < .45f) return -1;
        float a = std::atan2(dy, dx) + std::numbers::pi_v<float> / 2;
        if (a < 0) a += 2 * std::numbers::pi_v<float>;
        return static_cast<int>(std::lround(a / (2 * std::numbers::pi_v<float> / count))) % count;
    }
};
}
