#pragma once

#include <vector>
#include <QRectF>
#include <QColor>
#include <QMatrix4x4>

#include "hud_test_scene.hpp"

struct Vertex { float x, y; float u, v; float r, g, b, a; };

inline void push_quad(std::vector<Vertex>& out, const QRectF& rect, const QRectF& uv, const QColor& tint) {
    const float x0 = rect.left(), y0 = rect.top(), x1 = rect.right(), y1 = rect.bottom();
    const float u0 = uv.left(), v0 = uv.top(), u1 = uv.right(), v1 = uv.bottom();
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

static void push_nine_slice(std::vector<Vertex>& out, const QRectF& dst,const QRectF& uv, float corner_px,float atlas_dim, const QColor& tint) {
    const float c  = corner_px;
    const float cu = corner_px / atlas_dim;     // corner size in UV units

    const float xs[4] = { float(dst.left()), float(dst.left()) + c, float(dst.right()) - c, float(dst.right()) };
    const float ys[4] = { float(dst.top()), float(dst.top()) + c, float(dst.bottom()) - c, float(dst.bottom()) };

    const float us[4] = { float(uv.left()), float(uv.left()) + cu, float(uv.right()) - cu, float(uv.right()) };
    const float vs[4] = { float(uv.top()), float(uv.top()) + cu, float(uv.bottom()) - cu, float(uv.bottom()) };

    for (int r = 0; r < 3; ++r)
        for (int col = 0; col < 3; ++col)
            push_quad(out, QRectF(QPointF(xs[col], ys[r]), QPointF(xs[col + 1], ys[r + 1])), QRectF(QPointF(us[col], vs[r]), QPointF(us[col + 1], vs[r + 1])), tint);
}

static QMatrix4x4 buildCameraMatrix(const Camera& camera, const float& width, const float& height) {
    QMatrix4x4 m;
    m.ortho(0, width, height, 0, -1, 1);
    m.translate(width / 2.0f, height / 2.0f);
    m.scale(camera.zoom, camera.zoom);
    m.translate(-camera.pan_x, -camera.pan_y);
    return m;
}