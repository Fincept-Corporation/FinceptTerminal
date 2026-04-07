#pragma once
#include "ui/navigation/ToolBar.h"

#include <QToolBar>

namespace fincept::ui {

/// QToolBar wrapper around the existing ToolBar widget.
/// Used with QMainWindow::addToolBar() so the toolbar lives outside
/// CDockManager's central widget area — no layout conflicts.
class DockToolBar : public QToolBar {
    Q_OBJECT
  public:
    explicit DockToolBar(QWidget* parent = nullptr);

    ToolBar* inner() const { return inner_; }

  private:
    ToolBar* inner_ = nullptr;
};

} // namespace fincept::ui
