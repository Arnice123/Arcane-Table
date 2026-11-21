#include "spike_canvas.hpp"

#include <QCoreApplication>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QMatrix4x4>
#include <QKeyEvent>
#include <QObject>
#include <QImage>
#include <QColor>
#include <QRectF>
#include <QDir>
#include <QDebug>

#include <cstddef>
#include <algorithm>
#include <vector>
#include <cmath>

#include "gl_vertex.hpp"


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

Camera& SpikeCanvas::camera() { return camera_; }
FrameStats& SpikeCanvas::stats() {return stats_; }

void SpikeCanvas::set_hud_mode(HudMode mode) {
    mode_ = mode;
    const bool qt = (mode == HudMode::QtOverlay);

    overlay_->show_hud(qt);
    // Hidden Qt plates would still cost time in the None/GL runs, so only keep them in Qt mode.
    overlay_->set_nameplate_count(qt ? nameplate_count_ : 0);
    if (qt) {
        overlay_->apply_zoom(camera_.zoom);
        overlay_->update_positions(camera_, size());
    }

    stats_.reset();
    update();
}

void SpikeCanvas::set_nameplate_count(std::size_t count) {
    nameplate_count_ = count;

    if (mode_ == HudMode::QtOverlay) {
        overlay_->set_nameplate_count(count);
        overlay_->apply_zoom(camera_.zoom);
        overlay_->update_positions(camera_, size());
    }

    stats_.reset();
    update();
}

void SpikeCanvas::set_zoom(float zoom) {
    camera_.zoom = std::clamp(zoom, 1.0f, 3.0f);

    if (mode_ == HudMode::QtOverlay) {
        overlay_->apply_zoom(camera_.zoom);
        overlay_->update_positions(camera_, size());
    }

    stats_.reset();
    update();
}

QImage SpikeCanvas::grab_whole_window() {return window()->grab().toImage();}

void SpikeCanvas::initializeGL() {
    initializeOpenGLFunctions();
    map_shader_.addShaderFromSourceFile(QOpenGLShader::Vertex, ":/mapShader.vert");
    map_shader_.addShaderFromSourceFile(QOpenGLShader::Fragment, ":/mapShader.frag");
    if (!map_shader_.link())
        qWarning() << "map shader:" << map_shader_.log();
    map_shader_.bind();
    map_shader_.setUniformValue("u_tex", 0);

    // background quad + one quad per token
    std::vector<Vertex> verts;
    // covers the 1280x720 world, UVs > 1 so the tile repeats
    push_quad(verts, QRectF(0, 0, 1280, 720), QRectF(0, 0, 1280 / 16.0, 720 / 16.0), Qt::white);
    for (const FakeToken& t : scene_.tokens) {
        QRectF r(t.world_x - 3, t.world_y - 3, 6, 6);
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
    vao_.bind();
    glDrawArrays(GL_TRIANGLES, 0, 6);

    white_tex_->bind();
    glDrawArrays(GL_TRIANGLES, 6, map_vertex_count_ - 6);

    if (mode_ == HudMode::GlDrawn) {
        hud_renderer_.draw(scene_, camera_, layout_, size(), dpr, nameplate_count_);
        hud_input_.update_layout(layout_, size());
    }

    stats_.end_paint();
}

void SpikeCanvas::resizeGL(int w, int h) {
    layout_ = HudLayout::compute(size());
    if (overlay_) overlay_->relayout(layout_,size());
}

void SpikeCanvas::wheelEvent(QWheelEvent* event) {
    const int dy = event->angleDelta().y();
    if (dy > 0)      set_zoom(camera_.zoom + 1.0f);
    else if (dy < 0) set_zoom(camera_.zoom - 1.0f);
    event->accept();
}

void SpikeCanvas::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) return;
    if (mode_ == HudMode::GlDrawn) {
        if (auto hit = hud_input_.hit_test(event->pos()); hit != 0) {
            qDebug() << "HUD element clicked:" << hit;
            return;
        }
    }
    dragging_ = true;
    last_mouse_pos_ = event->pos();
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
    if (mode_ == HudMode::GlDrawn) hud_input_.update_hover(pos);
    else if (mode_ == HudMode::QtOverlay) overlay_->update_positions(camera_, size());
    last_mouse_pos_ = pos;
    event->accept();
    update();
}

static void saveImageToFolder(const QImage &image, const QString &fileName) {
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
// ---- keys: no logic of their own ----
void SpikeCanvas::keyPressEvent(QKeyEvent* event) {
    switch (event->key()) {
    case Qt::Key_1: set_hud_mode(HudMode::None);       break;
    case Qt::Key_2: set_hud_mode(HudMode::QtOverlay);  break;
    case Qt::Key_3: set_hud_mode(HudMode::GlDrawn);    break;

    case Qt::Key_N:
        nIndex = (nIndex + 1) % nameplate_opt.size();
        set_nameplate_count(nameplate_opt[nIndex]);
        break;

    case Qt::Key_Z:
        set_zoom(float(int(camera_.zoom) % 3 + 1));   // 1 -> 2 -> 3 -> 1
        break;

    case Qt::Key_B:
        benchmark_running_ = !benchmark_running_;
        if (benchmark_running_) tick();
        break;

    case Qt::Key_G:
        saveImageToFolder(grab_whole_window(),"output" + QString::number(++saveCount) + ".png");
        break;

    default:
        QOpenGLWidget::keyPressEvent(event);
    }
}

void SpikeCanvas::tick() {
    if (benchmark_running_) {
        stats_.begin_frame();
        camera_.pan_x += 0.1f;
        camera_.pan_x -= 0.1f;
        if (mode_ == HudMode::QtOverlay) overlay_->update_positions(camera_, size());
        update();
    }
}