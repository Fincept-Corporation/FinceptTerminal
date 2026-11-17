#pragma once
#include "core/logging/Logger.h"
#include "services/notifications/NotificationService.h"
#include "storage/repositories/SettingsRepository.h"
#include "storage/secure/SecureStorage.h"

#include <QSet>

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

    // A secret that is stored but cannot be read (store busy / closed, key unavailable
    // on this account, damaged row) is NOT "not set": the legacy plaintext row is not
    // used in its place (it is blanked once migrated, or stale), and the field is
    // remembered so the next save_config() — which writes every field, this one empty
    // — does not erase the stored secret. Only a damaged row may be erased (it can
    // never be read again). A value the user enters replaces it as usual.
    QString get_secret(SettingsRepository& r, const QString& key) {
        const SecretRead sr = SecureStorage::instance().read(secret_handle(key));
        const bool failing = sr.failed();
        LOG_TRANSITION("Notifications", secret_handle(key), failing, failing ? LogLevel::Warn : LogLevel::Info,
                       failing ? logf("Notification credential could not be read; not replaced by the plaintext row "
                                      "and not erased",
                                      {{"provider", provider_id()}, {"field", key}})
                               : logf("Notification credential readable again",
                                      {{"provider", provider_id()}, {"field", key}}));
        if (failing) {
            if (sr.error != SecretRead::Error::Corrupt)
                unreadable_secrets_.insert(key);
            return {};
        }
        unreadable_secrets_.remove(key);
        if (sr.found() && !sr.value.isEmpty())
            return sr.value;
        return get_str(r, key); // nothing stored: legacy plaintext row, migrated on next save
    }

    void set_secret(SettingsRepository& r, const QString& key, const QString& value, const QString& cat) {
        if (value.isEmpty()) {
            if (unreadable_secrets_.contains(key))
                return; // empty only because it could not be read: keep the stored secret
            SecureStorage::instance().remove(secret_handle(key));
            r.set(key, QString{}, cat);
            return;
        }
        // The plaintext row is blanked only once the encrypted copy was written AND
        // read back equal (write -> read back -> compare -> only then delete the old).
        const auto stored = SecureStorage::instance().store_verified(secret_handle(key), value);
        LOG_TRANSITION("Notifications", QStringLiteral("secret_store|") + secret_handle(key), stored.is_err(),
                       stored.is_err() ? LogLevel::Warn : LogLevel::Info,
                       stored.is_err() ? logf("Notification credential kept in settings, secure storage write not "
                                              "confirmed",
                                              {{"provider", provider_id()},
                                               {"field", key},
                                               {"error", QString::fromStdString(stored.error())}})
                                       : logf("Notification credential stored in secure storage again",
                                              {{"provider", provider_id()}, {"field", key}}));
        if (stored.is_ok()) {
            r.set(key, QString{}, cat); // encrypted copy is authoritative
            unreadable_secrets_.remove(key);
        } else {
            // Keychain unavailable — don't drop the credential: it goes to the row instead.
            // get_secret() prefers a stored copy, so the copy this replaces must not shadow
            // the row (the user replaced it on purpose) — but it is deleted only once the
            // row demonstrably holds the new value (write -> read back -> compare). If the
            // row could not be written either, the stored copy stays: an older credential
            // beats none.
            const auto row_written = r.set(key, value, cat);
            const auto row_back = row_written.is_ok() ? r.get(key) : Result<QString>::err(row_written.error());
            if (row_back.is_ok() && row_back.value() == value) {
                SecureStorage::instance().remove(secret_handle(key));
            } else {
                LOG_WARN("Notifications", logf("Notification credential could not be saved; the previous one is kept",
                                               {{"provider", provider_id()},
                                                {"field", key},
                                                {"error", row_back.is_err() ? QString::fromStdString(row_back.error())
                                                                            : QStringLiteral("read_back_mismatch")}}));
            }
        }
    }

  public:
    /// True when a stored secret of this provider could not be read at the last
    /// load_config(): is_configured() is then false although a secret IS stored —
    /// a settings UI should say "could not read the stored credential" (and offer
    /// load_config() again as the retry) instead of "not configured".
    bool has_unreadable_secret() const { return !unreadable_secrets_.isEmpty(); }

  protected:
    QSet<QString> unreadable_secrets_; ///< fields whose stored secret could not be read

    /// Subclasses load their specific credential fields here.
    virtual void load_fields(SettingsRepository& r, const QString& cat) = 0;
    /// Subclasses persist their specific credential fields here.
    virtual void save_fields(SettingsRepository& r, const QString& cat) = 0;
};

} // namespace fincept::notifications
