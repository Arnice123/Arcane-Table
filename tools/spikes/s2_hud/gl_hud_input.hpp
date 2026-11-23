#pragma once

#include <cstdint>

#include "hud_test_scene.hpp"

class GlHudInput {
public:
    // Identifies a HUD element. 0 means "nothing: the click belongs to the map".
    using ElementId = std::uint32_t;

    // In: the same layout the renderer used.
    // Outcome: hit-testing data updated
    void update_layout(const HudLayout& layout, QSize canvas_logical);

    // In: a position.
    // Out: an element id, or 0 for no hit.
    ElementId hit_test(QPoint pos) const;
private:
    std::vector<std::pair<QRectF, ElementId>> entries;
};