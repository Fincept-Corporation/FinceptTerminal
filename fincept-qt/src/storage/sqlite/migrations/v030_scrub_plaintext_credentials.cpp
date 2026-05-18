// v030_scrub_plaintext_credentials.cpp
//
// CR-08 migration: scrub plaintext api_key and session_token that prior
// versions of save_session() wrote into the unencrypted settings table.
// Moves both secrets into SecureStorage (AES-256-GCM) and rewrites the
// fincept_session JSON row without them so existing installs are protected
// on first upgrade.

#include "storage/migrations/MigrationRunner.h"
#include "storage/repositories/SettingsRepository.h"
#include "storage/secure/SecureStorage.h"
#include "core/logging/Logger.h"

#include <QJsonDocument>
#include <QJsonObject>

namespace fincept::storage::migrations {

static bool v030_scrub_plaintext_credentials() {
    // ── Step 1: scrub fincept_session JSON ───────────────────────────────────
    auto r = SettingsRepository::instance().get("fincept_session");
    if (r.is_ok() && !r.value().isEmpty()) {
        auto doc = QJsonDocument::fromJson(r.value().toUtf8());
        if (!doc.isNull() && doc.isObject()) {
            QJsonObject obj = doc.object();

            // Migrate secrets to SecureStorage before removing them
            const QString api_key     = obj.value("api_key").toString();
            const QString session_tok = obj.value("session_token").toString();

            if (!api_key.isEmpty()) {
                auto sr = SecureStorage::instance().store("api_key", api_key);
                if (sr.is_err())
                    LOG_WARN("Migration", "v030: failed to store api_key in SecureStorage");
                else
                    LOG_INFO("Migration", "v030: api_key migrated to SecureStorage");
            }

            if (!session_tok.isEmpty()) {
                auto sr = SecureStorage::instance().store("session_token", session_tok);
                if (sr.is_err())
                    LOG_WARN("Migration", "v030: failed to store session_token in SecureStorage");
                else
                    LOG_INFO("Migration", "v030: session_token migrated to SecureStorage");
            }

            // Rewrite the row without secrets
            obj.remove("api_key");
            obj.remove("session_token");
            QString clean = QString::fromUtf8(
                QJsonDocument(obj).toJson(QJsonDocument::Compact));
            auto wr = SettingsRepository::instance().set("fincept_session", clean, "auth");
            if (wr.is_err())
                LOG_WARN("Migration", "v030: failed to rewrite fincept_session row");
            else
                LOG_INFO("Migration", "v030: fincept_session rewritten without plaintext secrets");
        }
    }

    // ── Step 2: remove legacy fincept_api_key plaintext row ─────────────────
    SettingsRepository::instance().remove("fincept_api_key");
    LOG_INFO("Migration", "v030: removed legacy fincept_api_key row from settings");

    return true;
}

REGISTER_MIGRATION(30, "scrub_plaintext_credentials", v030_scrub_plaintext_credentials);

} // namespace fincept::storage::migrations