#include "ui/navigation/DockToolBar.h"

#include "ui/widgets/ThemeHelpers.h"

namespace fincept::ui {

DockToolBar::DockToolBar(QWidget* parent) : QToolBar("Main Toolbar", parent) {
    setMovable(false);
    setFloatable(false);
    setAllowedAreas(Qt::TopToolBarArea);
    metrics::follow_fixed_height(this, 32); // grows with the user's font
    setStyleSheet("QToolBar{border:none;spacing:0;padding:0;margin:0;}");

    inner_ = new ToolBar(this);
    addWidget(inner_);
}

} // namespace fincept::ui
