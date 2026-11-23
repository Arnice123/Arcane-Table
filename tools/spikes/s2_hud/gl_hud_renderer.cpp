#include "gl_hud_renderer.hpp"

#include <QImage>
#include <QChar>
#include <QFont>
#include <QDebug>
#include <QPainter>
#include <QMatrix4x4>
#include <QFontMetricsF>

#include <cmath>
#include <cstddef>

void GlHudRenderer::init(QOpenGLFunctions_3_3_Core* gl) {
    gl_ = gl;
    vao.create(); vao.bind();
    vbo.create(); vbo.bind();
    vbo.setUsagePattern(QOpenGLBuffer::DynamicDraw);
    vbo.allocate(nullptr, max_quads * 6 * int(sizeof(Vertex)));
    gl_->glEnableVertexAttribArray(0);
    gl_->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, x));
    gl_->glEnableVertexAttribArray(1);
    gl_->glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, u));
    gl_->glEnableVertexAttribArray(2);
    gl_->glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, r));
    vao.release();

    if (!shader.addShaderFromSourceFile(QOpenGLShader::Vertex, ":/mapShader.vert")) qWarning() << "ERROR::VERTEX::SHADER:" << shader.log();
    if (!shader.addShaderFromSourceFile(QOpenGLShader::Fragment, ":/mapShader.frag")) qWarning() << "ERROR::FRAGMENT::SHADER:" << shader.log();
    if (!shader.link()) qWarning() << "map shader:" << shader.log();

    shader.bind();
    shader.setUniformValue("u_tex", 0);

    constexpr int dim = 128;
    auto uv = [](const QRect& px) {return QRectF(double(px.left()) / dim, double(px.top()) / dim, double(px.width()) / dim, double(px.height()) / dim);};

    QImage atlas_img(dim, dim, QImage::Format_ARGB32);
    atlas_img.fill(Qt::transparent);

    QPainter p(&atlas_img);

    QFont f("Arial");
    f.setPixelSize(10);
    f.setStyleStrategy(QFont::NoAntialias);
    p.setFont(f);
    p.setRenderHint(QPainter::TextAntialiasing, false);
    p.setPen(Qt::white);

    QFontMetricsF fm(f);
    const int cell_h = int(std::ceil(fm.height()));
    const int ascent = int(std::ceil(fm.ascent()));

    constexpr int last = 126;
    textGlyph.clear();
    textGlyph.reserve(last - first + 1);

    int cx = 0, cy = 0;
    for (int i = first; i <= last; ++i) {
        const QChar ch(static_cast<ushort>(i));
        const int w = int(std::ceil(fm.horizontalAdvance(ch)));

        if (cx + w > dim) {          // wrap to the next row
            cx = 0;
            cy += cell_h + 1;
        }
        if (cy + cell_h > dim) { qWarning() << "glyph atlas too small"; break; }

        p.drawText(QPoint(cx, cy + ascent), QString(ch));

        textGlyph.push_back({uv(QRect(cx, cy, w, cell_h)), w, cell_h, w + 1});

        cx += w + 1;
    }

    const int ui_top = cy + cell_h + 2;
    if (ui_top + 16 > dim) qWarning() << "atlas too small for UI cells";

    // solid white block; sample from its inset middle so nothing bleeds in
    p.fillRect(QRect(0, ui_top, 4, 4), Qt::white);
    white_uv_ = uv(QRect(1, ui_top + 1, 2, 2));

    // 12x12 frame (corner size 4)
    constexpr int frame = 12;
    const int fx = 6;
    p.fillRect(QRect(fx, ui_top, 12, 12), QColor(30, 30, 30));
    p.fillRect(QRect(fx + 1, ui_top + 1, 10, 10), QColor(200, 200, 200));
    p.fillRect(QRect(fx + 2, ui_top + 2,  8,  8), QColor(60, 60, 80));
    frame_uv_ = uv(QRect(fx, ui_top, frame, frame));

    // 16x16 black/white checkerboard icon
    const int ix = fx + frame + 1;
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x)
            p.fillRect(QRect(ix + x, ui_top + y, 1, 1), ((x + y) & 1) ? Qt::white : Qt::black);
    icon_uv_ = uv(QRect(ix, ui_top, 16, 16));

    p.end();

    atlas = std::make_unique<QOpenGLTexture>(atlas_img,QOpenGLTexture::DontGenerateMipMaps);
    atlas->setMinMagFilters(QOpenGLTexture::Nearest, QOpenGLTexture::Nearest);
    atlas->setWrapMode(QOpenGLTexture::ClampToEdge);

    shader.release();
}

void GlHudRenderer::draw(const HudTestScene& scene, const Camera& camera, const HudLayout& layout, QSize canvas_logical, std::size_t nameplate_count, GlHudInput::ElementId eleId) {
    shader.bind();
    vbo.bind();
    vao.bind();
    atlas->bind();

    gl_->glEnable(GL_BLEND);
    gl_->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    world_verts.clear();
    screen_verts.clear();

    // ---- text helpers ----
    auto push_text = [this](std::vector<Vertex>& out, const std::string& str, float x, float y, float max_x) {
        for (char c : str) {
            const int idx = c - first;
            if (idx < 0 || idx >= int(textGlyph.size())) continue;
            const GlyphInfo& gi = textGlyph[idx];
            if (x + gi.width > max_x) break;
            push_quad(out, QRectF(x, y, gi.width, gi.height), gi.uv, Qt::white);
            x += gi.advance;
        }
    };

    auto text_width = [this](const std::string& str) {
        int w = 0;
        for (char c : str) {
            const int idx = c - first;
            if (idx >= 0 && idx < int(textGlyph.size())) w += textGlyph[idx].advance;
        }
        return w > 0 ? w - 1 : 0;
    };
    const float glyph_h = textGlyph.empty() ? 8.0f : float(textGlyph[0].height);

    const QRect canvas_rect(QPoint(0, 0), canvas_logical);
    const auto& plate  = layout.nameplate_rect;
    const auto& hp_bar = layout.hp_bar_rect;
    const float bar_dx = float(hp_bar.x() - plate.x());
    const float bar_dy = float(hp_bar.y() - plate.y());

    for (std::size_t i = 0; i < nameplate_count; ++i) {
        const auto& t = scene.tokens[i];

        QPointF s = camera.world_to_screen({t.world_x, t.world_y}, canvas_logical);
        s += plate.topLeft() * camera.zoom;
        if (!QRect(s.toPoint(), (plate.size() * camera.zoom).toSize()).intersects(canvas_rect)) continue;

        const float ox = std::round(t.world_x + float(plate.x()));
        const float oy = std::round(t.world_y + float(plate.y()));

        push_quad(world_verts, QRectF(ox, oy, plate.width(), plate.height()), white_uv_, QColor(20, 20, 20, 200));
        push_quad(world_verts, QRectF(ox + bar_dx, oy + bar_dy, hp_bar.width(), hp_bar.height()), white_uv_, QColor(100, 100, 100, 220));
        push_quad(world_verts, QRectF(ox + bar_dx, oy + bar_dy, hp_bar.width() * t.hp_frac, hp_bar.height()), white_uv_, QColor(255, 0, 0, 200));

        push_text(world_verts, t.name, ox + 2, oy, ox + float(plate.width()) - 2);
    }

    if (!scene.tokens.empty()) {
        const auto& t0 = scene.tokens[0];
        push_quad(world_verts, QRectF(std::round(t0.world_x + float(plate.right()) + 2), std::round(t0.world_y + float(plate.top())), 16, 16), icon_uv_, Qt::white);
    }

    for (std::size_t i = 0; i < scene.hotbar.size(); ++i) {
        const QRectF& slot = layout.hotbar_slots[i];
        push_nine_slice(screen_verts, slot, frame_uv_, 4, 128, Qt::gray);

        if (eleId == i)
            push_quad(screen_verts, slot, white_uv_, QColor(255, 255, 255, 80));

        if (i == 0) {
            push_quad(screen_verts, QRectF(slot.left() + (slot.width() - 16) / 2, slot.top()  + (slot.height() - 16) / 2, 16, 16), icon_uv_, Qt::white);
            continue;
        }

        const std::string& label = scene.hotbar[i];
        const float x = slot.left() + std::floor((slot.width() - text_width(label)) / 2.0);
        const float y = slot.top() + std::floor((slot.height() - glyph_h) / 2.0f);
        push_text(screen_verts, label, x, y, float(slot.right()) - 2);
    }

    const QRectF& strip = layout.combat_strip;
    push_nine_slice(screen_verts, strip, frame_uv_, 4, 128, Qt::gray);
    push_text(screen_verts, scene.combat_text, strip.left() + 4, strip.top() + std::floor((strip.height() - glyph_h) / 2.0f), float(strip.right()) - 4);

    const std::size_t s = screen_verts.size();
    const std::size_t w = world_verts.size();

    auto cleanup = [&]() {
        gl_->glDisable(GL_BLEND);
        shader.release();
        vbo.release();
        vao.release();
        atlas->release();
    };

    if (s + w > max_quads * 6) {
        qDebug() << "vertex lists exceeded size, skipping HUD draw";
        cleanup();
        return;
    }

    world_verts.reserve(s + w);
    world_verts.insert(world_verts.end(), screen_verts.begin(), screen_verts.end());
    vbo.write(0, world_verts.data(), int(world_verts.size() * sizeof(Vertex)));

    const float width = float(canvas_logical.width());
    const float height = float(canvas_logical.height());

    shader.setUniformValue("u_camera", buildCameraMatrix(camera, width, height));
    gl_->glDrawArrays(GL_TRIANGLES, 0, GLsizei(w));

    QMatrix4x4 projection;
    projection.ortho(0.0f, width, height, 0.0f, -1.0f, 1.0f);
    shader.setUniformValue("u_camera", projection);
    gl_->glDrawArrays(GL_TRIANGLES, GLint(w), GLsizei(s));

    cleanup();
}

void GlHudRenderer::shutdown() {
    atlas.reset();
    vao.destroy();
    vbo.destroy();
}