#pragma once

#include <cstdint>

#include "hud_test_scene.hpp"

class GlHudInput {
public:
    using ElementId = std::uint32_t;

    void update_layout(const HudTestScene& scene, QSize canvas_size, qreal device_pixel_ratio);

    ElementId hit_test(QPoint pos) const;

    ElementId update_hover(QPoint pos);
};