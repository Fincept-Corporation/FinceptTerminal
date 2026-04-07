#include "ui/navigation/DockStatusBar.h"

namespace fincept::ui {

DockStatusBar::DockStatusBar(QWidget* parent) : QStatusBar(parent) {
    inner_ = new StatusBar(this);
    addPermanentWidget(inner_, 1);
    setFixedHeight(26);
    setSizeGripEnabled(false);
    setStyleSheet("QStatusBar{border:none;padding:0;margin:0;}");
}

} // namespace fincept::ui
