#pragma once

#include <QMainWindow>

namespace arcane::ui {

// Placeholder shell window. Replaced by the real shell in story UILAYOUT-01
// (canvas + dock areas + panel registry).
class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  explicit MainWindow(QWidget* parent = nullptr);
};

}  // namespace arcane::ui
