#include "qt_hud_overlay.hpp"

#include <algorithm>

QtHudOverlay::QtHudOverlay(QWidget* canvas, const HudTestScene& scene): QObject(nullptr), scene_(scene) {
    container_ = new QWidget(canvas);
    container_->resize(canvas->size());
    container_->hide();
    layout_ = HudLayout::compute(canvas->size());
    for (std::size_t i = 0; i < layout_.hotbar_slots.size(); ++i) {
        auto n = std::make_unique<QPushButton>(container_);

        n->setGeometry(layout_.hotbar_slots[i].toRect());
        n->setText(QString::fromStdString(scene.hotbar[i]));
        n->setFocusPolicy(Qt::NoFocus);
        connect(n.get(), &QPushButton::clicked, this, [this, i] {
            emit hotbar_slot_clicked(int(i + 1));
        });
        hotBars_.push_back(std::move(n));
    }
    combatStrip_ = std::make_unique<QLabel>(container_);
    combatStrip_->setGeometry(layout_.combat_strip.toRect());
    combatStrip_->setText(QString::fromStdString(scene.combat_text));
}

QtHudOverlay::~QtHudOverlay() {
    hotBars_.clear();
    combatStrip_.reset();
    plates_.clear();
    delete container_;
}

void QtHudOverlay::show_hud(bool visible) {
    if (visible) container_->show();
    else container_->hide();
}

void QtHudOverlay::set_nameplate_count(std::size_t count) {
    count = std::clamp<std::size_t>(count, 0, scene_.tokens.size());
    if (plates_.size() > count) { // plates to remove
        plates_.erase(plates_.begin() + count, plates_.end());
    }
    else
    {
        QSize s = (layout_.nameplate_rect.size() * zoom_).toSize();
        for (std::size_t i = plates_.size(); i < count; ++i) {
            plates_.push_back(std::make_unique<NameplateWidget>(container_));
            const FakeToken& t = scene_.tokens[i];
            plates_[i]->set_data(QString::fromStdString(t.name), t.hp_frac);
            plates_[i]->resize(s);
            plates_[i]->set_zoom(zoom_);
        }
    }
}

void QtHudOverlay::update_positions(const Camera& camera, QSize canvas_logical) {
    QRect canvas_rect(QPoint(0, 0), canvas_logical);
    for (std::size_t i = 0; i < plates_.size(); ++i) {
        const FakeToken& t = scene_.tokens[i];
        QPointF s = camera.world_to_screen({t.world_x, t.world_y}, canvas_logical);
        s += layout_.nameplate_rect.topLeft() * camera.zoom;
        auto sP = s.toPoint();
        plates_[i]->move(sP);
        const bool should_show = QRect(sP, plates_[i]->size()).intersects(canvas_rect);
        if (plates_[i]->isHidden() == should_show) plates_[i]->setVisible(should_show);
    }
}

void QtHudOverlay::relayout(const HudLayout& layout, QSize canvas_size) {
    container_->resize(canvas_size);
    layout_ = layout;

    for (std::size_t i = 0; i < layout_.hotbar_slots.size(); ++i) {
        hotBars_[i]->setGeometry(layout_.hotbar_slots[i].toRect());
    }
    combatStrip_->setGeometry(layout_.combat_strip.toRect());
}

void QtHudOverlay::apply_zoom(float zoom) {
    zoom_ = zoom;
    QSize s = (layout_.nameplate_rect.size() * zoom_).toSize();
    for (std::size_t i = 0; i < plates_.size(); ++i) {
        plates_[i]->resize(s);
        plates_[i]->set_zoom(zoom_);
    }
}