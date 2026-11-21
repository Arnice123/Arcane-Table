#pragma once

#include <vector>
#include <QRectF>
#include <QColor>

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