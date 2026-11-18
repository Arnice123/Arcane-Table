#pragma once

#include <QObject>
#include <QSize>

#include "spike_canvas.hpp"
#include "hud_test_scene.hpp"

class QtHudOverlay : public QObject {
    Q_OBJECT
public:
    QtHudOverlay(SpikeCanvas* canvas, const HudTestScene& scene);

    void show_hud(bool visible);

    void set_nameplate_count(std::size_t count);

    void update_positions(const Camera& camera, QSize canvas_size);
signals:
    void hotbar_slot_clicked(int slot);
};