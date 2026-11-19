#include "spike_canvas.hpp"

#include <QObject>
#include <QMatrix4x4>

struct Vertex { float x, y; float u, v; float r, g, b, a; };

SpikeCanvas::SpikeCanvas(QWidget* parent) : QOpenGLWidget(parent), scene_(HudTestScene::make(12345, 1000)
{
    setMinimumSize(1280, 720);
    resize(1280, 720);

    // make keyPressEvent fires
    setFocusPolicy(Qt::StrongFocus);

    // enable for hover highlights
    setMouseTracking(true);

    // the overlay's widgets are children of this canvas.
    overlay_ = std::make_unique<QtHudOverlay>(this, scene_);
    overlay_->show_hud(false);
    connect(overlay_.get(), &QtHudOverlay::hotbar_slot_clicked,
            this, [](int slot) { qDebug() << "Qt hotbar slot clicked:" << slot; });
}

void SpikeCanvas::set_hud_mode(HudMode mode) {mode_ = mode;}
void SpikeCanvas::set_nameplate_count(std::size_t count) {count_ = count;}
Camera& SpikeCanvas::camera() { return camera_; }
FrameStats& SpikeCanvas::stats() {return stats_; }

QImage SpikeCanvas::grab_whole_window() {return parent_->window()->grab().toImage();}

void SpikeCanvas::initializeGL() {
    initializeOpenGLFunctions();
    map_shader_.addShaderFromSourceFile(QOpenGLShader::Vertex, "mapShader.vert");
    map_shader_.addShaderFromSourceCode(QOpenGLShader::Fragment, "mapShader.frag");
    if (!map_shader_.link())
        qWarning() << "map shader:" << map_shader_.log();
    map_shader_.bind();
    map_shader_.setUniformValue("u_tex", 0);

    // background quad + one quad per token
    std::vector<Vertex> verts;
    // covers the 1280x720 world, UVs > 1 so the tile repeats
    push_quad(verts, QRectF(0, 0, 1280, 720), QRectF(0, 0, 1280 / 16.0, 720 / 16.0), Qt::white);
    for (const Token& t : scene_.tokens) {
        QRectF r(t.pos.x() - 3, t.pos.y() - 3, 6, 6);
        // (0,0,1,1) UV on a white texel region, or use a 1x1 white texture
        push_quad(verts, r, QRectF(0, 0, 0, 0), t.color);
    }
    map_vertex_count_ = int(verts.size());

    // VAO records the attribute layout so paintGL only has to bind it
    vao_.create();
    vao_.bind();

    map_vbo_.create();
    map_vbo_.bind();
    map_vbo_.setUsagePattern(QOpenGLBuffer::StaticDraw);
    map_vbo_.allocate(verts.data(), int(verts.size() * sizeof(Vertex)));

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, x));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, u));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, r));

    vao_.release();

    // 4. Background texture: small checkerboard, nearest filtering, repeating
    QImage tile(16, 16, QImage::Format_RGBA8888);
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x)
            tile.setPixelColor(x, y, ((x / 8 + y / 8) % 2) ? QColor(60, 60, 60) : QColor(90, 90, 90));
    background_tex_ = std::make_unique<QOpenGLTexture>(tile);
    background_tex_->setMinMagFilters(QOpenGLTexture::Nearest, QOpenGLTexture::Nearest);
    background_tex_->setWrapMode(QOpenGLTexture::Repeat);

    // 5. HUD renderer
    hud_renderer_.init(this);

    connect(this, &QOpenGLWidget::frameSwapped, this, [this] {
        stats_.frame_presented();
        if (benchmark_running_)
            tick();
    });

    // destructor
    connect(context(), &QOpenGLContext::aboutToBeDestroyed, this, [this] {
        makeCurrent();
        background_tex_.reset();
        white_tex_.reset();
        map_vbo_.destroy();
        vao_.destroy();
        hud_renderer_.shutdown();
        doneCurrent();
    });
}

void SpikeCanvas::paintGL() {
    stats_.begin_paint();

    const qreal dpr = devicePixelRatioF();
    glViewport(0, 0, int(width() * dpr), int(height() * dpr));
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Camera matrix: world -> clip space
    QMatrix4x4 m;
    m.ortho(0, width(), height(), 0, -1, 1);
    m.translate(width() / 2.0f, height() / 2.0f);
    m.scale(camera_.zoom, camera_.zoom);
    m.translate(-camera_.pan_x, -camera_.pan_y);

    map_shader_.bind();
    map_shader_.setUniformValue("u_camera", m);
    background_tex_->bind();

    map_vbo_.bind();
    glDrawArrays(GL_TRIANGLES, 0, map_vertex_count_);

    if (mode_ == HudMode::GlDrawn) {
        hud_renderer_.draw(scene_, camera_, layout_, size(), dpr, nameplate_count_);
        hud_input_.update_layout(layout_, size());
    }

    stats_.end_paint();
}

void SpikeCanvas::resizeGL(int w, int h) {
    layout_ = HudLayout::compute(size());
    if (overlay_) overlay_->relayout(layout_);
}

void SpikeCanvas::wheelEvent(QWheelEvent* event) {
    if (event->angleDelta().y() > 0) {
        camera_.zoom = ((camera_.zoom - 1 + 3) % 3) + 1;
    }
    else if (event->angleDelta().y() < 0) {
        camera_.zoom = (camera_.zoom % 3) + 1;
    }
    overlay_->apply_zoom(camera_.zoom);
    event->accept();
}

void SpikeCanvas::mousePressEvent(QMouseEvent* event) {
    auto hit = hud_input_.hit_test(event->pos());
    if (hit == 0) event->ignore();
    dragging_ = true;
    last_mouse_pos_ = event->pos();
    event->accept();
}

void SpikeCanvas::mouseMoveEvent(QMouseEvent*) {}

void SpikeCanvas::keyPressEvent(QKeyEvent*) {}