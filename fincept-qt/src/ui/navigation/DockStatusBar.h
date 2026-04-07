#pragma once
#include "ui/navigation/StatusBar.h"

#include <QStatusBar>

namespace fincept::ui {

/// QStatusBar wrapper around the existing StatusBar widget.
/// Used with QMainWindow::setStatusBar() so the status bar lives outside
/// CDockManager's central widget area — no layout conflicts.
class DockStatusBar : public QStatusBar {
    Q_OBJECT
  public:
    explicit DockStatusBar(QWidget* parent = nullptr);

    StatusBar* inner() const { return inner_; }

  private:
    StatusBar* inner_ = nullptr;
};

} // namespace fincept::ui
