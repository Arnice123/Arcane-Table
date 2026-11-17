// PLACEHOLDER so the spike target builds. Replace this file with your harness.
// It opens an empty QOpenGLWidget window, nothing more.

#include <QApplication>
#include <QOpenGLWidget>
#include <QSurfaceFormat>

int main(int argc, char** argv) {
    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    // For the vsync-off timing runs: format.setSwapInterval(0);
    QSurfaceFormat::setDefaultFormat(format);

    QApplication app(argc, argv);
    QOpenGLWidget canvas;
    canvas.setWindowTitle("Arcane spike S2 (placeholder)");
    canvas.resize(1280, 720);
    canvas.show();
    return app.exec();
}
