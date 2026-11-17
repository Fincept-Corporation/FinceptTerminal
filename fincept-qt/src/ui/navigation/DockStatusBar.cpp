#include "ui/navigation/DockStatusBar.h"

#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/ThemeHelpers.h"

namespace fincept::ui {

DockStatusBar::DockStatusBar(QWidget* parent) : QStatusBar(parent) {
    inner_ = new StatusBar(this);
    addPermanentWidget(inner_, 1);
    metrics::follow_fixed_height(this, 26); // grows with the user's font
    setSizeGripEnabled(false);

    auto apply_style = [this]() {
        setStyleSheet(QString("QStatusBar{border:none;padding:0;margin:0;background:%1;}").arg(colors::BG_BASE()));
    };
    apply_style();
    connect(&ThemeManager::instance(), &ThemeManager::theme_changed, this,
            [apply_style](const ThemeTokens&) { apply_style(); });
}

} // namespace fincept::ui
