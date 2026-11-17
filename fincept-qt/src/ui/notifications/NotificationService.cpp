#include "ui/notifications/NotificationService.h"

#include "core/logging/Logger.h"

#include <QDateTime>

namespace fincept::ui {

ToastService& ToastService::instance() {
    static ToastService s;
    return s;
}

void ToastService::post(Severity sev, const QString& message, const QString& source, QWidget* target_frame) {
    Notification n;
    n.severity = sev;
    n.message = message;
    n.source = source;
    n.target_frame = target_frame;
    n.ts_ms = QDateTime::currentMSecsSinceEpoch();

    history_.prepend(n);
    while (history_.size() > kHistoryLimit)
        history_.removeLast();

    const char* level = "INFO";
    switch (sev) {
        case Severity::Success:
            level = "OK";
            break;
        case Severity::Warning:
            level = "WARN";
            break;
        case Severity::Error:
            level = "ERR";
            break;
        case Severity::Info:
            break;
    }
    // The toast text is not logged (it quotes file names, symbols, account names, error
    // bodies); the code that raised the toast logs its own event.
    LOG_DEBUG("Toast", fincept::logf("Toast shown", {{"level", level}, {"source", source}, {"chars", message.size()}}));

    emit posted(n);
}

QList<ToastService::Notification> ToastService::history() const {
    return history_;
}

} // namespace fincept::ui
