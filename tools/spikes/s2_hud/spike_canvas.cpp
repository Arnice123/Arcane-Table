#include "spike_canvas.hpp"

SpikeCanvas::SpikeCanvas(QWidget* parent) : count_(0) {
    camera_ = Camera{0, 0, 1};
    stats_ = FrameStats{};
    parent_ = parent;
}

void SpikeCanvas::set_hud_mode(HudMode mode) {mode_ = mode;}
void SpikeCanvas::set_nameplate_count(std::size_t count) {count_ = count;}
Camera& SpikeCanvas::camera() { return camera_; }
FrameStats& SpikeCanvas::stats() {return stats_; }

QImage SpikeCanvas::grab_whole_window() {return parent_->window()->grab().toImage();}

void SpikeCanvas::initializeGL() {}
void SpikeCanvas::paintGL() {}
void SpikeCanvas::resizeGL(int w, int h) {}
void SpikeCanvas::wheelEvent(QWheelEvent*) {}
void SpikeCanvas::mousePressEvent(QMouseEvent*) {}
void SpikeCanvas::mouseMoveEvent(QMouseEvent*) {}