#include "ui/navigation/DockToolBar.h"

namespace fincept::ui {

DockToolBar::DockToolBar(QWidget* parent) : QToolBar("Main Toolbar", parent) {
    setMovable(false);
    setFloatable(false);
    setAllowedAreas(Qt::TopToolBarArea);
    setFixedHeight(32);
    setStyleSheet("QToolBar{border:none;spacing:0;padding:0;margin:0;}");

    inner_ = new ToolBar(this);
    addWidget(inner_);
}

} // namespace fincept::ui
