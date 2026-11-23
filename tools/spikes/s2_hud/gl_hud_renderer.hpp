#pragma once

#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QSize>
#include <QPointF>
#include <QRectF>
#include <QRect>

#include <vector>
#include <memory>

#include "hud_test_scene.hpp"
#include "gl_hud_input.hpp"
#include "gl_vertex.hpp"

class GlHudRenderer {
public:
    // Creates shaders, a glyph atlas and a UI atlas for the frames and icons.
    void init(QOpenGLFunctions_3_3_Core* gl);

    // Draws every HUD element for this frame.
    // In: scene, camera, canvas size and device pixel ratio, how many nameplates to draw, and the special hotbar item to glow
    // Out: pixels on the canvas. ONE draw call per kind of element.
    void draw(const HudTestScene& scene, const Camera& camera, const HudLayout& layout, QSize canvas_logical, std::size_t nameplate_count, GlHudInput::ElementId eleId);

    // Free GPU Resources
    void shutdown();


private:
    static constexpr int first = 32;

    QOpenGLFunctions_3_3_Core* gl_ = nullptr;
    QOpenGLShaderProgram shader;
    QOpenGLBuffer vbo{QOpenGLBuffer::VertexBuffer};
    QOpenGLVertexArrayObject vao;

    std::size_t max_quads = 20000;

    struct GlyphInfo { QRectF uv; int width, height, advance; };
    std::vector<GlyphInfo> textGlyph;
    QRectF white_uv_, frame_uv_, icon_uv_;
    std::unique_ptr<QOpenGLTexture> atlas;

    std::vector<Vertex> world_verts;
    std::vector<Vertex> screen_verts;
};