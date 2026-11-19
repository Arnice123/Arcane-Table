#pragma once

#include <cstdint>

#include "hud_test_scene.hpp"

class GlHudInput {
public:
    using ElementId = std::uint32_t;                       // 0 = nothing, map click
    void update_layout(const HudLayout& layout, QSize canvas_logical);   // rebuild the list of clickable rectangles
    ElementId hit_test(QPoint pos) const;                  // click position -> id
    ElementId update_hover(QPoint pos);                    // call on mouse move; returns the newly hovered id (renderer draws a highlight)
};