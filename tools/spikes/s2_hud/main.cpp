// Arcane spike S2: HUD approach comparison (Qt widgets vs GL-drawn).
// Keys: 1 = no HUD, 2 = Qt overlay, 3 = GL-drawn, N = cycle nameplate count,
//       Z = cycle zoom, B = start/stop benchmark pan, G = save screenshot.

#include <QApplication>
#include <QSurfaceFormat>

#include "spike_canvas.hpp"

int main(int argc, char** argv) {
    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setSwapInterval(0);   // vsync off
    QSurfaceFormat::setDefaultFormat(format);

    QApplication app(argc, argv);

    SpikeCanvas canvas;
    canvas.setWindowTitle("Arcane spike S2 - HUD comparison");
    canvas.show();
    canvas.setFocus();

    return app.exec();
}