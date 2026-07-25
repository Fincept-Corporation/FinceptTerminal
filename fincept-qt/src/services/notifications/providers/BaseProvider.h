#pragma once
#include "services/notifications/NotificationService.h"
#include "storage/repositories/SettingsRepository.h"
#include "storage/secure/SecureStorage.h"

namespace fincept::notifications {

/// Convenience base class for HTTP-based notification providers.
/// Subclasses implement provider_id(), display_name(), icon(), and send().
/// load_config() / save_config() / is_configured() / is_enabled() are handled here
/// via SettingsRepository under category "notif_<provider_id>".
class BaseProvider : public INotificationProvider {
  public:
    bool is_enabled() const override { return enabled_; }

    void load_config() override {
        auto& r = SettingsRepository::instance();
        const QString cat = category();
        enabled_ = get_bool(r, cat + ".enabled", false);
        load_fields(r, cat);
    }

    void save_config() override {
        auto& r = SettingsRepository::instance();
        const QString cat = category();
        r.set(cat + ".enabled", enabled_ ? "1" : "0", cat);
        save_fields(r, cat);
    }

  protected:
    bool enabled_{false};

    QString category() const { return QString("notif_%1").arg(provider_id()); }

    static bool get_bool(SettingsRepository& r, const QString& key, bool def) {
        auto res = r.get(key);
        return res.is_ok() && !res.value().isEmpty() ? (res.value() == "1") : def;
    }

    static QString get_str(SettingsRepository& r, const QString& key) {
        auto res = r.get(key);
        return res.is_ok() ? res.value() : QString{};
    }

    // ── Secret fields ────────────────────────────────────────────────────────
    // SMTP passwords, bot tokens, API keys and webhook URLs must NOT sit in the
    // plaintext `settings` SQLite table — Settings ▸ Storage ships a SQL console
    // with a one-click `SELECT * FROM settings` that would render them on screen.
    // These mirror the proven LlmConfigRepository pattern: read prefers
    // SecureStorage (AES-256-GCM, machine-bound) and falls back to the legacy
    // plaintext column so no existing credential is ever lost; write moves the
    // value into SecureStorage and blanks the column, but ONLY if the encrypted
    // store succeeded — a failure keeps the plaintext rather than losing the key.

    static QString secret_handle(const QString& key) { return QStringLiteral("notif:secret:") + key; }

    static QString get_secret(SettingsRepository& r, const QString& key) {
        auto sr = SecureStorage::instance().retrieve(secret_handle(key));
        if (sr.is_ok() && !sr.value().isEmpty())
            return sr.value();
        return get_str(r, key); // legacy plaintext row, migrated on next save
    }

    static void set_secret(SettingsRepository& r, const QString& key, const QString& value, const QString& cat) {
        if (value.isEmpty()) {
            SecureStorage::instance().remove(secret_handle(key));
            r.set(key, QString{}, cat);
            return;
        }
        if (SecureStorage::instance().store(secret_handle(key), value).is_ok())
            r.set(key, QString{}, cat); // encrypted copy is authoritative
        else
            r.set(key, value, cat); // keychain unavailable — don't drop the credential
    }

    /// Subclasses load their specific credential fields here.
    virtual void load_fields(SettingsRepository& r, const QString& cat) = 0;
    /// Subclasses persist their specific credential fields here.
    virtual void save_fields(SettingsRepository& r, const QString& cat) = 0;
};

} // namespace fincept::notifications
