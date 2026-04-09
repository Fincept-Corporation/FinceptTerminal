#pragma once
#include <QDateTime>
#include <QObject>
#include <QString>
#include <QVector>

#include <functional>
#include <memory>
#include <vector>

namespace fincept::notifications {

// ── Enumerations ──────────────────────────────────────────────────────────────

enum class NotifLevel { Info, Warning, Alert, Critical };
enum class NotifTrigger { Manual, PriceAlert, OrderFill, NewsAlert, WorkflowNode };

// ── Data structures ───────────────────────────────────────────────────────────

struct NotificationRequest {
    QString title;
    QString message;
    NotifLevel level = NotifLevel::Info;
    NotifTrigger trigger = NotifTrigger::Manual;
    QDateTime timestamp{QDateTime::currentDateTime()};
};

struct NotificationRecord {
    int id{0};
    NotificationRequest request;
    bool read{false};
    QDateTime received_at{QDateTime::currentDateTime()};
};

// ── Provider interface ────────────────────────────────────────────────────────

/// Abstract base for every external notification channel.
/// Each provider is responsible for reading/writing its own credentials
/// from SettingsRepository under category "notif_<provider_id>".
class INotificationProvider {
  public:
    virtual ~INotificationProvider() = default;

    /// Unique machine-readable identifier, e.g. "telegram".
    virtual QString provider_id() const = 0;

    /// Human-readable name shown in UI, e.g. "Telegram".
    virtual QString display_name() const = 0;

    /// Icon/emoji character used in settings UI header.
    virtual QString icon() const = 0;

    /// Returns true when the minimum required credentials are present.
    virtual bool is_configured() const = 0;

    /// True when the user has enabled this provider in Settings.
    virtual bool is_enabled() const = 0;

    /// Perform the HTTP call to deliver req. Invoke cb with (ok, error_string).
    virtual void send(const NotificationRequest& req, std::function<void(bool ok, QString error)> cb) = 0;

    /// Reload credentials from SettingsRepository.
    virtual void load_config() = 0;

    /// Persist current credential state to SettingsRepository.
    virtual void save_config() = 0;
};

// ── Service ───────────────────────────────────────────────────────────────────

/// Central notification dispatcher.  Maintains in-process history, dispatches
/// to all configured+enabled providers, and emits Qt signals for UI layers.
class NotificationService : public QObject {
    Q_OBJECT
  public:
    static NotificationService& instance();

    /// Dispatch to all enabled+configured providers that match the trigger filter.
    /// Always adds to history and emits notification_received regardless of
    /// external delivery outcome.
    void send(const NotificationRequest& req);

    /// Deliver to a single named provider — used for Settings "Test Send" button.
    void send_to(const QString& provider_id, const NotificationRequest& req,
                 std::function<void(bool ok, QString error)> cb);

    // ── History ───────────────────────────────────────────────────────────────
    const QVector<NotificationRecord>& history() const;
    int unread_count() const;
    void mark_read(int id);
    void mark_all_read();

    // ── Provider access (for settings page) ──────────────────────────────────
    QVector<INotificationProvider*> providers() const;
    INotificationProvider* provider(const QString& id) const;

    /// Call after settings are saved to reload all provider configs.
    void reload_all_configs();

  signals:
    void notification_received(const fincept::notifications::NotificationRecord& record);
    void unread_count_changed(int count);

  private:
    explicit NotificationService();
    void register_providers();

    std::vector<std::unique_ptr<INotificationProvider>> providers_;
    QVector<NotificationRecord> history_;
    int next_id_{1};
    int unread_{0};
};

} // namespace fincept::notifications
