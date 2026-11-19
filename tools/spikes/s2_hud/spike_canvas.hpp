#pragma once

#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLTexture>

#include <QOpenGLWidget>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QWidget>
#include <QPixmap>
#include <QImage>

#include "hud_test_scene.hpp"
#include "frame_stats.hpp"
#include "qt_hud_overlay.hpp"
#include "gl_hud_renderer.hpp"
#include "gl_hud_input.hpp"

class SpikeCanvas : public QOpenGLWidget {
    Q_OBJECT
public:
    enum class HudMode { None, QtOverlay, GlDrawn };

    explicit SpikeCanvas(QWidget* parent = nullptr);
    void set_hud_mode(HudMode mode);

    // 0, 50, 200, 1000
    void set_nameplate_count(std::size_t count);

    // 1, 2 or 3
    void set_zoom(float zoom);
    Camera& camera();
    FrameStats& stats();
    // screenshot including child widgets
    QImage grab_whole_window();

public slots:
    // one iteration of the frame loop
    void tick();

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
private:
    HudTestScene scene_;
    Camera camera_;
    HudLayout layout_;

    FrameStats stats_;

    std::unique_ptr<QtHudOverlay> overlay_;
    GlHudRenderer hud_renderer_;
    GlHudInput hud_input_;

    QOpenGLShaderProgram map_shader_;
    QOpenGLBuffer map_vbo_{QOpenGLBuffer::VertexBuffer};
    QOpenGLVertexArrayObject vao_;
    std::unique_ptr<QOpenGLTexture> background_tex_;
    std::unique_ptr<QOpenGLTexture> white_tex_;
    int map_vertex_count_ = 0;
    int background_vertex_count_ = 6;

    HudMode mode_ = HudMode::None;
    std::size_t nameplate_count_ = 0;

    bool dragging_ = false;
    QPointF last_mouse_pos_;

    bool benchmark_running_ = false;
};