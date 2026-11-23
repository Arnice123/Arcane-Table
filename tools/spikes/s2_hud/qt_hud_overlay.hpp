#pragma once

#include <QObject>
#include <QWidget>
#include <QSize>
#include <QPushButton>
#include <QLabel>
#include <QFont>

#include <vector>
#include <memory>

#include "hud_test_scene.hpp"
#include "nameplate.hpp"

class QtHudOverlay : public QObject {
    Q_OBJECT
public:
    QtHudOverlay(QWidget* canvas, const HudTestScene& scene);
    ~QtHudOverlay();

    void show_hud(bool visible);
    void set_nameplate_count(std::size_t count);          // create or delete plate widgets until exactly `count` exist
    void update_positions(const Camera& camera, QSize canvas_logical);   // call every tick
    void relayout(const HudLayout& layout, QSize canvas_size);
    void apply_zoom(float zoom);                          // resize and re-render every plate for a new zoom

signals:
    void hotbar_slot_clicked(int slot);

private:
    HudLayout layout_;
    const HudTestScene& scene_;
    std::vector<std::unique_ptr<QPushButton>> hotBars_;
    std::unique_ptr<QLabel> combatStrip_;
    QWidget* container_;
    std::vector<std::unique_ptr<NameplateWidget>> plates_;
    float zoom_ = 1.0f;

};