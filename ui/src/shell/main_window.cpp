#include "arcane/ui/shell/main_window.hpp"

#include <QLabel>
#include <Qt>

#include "arcane/core/version.hpp"

namespace arcane::ui {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  setWindowTitle(QStringLiteral("Arcane Table %1")
                     .arg(QLatin1String(arcane::version_string)));

  auto* label = new QLabel(QStringLiteral("Arcane Table scaffold"), this);
  label->setAlignment(Qt::AlignCenter);
  setCentralWidget(label);

  resize(1280, 720);
}

}  // namespace arcane::ui
