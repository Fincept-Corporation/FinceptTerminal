#pragma once

#include <QList>
#include <QObject>
#include <QString>

class QWidget;

namespace fincept::ui {

/// Phase 9 / decision 10.7: shell-singleton toast service.
///
/// A "toast" is a short-lived message routed to a target frame (or the
/// shell-level toast tray when no frame target makes sense). Subscribers
/// — typically the per-frame `ToastBar` widget (Phase 9 follow-up) —
/// connect to `posted` and render their own toast UI. The service itself
/// just queues + emits; rendering + animation lives in the consumer.
///
/// Per design 10.7 we also keep a persistent dismissable history drawer.
/// That UI lands with the full Phase 9 ErrorPipeline. v1 just keeps the
/// last N notifications in memory for any drawer that wants them.
///
/// Naming note: this is intentionally `ToastService`, not `NotificationService`,
/// to avoid colliding with the pre-existing `fincept::notifications::NotificationService`
/// (the external-channel dispatcher: Telegram / Slack / Discord / etc.).
/// The two have different responsibilities and should not be confused.
class ToastService : public QObject {
    Q_OBJECT
  public:
    enum class Severity { Info, Success, Warning, Error };

    struct Notification {
        Severity severity = Severity::Info;
        QString message;
        QString source;          ///< panel id / service name / action id
        QWidget* target_frame = nullptr; ///< nullptr = shell-level (any frame can show)
        qint64 ts_ms = 0;
    };

    static ToastService& instance();

    /// Post a notification. Stores in history; emits `posted` synchronously
    /// so connected toast bars render immediately.
    void post(Severity sev, const QString& message, const QString& source = {},
              QWidget* target_frame = nullptr);

    /// History (most recent first), capped at `kHistoryLimit`.
    QList<Notification> history() const;

    static constexpr int kHistoryLimit = 100;

  signals:
    void posted(const Notification& n);

  private:
    ToastService() = default;

    QList<Notification> history_;
};

} // namespace fincept::ui
