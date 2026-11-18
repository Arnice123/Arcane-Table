#pragma once

#include <QOpenGLFunctions_3_3_Core>
#include <QSize>

#include "hud_test_scene.hpp"

class GLHudRenderer {
public:
    void init(QOpenGLFunctions_3_3_Core* gl);

    void draw(const HudTestScene& scene, const Camera& camera, QSize canvas_size, qreal devicel_pixel_ratio, std::size_t nameplate_count);

    void shutdown();
};