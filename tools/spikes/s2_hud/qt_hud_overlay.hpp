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
    void set_nameplate_count(std::size_t count);          // create or delete plate widgets until exactly `count` exist
    void update_positions(const Camera& camera, QSize canvas_logical);   // call every tick
    void apply_zoom(float zoom);                          // resize and re-render every plate for a new zoom

signals:
    void hotbar_slot_clicked(int slot);
};