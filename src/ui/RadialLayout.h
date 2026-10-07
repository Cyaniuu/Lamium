#pragma once
#include <algorithm>
#include <cmath>
#include <numbers>

namespace lamium::ui {
// A ring of `count` items around a center panel, the first at the top and
// going clockwise (the schematic menu, BACKLOG L-93). Centered, or in the
// lower right so the view stays free. The center stays put whatever the
// count, so switching levels never moves the menu. GUI units; pure.
struct RadialLayout {
    static constexpr int maxCount = 8;
    static constexpr float itemHeight = 26, gap = 6;
    float cx = 0, cy = 0, rx = 0, ry = 0;
    int count = 0;
    // `itemWidth` is the widest item's width; `small` shrinks the ring.
    static RadialLayout at(float screenW, float screenH, int count, float itemWidth, bool small) {
        RadialLayout l;
        l.count = std::clamp(count, 1, maxCount);
        float scale = small ? .72f : 1.f;
        // Wide enough that neighbors on a full ring clear each other, but
        // never wider than the screen allows.
        l.rx = std::min(scale * std::max(150.f, itemWidth * 1.35f), screenW / 2 - itemWidth / 2 - gap);
        l.ry = std::min(scale * 104.f, screenH / 2 - itemHeight / 2 - 40);
        l.rx = std::max(l.rx, 40.f);
        l.ry = std::max(l.ry, 30.f);
        float halfW = l.rx + itemWidth / 2 + gap, halfH = l.ry + itemHeight / 2 + gap;
        if (small) { l.cx = screenW - halfW - 8; l.cy = screenH - halfH - 26; }
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
