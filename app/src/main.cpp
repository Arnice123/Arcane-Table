#include <QApplication>
#include <QSurfaceFormat>

#include "arcane/ui/shell/main_window.hpp"

int main(int argc, char** argv) {
  // Minimum supported OpenGL is 3.3 core. This must be set before the
  // QApplication is created.
  QSurfaceFormat format;
  format.setVersion(3, 3);
  format.setProfile(QSurfaceFormat::CoreProfile);
  QSurfaceFormat::setDefaultFormat(format);

  QApplication app(argc, argv);

  arcane::ui::MainWindow window;
  window.show();

  return QApplication::exec();
}
