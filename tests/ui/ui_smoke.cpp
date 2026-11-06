#include <QtTest>

#include "arcane/ui/shell/main_window.hpp"

class UiSmoke : public QObject {
  Q_OBJECT

 private slots:
  void main_window_constructs() {
    arcane::ui::MainWindow window;
    QVERIFY(!window.windowTitle().isEmpty());
  }
};

QTEST_MAIN(UiSmoke)
#include "ui_smoke.moc"
