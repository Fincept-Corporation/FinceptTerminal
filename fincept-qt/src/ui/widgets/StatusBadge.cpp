#include "ui/widgets/StatusBadge.h"

#include "ui/theme/Theme.h"

namespace fincept::ui {

StatusBadge::StatusBadge(QWidget* parent) : QLabel(parent) {
    set_status(Status::Idle);
}

void StatusBadge::set_status(Status s) {
    switch (s) {
        case Status::Connected:
            setText("CONNECTED");
            setStyleSheet(QString("color: %1; font-size: 13px; background: transparent;").arg(colors::GREEN));
            break;
        case Status::Disconnected:
            setText("OFFLINE");
            setStyleSheet(QString("color: %1; font-size: 13px; background: transparent;").arg(colors::RED));
            break;
        case Status::Loading:
            setText("LOADING...");
            setStyleSheet(QString("color: %1; font-size: 13px; background: transparent;").arg(colors::GRAY));
            break;
        case Status::Idle:
            setText("READY");
            setStyleSheet(QString("color: %1; font-size: 13px; background: transparent;").arg(colors::MUTED));
            break;
    }
}

void StatusBadge::set_custom(const QString& text, const QString& color) {
    setText(text);
    setStyleSheet(QString("color: %1; font-size: 13px; background: transparent;").arg(color));
}

} // namespace fincept::ui
