#include "spike_canvas.hpp"

#include <QObject>
#include <QImage>
#include <QColor>
#include <QRectF>
#include <QDir>
#include <QMatrix4x4>
#include <QCoreApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>

#include <cstddef>
#include <cmath>

struct Vertex { float x, y; float u, v; float r, g, b, a; };

inline void push_quad(std::vector<Vertex>& out, const QRectF& rect, const QRectF& uv, const QColor& tint) {
    const float x0 = rect.left(), y0 = rect.top(), x1 = rect.right(), y1 = rect.bottom();
    const float u0 = uv.left(), v0 = uv.top(); u1 = uv.right(), v1 = uv.bottom();
    const float r = tint.redF(), g = tint.greenF(), b = tint.blueF(), a = tint.alphaF();

    // triangle 1
    out.push_back({x0, y0, u0, v0, r, g, b, a});
    out.push_back({x1, y0, u1, v0, r, g, b, a});
    out.push_back({x1, y1, u1, v1, r, g, b, a});

    // triangle 2
    out.push_back({x0, y0, u0, v0, r, g, b, a});
    out.push_back({x1, y1, u1, v1, r, g, b, a});
    out.push_back({x0, y1, u0, v1, r, g, b, a});
}

SpikeCanvas::SpikeCanvas(QWidget* parent) : QOpenGLWidget(parent), scene_(HudTestScene::make(12345, 1000))
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

QImage SpikeCanvas::grab_whole_window() {return window()->grab().toImage();}

void SpikeCanvas::initializeGL() {
    initializeOpenGLFunctions();
    map_shader_.addShaderFromSourceFile(QOpenGLShader::Vertex, ":/mapShader.vert");
    map_shader_.addShaderFromSourceCode(QOpenGLShader::Fragment, ":/mapShader.frag");
    if (!map_shader_.link())
        qWarning() << "map shader:" << map_shader_.log();
    map_shader_.bind();
    map_shader_.setUniformValue("u_tex", 0);

    // background quad + one quad per token
    std::vector<Vertex> verts;
    // covers the 1280x720 world, UVs > 1 so the tile repeats
    push_quad(verts, QRectF(0, 0, 1280, 720), QRectF(0, 0, 1280 / 16.0, 720 / 16.0), Qt::white);
    for (const FakeToken& t : scene_.tokens) {
        QRectF r(t.world_x - 3, t.world_x - 3, 6, 6);
        // (0,0,1,1) UV on a white texel region, or use a 1x1 white texture
        push_quad(verts, r, QRectF(0, 0, 0, 0), Qt::white);
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

    QImage wTile(1, 1, QImage::Format_RGBA8888);
    wTile.setPixelColor(0, 0, QColor(255, 255, 255));
    white_tex_ = std::make_unique<QOpenGLTexture>(wTile);
    white_tex_->setMinMagFilters(QOpenGLTexture::Nearest, QOpenGLTexture::Nearest);
    white_tex_->setWrapMode(QOpenGLTexture::Repeat);

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
    vao.bind();
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
        camera_.zoom = (std::fmod(camera_.zoom - 1.0f + 3.0f, 3.0f)) + 1.0f;
    }
    else if (event->angleDelta().y() < 0) {
        camera_.zoom = (std::fmod(camera_.zoom, 3.0f)) + 1.0f;
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

void SpikeCanvas::mouseReleaseEvent(QMouseEvent *event) {
    dragging_ = false;
}

void SpikeCanvas::mouseMoveEvent(QMouseEvent* event) {
    auto pos = event->pos();
    if (dragging_) {

        camera_.pan_x += (last_mouse_pos_.x() - pos.x()) / camera_.zoom;
        camera_.pan_y += (last_mouse_pos_.y() - pos.y()) / camera_.zoom;
    }
    hud_input_.update_hover(pos);
    event->accept();
}

void saveImageToFolder(const QImage &image, const QString &fileName) {
    // 1. Build the path to the "images" folder next to your executable
    QString folderPath = QCoreApplication::applicationDirPath() + "/images";

    // 2. Create the directory if it doesn't already exist
    QDir dir(folderPath);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    // 3. Construct the full file path and save
    QString fullPath = dir.filePath(fileName); // e.g., ".../images/output.png"

    if (image.save(fullPath)) {
        qDebug() << "Saved successfully to:" << fullPath;
    } else {
        qDebug() << "Failed to save image.";
    }
}

// 1/2/3 switch HUD mode, N cycles nameplate counts, Z cycles zoom, B starts the benchmark, and G saves a screenshot
void SpikeCanvas::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_1) mode_ = HudMode::None;
    else if (event->key() == Qt::Key_2) mode_ = HudMode::GlDrawn;
    else if (event->key() == Qt::Key_3) mode_ = HudMode::QtOverlay;

    if (event->key() == Qt::Key_N) {nIndex = (nIndex + 1) % nameplate_opt.size(); nameplate_count_ = nameplate_opt[nIndex];}

    if (event->key() == Qt::Key_Z) camera_.zoom = std::fmod(camera_.zoom, 3.0f) + 1.0f;

    if (event->key() == Qt::Key_B) benchmark_running_ = true;

    if (event->key() == Qt::Key_G) saveImageToFolder(grab_whole_window(), "output" + std::to_string(++saveCount) + ".png");

}