#include "ui/notifications/NotificationService.h"

#include "core/logging/Logger.h"

#include <QDateTime>

namespace fincept::ui {

ToastService& ToastService::instance() {
    static ToastService s;
    return s;
}

void ToastService::post(Severity sev, const QString& message, const QString& source,
                        QWidget* target_frame) {
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
        case Severity::Success:  level = "OK"; break;
        case Severity::Warning:  level = "WARN"; break;
        case Severity::Error:    level = "ERR"; break;
        case Severity::Info:     break;
    }
    LOG_INFO("Toast",
             QString("[%1] %2 (%3)").arg(level, message, source.isEmpty() ? QStringLiteral("-") : source));

    emit posted(n);
}

QList<ToastService::Notification> ToastService::history() const {
    return history_;
}

} // namespace fincept::ui
