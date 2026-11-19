#pragma once

#include <QOpenGLWidget>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QWidget>
#include <QPixmap>
#include <QImage>

#include "hud_test_scene.hpp"
#include "frame_stats.hpp"

class SpikeCanvas : public QOpenGLWidget {
    Q_OBJECT
public:
    enum class HudMode { None, QtOveraly, GLDrawn };

    explicit SpikeCanvas(QWidget* parent = nullptr);

    void set_hud_mode(HudMode mode);

    void set_nameplate_count(std::size_t count);

    Camera& camera();
    FrameStats& stats();

    QImage grab_whole_window();
protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;
    void wheelEvent(QWheelEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
private:
    Camera camera_;
    FrameStats stats_;
    QWidget* parent_;
    HudMode mode_ = HudMode::None;
    std::size_t count_;
};
