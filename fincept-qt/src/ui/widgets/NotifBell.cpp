#include "ui/widgets/NotifBell.h"

#include "services/notifications/NotificationService.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

namespace fincept::ui {

NotifBell::NotifBell(QWidget* parent) : QPushButton(parent) {
    setFlat(true);
    setMinimumHeight(20); // one text line; grows with a large base font
    setCursor(Qt::PointingHandCursor);
    update_label();

    // Wire to service
    connect(&fincept::notifications::NotificationService::instance(),
            &fincept::notifications::NotificationService::unread_count_changed, this, &NotifBell::set_unread);

    connect(this, &QPushButton::clicked, this, &NotifBell::bell_clicked);
}

void NotifBell::set_unread(int count) {
    unread_ = count;
    update_label();
}

void NotifBell::update_label() {
    const QString color = unread_ > 0 ? ui::colors::WARNING() : ui::colors::TEXT_SECONDARY();
    const QString text = unread_ > 0 ? QString("ALERTS [%1]").arg(unread_) : QString("ALERTS");

    setText(text);
    setStyleSheet(QString("QPushButton { background: transparent; color: %1; border: none; "
                          "font-size: " + fincept::ui::fonts::px(-3) + "px; font-weight: %2; padding: 0 4px; }"
                          "QPushButton:hover { color: %3; }")
                      .arg(color, unread_ > 0 ? "bold" : "normal", QString(ui::colors::AMBER())));
}

} // namespace fincept::ui
