#pragma once
#include <algorithm>
#include <cmath>
#include <numbers>

namespace lamium::ui {
// A ring of `count` items around a center panel, the first at the top and
// going clockwise (the schematic menu, BACKLOG L-93). Centered, or in the
// lower right so the view stays free. The ring is sized for a full ring of
// the given item and center sizes, so no two items and no item and the
// center overlap, and it keeps its size and place on every level. GUI
// units; pure.
struct RadialLayout {
    static constexpr int maxCount = 8;
    static constexpr float gap = 5;
    float cx = 0, cy = 0, rx = 0, ry = 0;
    int count = 0;
    struct Sizes { float itemWidth, itemHeight, centerWidth, centerHeight; };
    // An item with a value line under its name, at a drawing scale.
    static constexpr float itemHeight(float scale) { return 26 * scale; }
    static RadialLayout at(float screenW, float screenH, int count, Sizes s, bool small) {
        RadialLayout l;
        l.count = std::clamp(count, 1, maxCount);
        constexpr float diagonal = std::numbers::sqrt2_v<float> / 2; // sin and cos of 45 degrees
        // Top and its diagonal neighbor apart sideways, the diagonal and the
        // side item apart vertically. An item at angle t clears the center
        // when rx|cos t| >= a or ry|sin t| >= b; on the ellipse that holds for
        // every t once (a/rx)^2 + (b/ry)^2 <= 1.
        float a = s.centerWidth / 2 + s.itemWidth / 2 + gap, b = s.centerHeight / 2 + s.itemHeight / 2 + gap;
        l.rx = std::max((s.itemWidth + gap) / diagonal, a * 1.25f);
        l.ry = std::max((s.itemHeight + gap) / diagonal, b / std::sqrt(1 - (a / l.rx) * (a / l.rx)));
        float halfW = l.rx + s.itemWidth / 2 + gap, halfH = l.ry + s.itemHeight / 2 + gap;
        if (small) { l.cx = screenW - halfW - 6; l.cy = screenH - halfH - 26; }
        else { l.cx = screenW / 2; l.cy = screenH / 2; }
        l.cx = std::max(halfW, l.cx);
        l.cy = std::max(halfH, l.cy);
        return l;
    }
    // Opening a level: items spread out from the center over `duration`
    // seconds, fast first. 0 at the start, 1 when settled.
    static constexpr float duration = .15f;
    static float spread(float seconds) {
        float t = std::clamp(seconds / duration, 0.f, 1.f);
        return 1 - (1 - t) * (1 - t) * (1 - t);
    }
    // Where item i is while the ring opens: from 55% of its radius outward.
    float itemX(int i, float progress) const { return cx + (itemX(i) - cx) * (.55f + .45f * progress); }
    float itemY(int i, float progress) const { return cy + (itemY(i) - cy) * (.55f + .45f * progress); }
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
