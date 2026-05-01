#include "ui/error/ErrorPipeline.h"

#include "core/logging/Logger.h"
#include "ui/notifications/NotificationService.h"

namespace fincept::ui {

ErrorPipeline& ErrorPipeline::instance() {
    static ErrorPipeline s;
    return s;
}

void ErrorPipeline::report_panel_error(const QString& panel_id, Severity sev,
                                        const QString& message, const QString& source) {
    panel_errors_[panel_id] = panel_errors_.value(panel_id) + 1;
    LOG_WARN("ErrorPipeline",
             QString("Panel %1 error (sev=%2): %3 [from %4]")
                 .arg(panel_id)
                 .arg(sev == Severity::Error ? "err" : "warn")
                 .arg(message, source.isEmpty() ? QStringLiteral("-") : source));

    ToastService::instance().post(
        sev == Severity::Error ? ToastService::Severity::Error
                               : ToastService::Severity::Warning,
        message,
        source.isEmpty() ? panel_id : source);

    emit panel_errors_changed(panel_id);
}

void ErrorPipeline::clear_panel_errors(const QString& panel_id) {
    if (panel_errors_.remove(panel_id) > 0)
        emit panel_errors_changed(panel_id);
}

void ErrorPipeline::report_shell_error(const QString& category, Severity sev,
                                        const QString& message, const QString& source) {
    if (message.isEmpty()) {
        clear_shell_error(category);
        return;
    }
    shell_errors_[category] = message;
    LOG_WARN("ErrorPipeline",
             QString("Shell error [%1] sev=%2: %3 [from %4]")
                 .arg(category)
                 .arg(sev == Severity::Error ? "err" : "warn")
                 .arg(message, source.isEmpty() ? QStringLiteral("-") : source));
    ToastService::instance().post(
        sev == Severity::Error ? ToastService::Severity::Error
                               : ToastService::Severity::Warning,
        message,
        source.isEmpty() ? category : source);
    emit shell_errors_changed();
}

void ErrorPipeline::clear_shell_error(const QString& category) {
    if (shell_errors_.remove(category) > 0)
        emit shell_errors_changed();
}

int ErrorPipeline::panel_error_count() const {
    return panel_errors_.size();
}

int ErrorPipeline::shell_error_count() const {
    return shell_errors_.size();
}

} // namespace fincept::ui
