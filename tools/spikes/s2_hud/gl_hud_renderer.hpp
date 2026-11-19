#pragma once

#include <QOpenGLFunctions_3_3_Core>
#include <QSize>

#include "hud_test_scene.hpp"

class GlHudRenderer {
public:
    void init(QOpenGLFunctions_3_3_Core* gl);
    void draw(const HudTestScene& scene, const Camera& camera, const HudLayout& layout, QSize canvas_logical, qreal device_pixel_ratio, std::size_t nameplate_count);
    void shutdown();
};