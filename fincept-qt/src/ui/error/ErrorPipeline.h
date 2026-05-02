#pragma once

#include <QHash>
#include <QObject>
#include <QString>

namespace fincept::ui {

/// Phase 9 / decision 10.6: structured error pipeline panel → frame → shell.
///
/// Three reporting levels:
///   - **Panel**: an inline error specific to one panel ("AAPL quote
///     fetch failed"). Reported via `report_panel_error(panel_id, ...)`.
///     Subscribers: panel-level error banner widget (Phase 9 follow-up).
///   - **Frame**: an aggregate "this frame has 3 panels with errors"
///     surface for the frame's title bar / status. Computed from panel-
///     level errors; consumers query `panel_error_count_for_frame(...)`.
///   - **Shell**: a global "network: degraded" indicator. Reported via
///     `report_shell_error(category, ...)`. Subscribers: command bar
///     status indicator, status bar.
///
/// All errors also flow into NotificationService as toasts — the
/// pipeline is about *aggregation*, the notification service is about
/// *display*. The two coexist; consumers pick whichever surface fits.
class ErrorPipeline : public QObject {
    Q_OBJECT
  public:
    enum class Severity { Warning, Error };

    static ErrorPipeline& instance();

    /// Per-panel error. `panel_id` is the dock router string id (or the
    /// PanelInstanceId UUID — either works as a key, callers choose).
    void report_panel_error(const QString& panel_id, Severity sev,
                            const QString& message, const QString& source = {});
    void clear_panel_errors(const QString& panel_id);

    /// Shell-level category error ("network", "telemetry", "auth", ...).
    /// Setting message to empty string clears the category.
    void report_shell_error(const QString& category, Severity sev,
                            const QString& message, const QString& source = {});
    void clear_shell_error(const QString& category);

    /// Inspectors for surfaces.
    int panel_error_count() const;            ///< total panels with at least one error
    int shell_error_count() const;            ///< total shell categories with active errors

  signals:
    void panel_errors_changed(const QString& panel_id);
    void shell_errors_changed();

  private:
    ErrorPipeline() = default;

    /// panel_id → number of errors. Cleared via clear_panel_errors.
    QHash<QString, int> panel_errors_;

    /// category → latest message (empty after clear_shell_error).
    QHash<QString, QString> shell_errors_;
};

} // namespace fincept::ui
