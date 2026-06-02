#include "ui/widgets/StatusBadge.h"

#include "ui/theme/Theme.h"

namespace fincept::ui {

StatusBadge::StatusBadge(QWidget* parent) : QLabel(parent) {
    set_status(Status::Idle);
}

void StatusBadge::set_status(Status s) {
    status_ = s;
    custom_ = false;
    switch (s) {
        case Status::Connected:
            setText(tr("CONNECTED"));
            setStyleSheet(QString("color: %1; font-size: 13px; background: transparent;").arg(colors::GREEN()));
            break;
        case Status::Disconnected:
            setText(tr("OFFLINE"));
            setStyleSheet(QString("color: %1; font-size: 13px; background: transparent;").arg(colors::RED()));
            break;
        case Status::Loading:
            setText(tr("LOADING..."));
            setStyleSheet(QString("color: %1; font-size: 13px; background: transparent;").arg(colors::GRAY()));
            break;
        case Status::Idle:
            setText(tr("READY"));
            setStyleSheet(QString("color: %1; font-size: 13px; background: transparent;").arg(colors::MUTED()));
            break;
    }
}

void StatusBadge::set_custom(const QString& text, const QString& color) {
    custom_ = true;
    setText(text);
    setStyleSheet(QString("color: %1; font-size: 13px; background: transparent;").arg(color));
}

void StatusBadge::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QLabel::changeEvent(event);
}

void StatusBadge::retranslateUi() {
    // Only preset statuses carry translatable text. A custom badge holds a
    // caller-supplied string (often a data value) — leave it as-is.
    if (custom_)
        return;
    switch (status_) {
        case Status::Connected:
            setText(tr("CONNECTED"));
            break;
        case Status::Disconnected:
            setText(tr("OFFLINE"));
            break;
        case Status::Loading:
            setText(tr("LOADING..."));
            break;
        case Status::Idle:
            setText(tr("READY"));
            break;
    }
}

} // namespace fincept::ui
