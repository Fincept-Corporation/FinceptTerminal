#include "auth/AuthManager.h"

#include "auth/AuthApi.h"
#include "auth/GoogleDesktopLogin.h"
#include "auth/InactivityGuard.h"
#include "auth/PinManager.h"
#include "auth/UserApi.h"
#include "core/logging/Logger.h"
#include "network/http/HttpClient.h"
#include "storage/repositories/LlmConfigRepository.h"
#include "storage/repositories/SettingsRepository.h"
#include "storage/secure/SecretMigration.h"
#include "storage/secure/SecureStorage.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonDocument>
#include <QRandomGenerator>
#include <QSysInfo>
#include <QThread>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <utility>

namespace fincept::auth {

AuthManager& AuthManager::instance() {
    static AuthManager s;
    return s;
}

AuthManager::AuthManager() {
    // HttpClient inspects every Fincept 401 (before the caller's handler runs)
    // and reports the two that concern the session. Both singletons live for
    // the whole app and on the GUI thread.
    auto& http = fincept::HttpClient::instance();
    connect(&http, &fincept::HttpClient::session_token_rejected, this,
            [this](const QString& dead_token) { handle_unauthorized(dead_token, true); });
    connect(&http, &fincept::HttpClient::api_key_rejected, this, [this]() { handle_unauthorized({}, false); });
    // Billed responses carry the balance after the charge: the header chip
    // follows spending without re-polling /user/profile.
    connect(&http, &fincept::HttpClient::credit_balance_reported, this,
            [this](double balance) { report_credit_balance(balance); });
}

void AuthManager::set_loading(bool v) {
    if (is_loading_ != v) {
        is_loading_ = v;
        emit loading_changed(v);
    }
}

QJsonObject AuthManager::unwrap_data(const QJsonObject& raw) const {
    if (raw.contains("data") && raw["data"].isObject()) {
        const auto inner = raw["data"].toObject();
        if (inner.contains("data") && inner["data"].isObject()) {
            return inner["data"].toObject();
        }
        return inner;
    }
    return raw;
}

QString AuthManager::generate_device_id() const {
    QString info =
        QSysInfo::machineHostName() + "|" + QSysInfo::productType() + "|" + QSysInfo::currentCpuArchitecture();
    QByteArray hash = QCryptographicHash::hash(info.toUtf8(), QCryptographicHash::Sha256).toHex();
    QString timestamp = QString::number(QDateTime::currentMSecsSinceEpoch(), 36);
    quint32 random = QRandomGenerator::global()->generate();
    return QString("fincept_desktop_%1_%2_%3")
        .arg(QString::fromUtf8(hash.left(16)), timestamp, QString::number(random, 36));
}

// ── Token helper — single place to set/clear tokens on HttpClient ────────────

static void apply_tokens(const QString& api_key, const QString& session_token) {
    auto& http = fincept::HttpClient::instance();
    http.set_auth_header(api_key);
    http.set_session_token(session_token);
}

static void clear_tokens() {
    apply_tokens({}, {});
}

// ── Session persistence (SQLite via SettingsRepository) ──────────────────────

bool AuthManager::write_session_blob() {
    // CR-08: only non-secret session fields go to the unencrypted settings table;
    // the api_key and session_token live in SecureStorage (AES-256-GCM). The one
    // exception is a plaintext secret an older build left in this blob whose move
    // into SecureStorage is not verified yet: it is written back as it was, never
    // dropped (retire_legacy_copies() removes it once the move is confirmed).
    QJsonObject obj = session_.to_persisted_json();
    if (!legacy_.blob_api_key.isEmpty())
        obj["api_key"] = legacy_.blob_api_key;
    if (!legacy_.blob_session_token.isEmpty())
        obj["session_token"] = legacy_.blob_session_token;
    const QString json = QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    auto r = fincept::SettingsRepository::instance().set("fincept_session", json, "auth");
    if (r.is_err()) {
        LOG_ERROR("Auth", logf("Session details could not be saved", {{"error", QString::fromStdString(r.error())}}));
        return false;
    }
    return true;
}

namespace {

/// One line when storing a credential starts failing, one when it works again —
/// save_session() runs after every profile refresh. The key name only.
void auth_log_credential_store(const char* name, const Result<void>& r) {
    const bool failing = r.is_err();
    LOG_TRANSITION("Auth", QStringLiteral("credential_store|") + QLatin1String(name), failing,
                   failing ? LogLevel::Error : LogLevel::Info,
                   failing ? logf("Credential could not be stored", {{"key", name}, {"error", QString::fromStdString(r.error())}})
                           : logf("Credential stored again", {{"key", name}}));
}

} // namespace

void AuthManager::save_session() {
    write_session_blob();

    // Secrets → SecureStorage only, each confirmed by reading it back. While the
    // saved credentials could not be read, an empty in-memory value is "unknown",
    // not "none": never delete what we could not read.
    auto& secure = fincept::SecureStorage::instance();
    const bool may_remove = !saved_session_failure_.failed();
    bool key_verified = false;
    bool token_verified = false;
    if (!session_.api_key.isEmpty()) {
        const auto sr = secure.store_verified(QStringLiteral("api_key"), session_.api_key);
        key_verified = sr.is_ok();
        auth_log_credential_store("api_key", sr);
    } else if (may_remove) {
        secure.remove(QStringLiteral("api_key"));
    }

    if (!session_.session_token.isEmpty()) {
        const auto st = secure.store_verified(QStringLiteral("session_token"), session_.session_token);
        token_verified = st.is_ok();
        auth_log_credential_store("session_token", st);
    } else if (may_remove) {
        secure.remove(QStringLiteral("session_token"));
    }

    retire_legacy_copies(key_verified, token_verified);
}

void AuthManager::load_session() {
    auto r = fincept::SettingsRepository::instance().get("fincept_session");
    if (r.is_err())
        LOG_WARN("Auth", logf("Saved session details could not be read",
                              {{"error", QString::fromStdString(r.error())}}));
    if (r.is_ok() && !r.value().isEmpty()) {
        QJsonParseError perr{};
        auto doc = QJsonDocument::fromJson(r.value().toUtf8(), &perr);
        if (!doc.isNull())
            session_ = SessionData::from_json(doc.object());
        else // never the blob itself
            LOG_ERROR("Auth", logf("Saved session details are not valid JSON, ignored",
                                   {{"bytes", r.value().size()}, {"error", perr.errorString()}}));
    }

    // A blob written by a pre-CR-08 build carries the API key and session token
    // in clear: from here on those are OLD copies, moved (or kept) below. The
    // session's secrets come from SecureStorage.
    legacy_ = LegacyCopies{};
    legacy_.blob_api_key = session_.api_key;
    legacy_.blob_session_token = session_.session_token;
    session_.api_key.clear();
    session_.session_token.clear();

    // Sets session_.api_key / session_token and saved_session_failure_. A read
    // ERROR of the store is not "signed out": initialize() then shows the blocked
    // state instead of the login screen (a fresh login would overwrite the key it
    // could not read), and no plaintext copy is moved or deleted meanwhile.
    migrate_legacy_plaintext_credentials();

    // Never trust saved authenticated flag — must be re-validated
    session_.authenticated = false;
}

void AuthManager::migrate_legacy_plaintext_credentials() {
    namespace sm = fincept::secret_migration;
    auto& settings = fincept::SettingsRepository::instance();
    auto& secure = fincept::SecureStorage::instance();

    // The standalone plaintext row. A read error is NOT "the row is absent": the
    // row may hold the only copy of the key, so it is neither moved nor deleted.
    sm::OldCopy row_copy;
    const auto row = settings.get("fincept_api_key");
    if (row.is_err())
        row_copy = sm::OldCopy::unreadable(QString::fromStdString(row.error()));
    else
        row_copy = sm::OldCopy::present(row.value());

    auto io_for = [&secure](const QString& key, std::function<Result<void>()> delete_old) {
        sm::Io io;
        io.read_stored = [&secure, key]() { return secure.read(key); };
        io.write_stored = [&secure, key](const QString& v) { return secure.store(key, v); };
        io.delete_old = std::move(delete_old);
        return io;
    };
    // The blob copies are removed by ONE rewrite of the blob after the moves (the
    // driver only marks them; the rewrite below may still fail and keep them).
    bool scrub_blob_key = false;
    bool scrub_blob_token = false;
    const sm::Outcome from_blob_key =
        sm::run(sm::OldCopy::present(legacy_.blob_api_key), sm::OnConflict::KeepBoth,
                io_for(QStringLiteral("api_key"), [&scrub_blob_key]() {
                    scrub_blob_key = true;
                    return Result<void>::ok();
                }));
    const sm::Outcome from_row_key =
        sm::run(row_copy, sm::OnConflict::KeepBoth,
                io_for(QStringLiteral("api_key"), [&settings]() { return settings.remove("fincept_api_key"); }));
    const sm::Outcome from_blob_token =
        sm::run(sm::OldCopy::present(legacy_.blob_session_token), sm::OnConflict::KeepBoth,
                io_for(QStringLiteral("session_token"), [&scrub_blob_token]() {
                    scrub_blob_token = true;
                    return Result<void>::ok();
                }));

    // What is still on disk after this run.
    if (row_copy.is_present() && !from_row_key.old_deleted)
        legacy_.row_api_key = row_copy.value;
    if (scrub_blob_key || scrub_blob_token) {
        LegacyCopies before = legacy_;
        if (scrub_blob_key)
            legacy_.blob_api_key.clear();
        if (scrub_blob_token)
            legacy_.blob_session_token.clear();
        if (!write_session_blob()) {
            legacy_.blob_api_key = before.blob_api_key; // still in the blob: kept, retried
            legacy_.blob_session_token = before.blob_session_token;
            scrub_blob_key = scrub_blob_token = false;
        }
    }

    // The session's secrets: the stored value; when the store holds none (the
    // move failed), the old copy in memory — a failed move never signs out.
    saved_session_failure_ = SecretRead::not_found();
    const SecretRead stored_key = secure.read(QStringLiteral("api_key"));
    const SecretRead stored_token = secure.read(QStringLiteral("session_token"));
    if (stored_key.failed())
        saved_session_failure_ = stored_key;
    else if (stored_token.failed())
        saved_session_failure_ = stored_token;
    if (stored_key.found() && !stored_key.value.isEmpty())
        session_.api_key = stored_key.value;
    else if (!stored_key.failed())
        session_.api_key = !legacy_.blob_api_key.isEmpty() ? legacy_.blob_api_key : legacy_.row_api_key;
    if (stored_token.found() && !stored_token.value.isEmpty())
        session_.session_token = stored_token.value;
    else if (!stored_token.failed())
        session_.session_token = legacy_.blob_session_token;

    // Both readable and different: the stored key is used; the old one is the
    // start-up fallback if the server refuses the stored one.
    if (from_blob_key.failure == sm::Failure::Conflict)
        legacy_.conflicting_api_key = legacy_.blob_api_key;
    else if (from_row_key.failure == sm::Failure::Conflict)
        legacy_.conflicting_api_key = legacy_.row_api_key;

    // One WARN per session while a copy stays behind; the names and the failure
    // kind only, never a value.
    const auto blob_key_state = scrub_blob_key ? sm::Failure::None : from_blob_key.failure;
    const auto blob_token_state = scrub_blob_token ? sm::Failure::None : from_blob_token.failure;
    const bool pending = blob_key_state != sm::Failure::None || from_row_key.failure != sm::Failure::None ||
                         blob_token_state != sm::Failure::None;
    if (pending) {
        QString detail = from_blob_key.detail;
        if (detail.isEmpty())
            detail = from_row_key.detail;
        if (detail.isEmpty())
            detail = from_blob_token.detail;
        LOG_TRANSITION("Auth", QStringLiteral("legacy_credentials_move"), 1, LogLevel::Warn,
                       logf("Legacy plaintext credentials kept, the move into secure storage is retried at the next "
                            "start",
                            {{"api_key_blob", sm::failure_name(blob_key_state)},
                             {"api_key_row", sm::failure_name(from_row_key.failure)},
                             {"session_token_blob", sm::failure_name(blob_token_state)},
                             {"error", detail.left(200)}}));
    } else {
        LOG_TRANSITION("Auth", QStringLiteral("legacy_credentials_move"), 0, LogLevel::Info,
                       QStringLiteral("Legacy plaintext credentials moved into secure storage"));
    }
    const int moved = (scrub_blob_key ? 1 : 0) + (scrub_blob_token ? 1 : 0) + (from_row_key.old_deleted ? 1 : 0);
    if (moved > 0)
        LOG_INFO("Auth", logf("Legacy plaintext credentials moved into secure storage", {{"copies", moved}}));
}

void AuthManager::retire_legacy_copies(bool key_stored_verified, bool token_stored_verified) {
    if (legacy_.blob_api_key.isEmpty() && legacy_.blob_session_token.isEmpty() && legacy_.row_api_key.isEmpty())
        return;
    // A copy EQUAL to the value in use goes once the store was just verified to
    // hold that value. One that DIFFERS is superseded once the server accepted the
    // key in use — for a key, only while that key is itself safely stored.
    namespace sm = fincept::secret_migration;
    const bool key_confirmed = !confirmed_api_key_.isEmpty() && confirmed_api_key_ == session_.api_key;
    auto may_retire_key = [&](const QString& copy) {
        return sm::may_retire_copy(copy, session_.api_key, key_stored_verified, key_confirmed && key_stored_verified);
    };
    auto may_retire_token = [&](const QString& copy) {
        return sm::may_retire_copy(copy, session_.session_token, token_stored_verified, key_confirmed);
    };

    int retired = 0;
    const LegacyCopies before = legacy_;
    bool blob_changed = false;
    if (may_retire_key(legacy_.blob_api_key)) {
        legacy_.blob_api_key.clear();
        blob_changed = true;
    }
    if (may_retire_token(legacy_.blob_session_token)) {
        legacy_.blob_session_token.clear();
        blob_changed = true;
    }
    if (blob_changed) {
        if (write_session_blob()) {
            retired += (before.blob_api_key != legacy_.blob_api_key ? 1 : 0) +
                       (before.blob_session_token != legacy_.blob_session_token ? 1 : 0);
        } else {
            legacy_.blob_api_key = before.blob_api_key;
            legacy_.blob_session_token = before.blob_session_token;
        }
    }
    if (may_retire_key(legacy_.row_api_key)) {
        if (fincept::SettingsRepository::instance().remove("fincept_api_key").is_ok()) {
            legacy_.row_api_key.clear();
            ++retired;
        }
    }
    if (legacy_.blob_api_key.isEmpty() && legacy_.row_api_key.isEmpty())
        legacy_.conflicting_api_key.clear();
    if (retired > 0) {
        LOG_INFO("Auth", logf("Legacy plaintext credentials removed after the move was confirmed", {{"copies", retired}}));
        if (legacy_.blob_api_key.isEmpty() && legacy_.blob_session_token.isEmpty() && legacy_.row_api_key.isEmpty())
            LOG_TRANSITION("Auth", QStringLiteral("legacy_credentials_move"), 0, LogLevel::Info,
                           QStringLiteral("Legacy plaintext credentials moved into secure storage"));
    }
}

bool AuthManager::try_conflicting_legacy_key(const QString& refused_key) {
    const QString alt = legacy_.conflicting_api_key;
    legacy_.conflicting_api_key.clear(); // one try
    if (alt.isEmpty() || alt == refused_key)
        return false;
    LOG_WARN("Auth", "Saved API key refused (401); trying the older saved key before signing out");
    session_.api_key = alt;
    fincept::HttpClient::instance().set_auth_header(alt);
    validating_saved_session_ = true;
    fetch_user_profile([this] { emit subscription_fetched(); });
    return true;
}

QString AuthManager::fincept_api_key() const {
    // Single supported resolver. Prefer the live in-memory session; fall back to
    // the encrypted SecureStorage copy. Never reads the legacy plaintext row.
    if (!session_.api_key.isEmpty())
        return session_.api_key;
    // Read-only fallback: an error yields "no key" for this call only — nothing is
    // written or deleted on its basis (the read failure is logged by SecureStorage).
    const SecretRead secure_key = fincept::SecureStorage::instance().read(QStringLiteral("api_key"));
    if (secure_key.found())
        return secure_key.value;
    return {};
}

void AuthManager::retry_saved_session() {
    if (!saved_session_failure_.failed())
        return;
    LOG_INFO("Auth", "Retrying the saved session read");
    initialize();
}

void AuthManager::clear_session() {
    session_ = SessionData{};
    forget_etags(); // a 304 must never refer to another session's copy
    session_token_unverified_ = false;
    reset_user_data_refresh();
    user_data_fetched_ms_ = 0;
    next_background_refresh_ms_ = 0;
    announced_balance_ = -1.0;
    clear_tokens();
    // A sign-out (or a key the server refused): the plaintext copies an older
    // build left go with the rest.
    fincept::SettingsRepository::instance().remove("fincept_session");
    fincept::SettingsRepository::instance().remove("fincept_api_key");
    fincept::SecureStorage::instance().remove("api_key");
    fincept::SecureStorage::instance().remove("session_token");
    legacy_ = LegacyCopies{};
    confirmed_api_key_.clear();
    // An explicit sign-out: whatever could not be read is now deliberately gone.
    saved_session_failure_ = SecretRead::not_found();

    // PIN intentionally NOT cleared here. The PIN is a local-device credential,
    // independent of the backend session. Wiping it on every logout means a
    // transient session expiry (SessionGuard 401 path, explicit Logout) trains
    // the user to keep choosing fresh PINs and destroys the audit trail across
    // sessions. The only path that should reset the PIN is the max-attempts
    // re-auth flow (LockScreen → reauth_requested), and that path should call
    // PinManager::clear_pin() explicitly before invoking logout().

    // Clear auto-configured fincept LLM provider and reset LlmService
    LlmConfigRepository::instance().delete_provider("fincept");
}

bool AuthManager::needs_pin_setup() const {
    // Only a PIN record that was READ and is absent needs setup — never an
    // unreadable one (setting a PIN over it would overwrite the real PIN).
    return session_.authenticated && PinManager::instance().pin_state() == PinManager::PinState::NotSet;
}

// ── Initialize ───────────────────────────────────────────────────────────────

void AuthManager::initialize() {
    set_loading(true);
    load_session();

    if (saved_session_failure_.failed()) {
        // Neither "signed in" nor "signed out": WindowFrame shows the blocked
        // state with Retry (retry_saved_session) until the store answers.
        LOG_WARN("Auth", QStringLiteral("Saved session could not be read kind=%1 — waiting for secure storage")
                             .arg(QLatin1String(saved_session_failure_.error_name())));
        set_loading(false);
        emit auth_state_changed();
        return;
    }

    if (!session_.api_key.isEmpty()) {
        // Apply api_key ONLY — do NOT send the stale session_token during
        // startup validation. The server enforces single-session via
        // X-Session-Token; sending a stale one triggers 401 even though
        // the api_key is perfectly valid and permanent.
        auto& http = fincept::HttpClient::instance();
        http.set_auth_header(session_.api_key);
        http.clear_session_token();
        // Put back on HttpClient only after verify_saved_session_token() has
        // confirmed it (it may have been replaced while the app was closed).
        session_token_unverified_ = !session_.session_token.isEmpty();

        validate_saved_session();
        return;
    }

    set_loading(false);
    emit auth_state_changed();
}

void AuthManager::validate_saved_session() {
    // Validate the saved api_key by fetching the user profile.
    // We intentionally do NOT send X-Session-Token here — the api_key is
    // the permanent credential. A stale session_token would cause a false 401.
    //
    // Flow:
    //   api_key valid (200) → check the saved session token (in parallel) →
    //                         fetch subscription → mark authenticated → save
    //   api_key revoked (401) → clear everything → show login
    //   network error / 5xx / 403 (IP block) → trust cached session data
    // DEBUG: the outcome ("Saved session token is still valid", a 401 sign-out, or
    // "Profile fetch failed … using cached data") is the event.
    LOG_DEBUG("Auth", "Validating saved session via profile fetch (api_key only, no session_token)");
    validating_saved_session_ = true;
    fetch_user_profile([this] { emit subscription_fetched(); });
}

void AuthManager::fetch_user_profile(std::function<void()> on_done, std::function<void(const QString&)> on_rejected) {
    const QString key_at_start = session_.api_key;
    // Conditional when this run already holds a full answer for this key: an
    // unchanged profile is then a bodiless 304 (old server: no ETag, so never sent).
    const QString if_none_match = profile_etag_.if_none_match_for(key_at_start);
    AuthApi::instance().get_user_profile([this, key_at_start, on_done = std::move(on_done),
                                          on_rejected = std::move(on_rejected)](ApiResponse r) mutable {
        // Only a 401 refuses the key. A 403 from this API is never an account
        // verdict — it is the IP block (403 IP_BLOCKED) or a proxy refusing us —
        // and treating it as "key invalid" logged users out and then told them
        // their account "may be suspended". A 401 caused by a stale session token
        // never reaches here: HttpClient drops the token and re-sends the request
        // with the key alone.
        const bool startup_check = std::exchange(validating_saved_session_, false);
        if (!r.success && r.status_code == 401) {
            if (key_rotation_in_flight_ || session_.api_key != key_at_start) {
                // Sent with a key that regenerate_api_key() has just replaced.
                LOG_INFO("Auth", "Profile fetch refused an API key that has just been replaced — ignored");
                return;
            }
            // Start-up, and an older saved key differs from the refused one: the
            // server decides which is current before anything is deleted.
            if (startup_check && try_conflicting_legacy_key(key_at_start))
                return;
            // API key is revoked or invalid — force re-login
            LOG_WARN("Auth", "Profile fetch returned 401 — API key invalid, clearing session");
            clear_session();
            set_loading(false);
            // A login/OTP/MFA attempt that got this far would otherwise never hear it
            // failed (only auth_state_changed fires) — tell the flow that started it,
            // before auth_state_changed so screens' fallbacks find themselves reset.
            if (on_rejected)
                on_rejected(tr("Sign-in could not be completed: the server rejected the new session. "
                               "Please try again."));
            emit auth_state_changed();
            return;
        }
        if (session_.api_key != key_at_start) {
            // Signed out / into another account, or the key was rotated, while
            // this was in flight: the profile is not this session's.
            LOG_INFO("Auth", "Profile answer for an API key no longer in use — ignored");
            if (session_.api_key.isEmpty() && is_loading_) {
                // Signed out during startup validation: nothing else ends the
                // loading state, and screens wait for it.
                set_loading(false);
                emit auth_state_changed();
            }
            return;
        }
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        // The server's pacing for this endpoint (X-Poll-Interval, on success and
        // error answers alike). Kept until a later answer says otherwise.
        if (r.poll_interval_ms > 0)
            profile_poll_hint_ms_ = r.poll_interval_ms;
        if (r.success) {
            if (r.status_code == 304) {
                // Unchanged since the copy in session_ (only ever asked with the
                // ETag of a full answer for this same key): keep it — it is
                // simply fresh as of now. Never parsed as an (empty) profile.
                LOG_DEBUG("Auth", "Profile unchanged (304) — keeping the cached copy");
            } else {
                const auto data = unwrap_data(r.data);
                session_.user_info = UserProfile::from_json(data);
                // An answer without ETag (older server) clears it: the next
                // fetch goes out unconditional, as before.
                profile_etag_ = {r.etag, key_at_start};
            }
            user_data_fetched_ms_ = now;
            confirmed_api_key_ = key_at_start; // lets save_session() retire superseded plaintext copies
            const qint64 jitter =
                bg_refresh_jitter_ms_ > 0 ? QRandomGenerator::global()->bounded(bg_refresh_jitter_ms_) : 0;
            next_background_refresh_ms_ = now + std::max<qint64>(bg_refresh_base_ms_ + jitter, profile_poll_hint_ms_);

            // api_key confirmed valid. A token saved by the previous run is
            // checked before it goes back on HttpClient; a login's fresh token
            // is already there (apply_tokens).
            if (session_token_unverified_)
                verify_saved_session_token();
        } else {
            // Network/server error or IP block — keep going with cached session data.
            // A background refresh tries again in a few minutes, not on the next tick
            // (and never sooner than the server's X-Poll-Interval).
            next_background_refresh_ms_ = now + std::max<qint64>(bg_refresh_base_ms_ / 6, profile_poll_hint_ms_);
            LOG_WARN("Auth", QString("Profile fetch failed (HTTP %1, not an auth verdict) — using cached data")
                                 .arg(r.status_code));
            session_.authenticated = !session_.api_key.isEmpty();
            // Can't check the saved token now (the check would fail the same
            // way): restore it as before. If it turns out to be stale, the first
            // 401 makes HttpClient drop it and AuthManager recover.
            if (session_token_unverified_) {
                session_token_unverified_ = false;
                if (!session_.session_token.isEmpty())
                    fincept::HttpClient::instance().set_session_token(session_.session_token);
            }
        }
        fetch_user_subscription(std::move(on_done));
    }, nullptr, if_none_match);
}

void AuthManager::verify_saved_session_token() {
    session_token_unverified_ = false;
    const QString token = session_.session_token;
    if (token.isEmpty())
        return;
    AuthApi::instance().check_session_token(token, [this, token](ApiResponse r) {
        if (session_.session_token != token)
            return; // replaced by a login, or already forgotten, meanwhile
        if (r.success) {
            fincept::HttpClient::instance().set_session_token(token);
            LOG_INFO("Auth", "Saved session token is still valid — restored");
            return;
        }
        if (r.status_code == 401) {
            // The key was just accepted, so the token is what was refused: the
            // account signed in elsewhere or reset its password since last run.
            // Same outcome as session recovery: carry on with the key alone.
            LOG_INFO("Auth", "Saved session token is no longer valid — continuing with the API key only");
            session_.session_token.clear();
            save_session();
            return;
        }
        // Offline, 5xx or IP block: can't tell. Restore it as before; a stale
        // token is dropped on its first 401 (HttpClient → handle_unauthorized).
        LOG_WARN("Auth", QString("Could not check the saved session token (HTTP %1) — restoring it").arg(r.status_code));
        fincept::HttpClient::instance().set_session_token(token);
    });
}

// ── Shared post-auth completion ───────────────────────────────────────────────
// Fetches subscription, syncs state, saves session, then calls on_done().
// Used by login / verify_otp / verify_mfa / session restore.

void AuthManager::complete_auth_flow(std::function<void()> on_done) {
    const QString key_at_start = session_.api_key;
    // Same conditional GET as the profile, once the server sends an ETag here.
    const QString if_none_match = subscription_etag_.if_none_match_for(key_at_start);
    UserApi::instance().get_user_subscription([this, key_at_start, on_done = std::move(on_done)](ApiResponse r) {
        const QString plan_before = session_.subscription.account_type;
        if (r.success && r.status_code == 304) {
            // Unchanged: session_.subscription still is the server's record.
            LOG_DEBUG("Auth", "Subscription unchanged (304) — keeping the cached copy");
        } else if (r.success) {
            const auto sub_data = unwrap_data(r.data);
            session_.subscription = UserSubscription::from_json(sub_data);
            session_.has_subscription = !session_.subscription.account_type.isEmpty();
            if (session_.api_key == key_at_start)
                subscription_etag_ = {r.etag, key_at_start}; // none (today's server) → stays unconditional
        }
        // Fallback: promote profile account_type into subscription so account_type() is consistent
        if (session_.subscription.account_type.isEmpty() && !session_.user_info.account_type.isEmpty()) {
            session_.subscription.account_type = session_.user_info.account_type;
            session_.subscription.credit_balance = session_.user_info.credit_balance;
            session_.has_subscription = true;
        }
        // Sync user_info from subscription so legacy readers stay consistent
        if (!session_.subscription.account_type.isEmpty())
            session_.user_info.account_type = session_.subscription.account_type;
        if (session_.subscription.credit_balance > 0)
            session_.user_info.credit_balance = session_.subscription.credit_balance;
        if (!plan_before.isEmpty() && session_.subscription.account_type != plan_before)
            LOG_INFO("Auth", logf("Plan changed", {{"from", plan_before}, {"to", session_.subscription.account_type}}));

        if (!session_.api_key.isEmpty())
            session_.authenticated = true;
        save_session();
        auto_configure_fincept_llm();
        set_loading(false);
        if (on_done)
            on_done();
        emit auth_state_changed();
        announce_credit_balance();
    }, nullptr, if_none_match);
}

void AuthManager::forget_etags() {
    profile_etag_ = {};
    subscription_etag_ = {};
}

void AuthManager::fetch_user_subscription(std::function<void()> on_done) {
    complete_auth_flow(std::move(on_done));
}

// ── Login ────────────────────────────────────────────────────────────────────

void AuthManager::login(const QString& email, const QString& password, bool force_login) {
    set_loading(true);

    LoginRequest req;
    req.email = sanitize_input(email).toLower();
    req.password = password;
    req.force_login = force_login;

    AuthApi::instance().login(req, [this, login_email = req.email](ApiResponse r) {
        // Never the e-mail address or the password; the server's message is scrubbed.
        if (!r.success) {
            set_loading(false);
            // The only 403 /user/login answers itself is "account not verified"
            // (sent after the password checked out). The other 403 — the IP
            // block — has already been flagged by HttpClient before this runs.
            if (r.status_code == 403 && !fincept::HttpClient::instance().is_ip_blocked()) {
                LOG_WARN("Auth", logf("Sign-in failed", {{"method", "password"},
                                                         {"reason", "unverified"},
                                                         {"status", r.status_code}}));
                emit login_unverified(login_email, r.error);
                return;
            }
            LOG_WARN("Auth", logf("Sign-in failed", {{"method", "password"},
                                                     {"reason", "rejected"},
                                                     {"status", r.status_code},
                                                     {"error", log_scrub(r.error)}}));
            emit login_failed(r.error.isEmpty() ? tr("Login failed") : r.error);
            return;
        }

        const auto data = unwrap_data(r.data);

        if (data["active_session"].toBool()) {
            set_loading(false);
            LOG_WARN("Auth", logf("Sign-in failed", {{"method", "password"}, {"reason", "active_session"}}));
            emit login_active_session(data["message"].toString(tr("You are already logged in on another device.")));
            return;
        }

        if (data["mfa_required"].toBool()) {
            set_loading(false);
            LOG_INFO("Auth", logf("Sign-in waiting for a second factor", {{"method", "password"},
                                                                         {"reason", "mfa_required"}}));
            emit login_mfa_required();
            return;
        }

        const QString api_key = data["api_key"].toString();
        if (api_key.isEmpty()) {
            set_loading(false);
            LOG_WARN("Auth", logf("Sign-in failed", {{"method", "password"},
                                                     {"reason", "no_api_key"},
                                                     {"status", r.status_code}}));
            emit login_failed(tr("No API key returned from server"));
            return;
        }

        const QString session_token = data["session_token"].toString();

        // Set tokens on HttpClient — all subsequent API calls will use them
        apply_tokens(api_key, session_token);

        session_.authenticated = true;
        session_.api_key = api_key;
        session_.session_token = session_token;
        session_.device_id = generate_device_id();

        // Fetch profile then subscription; on_done emits login_succeeded
        fetch_user_profile(
            [this] {
                LOG_AUDIT("Auth", "Signed in", {{"method", "password"}});
                emit login_succeeded();
            },
            [this](const QString& msg) { emit login_failed(msg); });
    });
}

// ── Google desktop login (website loopback handoff) ──────────────────────────

void AuthManager::login_with_google() {
    set_loading(true);

    // Owned by AuthManager; self-deletes after emitting code_received/failed.
    auto* flow = new GoogleDesktopLogin(this);
    google_flow_ = flow;

    connect(flow, &GoogleDesktopLogin::code_received, this, [this](const QString& code) {
        AuthApi::instance().redeem_desktop_handoff(code, [this](ApiResponse r) {
            if (!r.success) {
                set_loading(false);
                LOG_WARN("Auth", logf("Sign-in failed", {{"method", "google"},
                                                         {"reason", "handoff_redeem_failed"},
                                                         {"status", r.status_code},
                                                         {"error", log_scrub(r.error)}}));
                emit login_failed(r.error.isEmpty() ? tr("Google login failed") : r.error);
                return;
            }
            const auto data = unwrap_data(r.data);
            complete_desktop_login(data["api_key"].toString(), data["session_token"].toString());
        });
    });

    connect(flow, &GoogleDesktopLogin::failed, this, [this](const QString& message) {
        set_loading(false);
        emit login_failed(message);
    });

    flow->start();
}

bool AuthManager::cancel_google_login() {
    if (!google_flow_ || !google_flow_->is_pending())
        return false;
    LOG_INFO("Auth", "Google sign-in cancelled by the user while waiting for the browser");
    // Emits failed → login_failed (above), stops the listener and self-deletes.
    google_flow_->cancel(tr("Google sign-in cancelled."));
    return true;
}

void AuthManager::complete_desktop_login(const QString& api_key, const QString& session_token) {
    if (api_key.isEmpty()) {
        set_loading(false);
        emit login_failed(tr("No API key returned from server"));
        return;
    }

    // Mirror the login() success tail exactly so the desktop handoff lands in
    // the same post-login machinery (profile + subscription fetch, save_session,
    // fincept LLM auto-config, login_succeeded).
    apply_tokens(api_key, session_token);

    session_.authenticated = true;
    session_.api_key = api_key;
    session_.session_token = session_token;
    session_.device_id = generate_device_id();

    fetch_user_profile(
        [this] {
            LOG_AUDIT("Auth", "Signed in", {{"method", "google"}});
            emit login_succeeded();
        },
        [this](const QString& msg) { emit login_failed(msg); });
}

// ── Signup ───────────────────────────────────────────────────────────────────

void AuthManager::signup(const QString& username, const QString& email, const QString& password, const QString& phone,
                         const QString& country, const QString& country_code) {
    set_loading(true);

    RegisterRequest req;
    req.username = sanitize_input(username).toLower();
    req.email = sanitize_input(email).toLower();
    req.password = password;
    req.phone = phone;
    req.country = country;
    req.country_code = country_code;

    AuthApi::instance().register_user(req, [this](ApiResponse r) {
        set_loading(false);
        if (r.success)
            emit signup_succeeded();
        else
            emit signup_failed(r.error.isEmpty() ? tr("Registration failed") : r.error);
    });
}

// ── Signup code resend ───────────────────────────────────────────────────────
// The dedicated endpoint — re-submitting the whole registration for a resend
// sent nothing inside the server's cooldown yet reported success, and needed the
// password the user may no longer have on screen (signing in unverified).

void AuthManager::resend_signup_otp(const QString& email) {
    AuthApi::instance().resend_otp(sanitize_input(email).toLower(), [this](ApiResponse r) {
        if (r.success) {
            const QString msg = r.data.value("message").toString().trimmed();
            emit otp_resent(msg.isEmpty() ? tr("If the account still needs verifying, a new code has been sent.")
                                          : msg);
        } else {
            emit otp_resend_failed(r.error.isEmpty() ? tr("Could not send a new code. Please try again.") : r.error);
        }
    });
}

// ── OTP verification ─────────────────────────────────────────────────────────

void AuthManager::verify_otp(const QString& email, const QString& otp) {
    set_loading(true);

    VerifyOtpRequest req;
    req.email = sanitize_input(email).toLower();
    req.otp = sanitize_input(otp);

    AuthApi::instance().verify_otp(req, [this](ApiResponse r) {
        if (!r.success) {
            set_loading(false);
            emit otp_failed(r.error.isEmpty() ? tr("Verification failed") : r.error);
            return;
        }

        const auto data = unwrap_data(r.data);
        const QString api_key = data["api_key"].toString();
        if (api_key.isEmpty()) {
            set_loading(false);
            emit otp_failed(tr("No API key returned"));
            return;
        }

        const QString session_token = data["session_token"].toString();
        apply_tokens(api_key, session_token);

        session_.authenticated = true;
        session_.api_key = api_key;
        session_.session_token = session_token;
        session_.device_id = generate_device_id();

        LOG_INFO("Auth", "OTP verified successfully");
        fetch_user_profile([this] { emit otp_verified(); }, [this](const QString& msg) { emit otp_failed(msg); });
    });
}

// ── MFA verification ─────────────────────────────────────────────────────────

void AuthManager::verify_mfa(const QString& email, const QString& otp) {
    set_loading(true);

    AuthApi::instance().verify_mfa(sanitize_input(email).toLower(), sanitize_input(otp), [this](ApiResponse r) {
        if (!r.success) {
            set_loading(false);
            emit mfa_failed(r.error.isEmpty() ? tr("MFA verification failed") : r.error);
            return;
        }

        const auto data = unwrap_data(r.data);
        const QString api_key = data["api_key"].toString();
        if (api_key.isEmpty()) {
            set_loading(false);
            emit mfa_failed(tr("No API key returned"));
            return;
        }

        const QString session_token = data["session_token"].toString();
        apply_tokens(api_key, session_token);

        session_.authenticated = true;
        session_.api_key = api_key;
        session_.session_token = session_token;
        session_.device_id = generate_device_id();

        LOG_INFO("Auth", "MFA verified successfully");
        fetch_user_profile([this] { emit mfa_verified(); }, [this](const QString& msg) { emit mfa_failed(msg); });
    });
}

// ── Forgot password ──────────────────────────────────────────────────────────

void AuthManager::forgot_password(const QString& email) {
    set_loading(true);

    ForgotPasswordRequest req;
    req.email = sanitize_input(email).toLower();

    AuthApi::instance().forgot_password(req, [this](ApiResponse r) {
        set_loading(false);
        if (r.success)
            emit forgot_password_sent();
        else
            emit forgot_password_failed(r.error.isEmpty() ? tr("Failed to send reset code") : r.error);
    });
}

// ── Reset password ───────────────────────────────────────────────────────────

void AuthManager::reset_password(const QString& email, const QString& otp, const QString& new_password) {
    set_loading(true);

    ResetPasswordRequest req;
    req.email = sanitize_input(email).toLower();
    req.otp = sanitize_input(otp);
    req.new_password = new_password;

    AuthApi::instance().reset_password(req, [this](ApiResponse r) {
        set_loading(false);
        if (r.success)
            emit password_reset_succeeded();
        else
            emit password_reset_failed(r.error.isEmpty() ? tr("Password reset failed") : r.error);
    });
}

// ── Logout ───────────────────────────────────────────────────────────────────

void AuthManager::logout() {
    logout_impl(true);
}

void AuthManager::logout_impl(bool notify_server) {
    if (is_logging_out_)
        return;
    is_logging_out_ = true;

    if (notify_server && !session_.api_key.isEmpty()) {
        AuthApi::instance().logout([](ApiResponse) {});
    }

    clear_session();
    is_logging_out_ = false;
    LOG_AUDIT("Auth", "Signed out", {{"reason", notify_server ? "user" : "session_invalid"}});

    emit logged_out();
    emit auth_state_changed();
}

// ── Session recovery ─────────────────────────────────────────────────────────
// The API answers 401 "session_invalidated" when the X-Session-Token we send is
// no longer the account's (signed in on the website / another machine, password
// reset) even though the API key is still valid. The first such 401 seen by any
// client (HttpClient inspects every reply; other stacks call handle_unauthorized)
// forgets the token everywhere at once and re-validates the key alone:
//   key valid → carry on with the key only (no disruption; a fresh token comes
//               with the next explicit login)
//   key 401   → really signed out: session_expired() + logout
//   anything else (offline, 5xx, 403 IP block) → keep the session
// Until the token is forgotten every request 401s, and the server bans the IP
// after 15 x 401 in 2 min — so this must happen on the first one, not on
// SessionGuard's (hourly, activity-gated) pulse.

void AuthManager::handle_unauthorized(const QString& sent_session_token, bool session_expired) {
    // Copy: callers may pass a reference into session_, which this clears.
    const QString token = sent_session_token;
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(
            this, [this, token, session_expired]() { handle_unauthorized(token, session_expired); },
            Qt::QueuedConnection);
        return;
    }
    // Only a signed-in session is recovered here. During startup validation and
    // the login/OTP/MFA flows fetch_user_profile() owns the outcome.
    if (!session_.authenticated || session_.api_key.isEmpty() || is_logging_out_)
        return;

    if (session_expired) {
        if (token.isEmpty() || token != session_.session_token)
            return; // already forgotten (concurrent 401s) or replaced by a newer login
        LOG_WARN("Auth", logf("Session token rejected by the server",
                              {{"reason", "signed_in_elsewhere_or_password_reset"},
                               {"action", "drop_token_and_recheck_key"}}));
        forget_session_token(token);
        schedule_key_check();
        return;
    }

    // Any other 401 on a request that carried our key: the key may have been
    // revoked (regenerated elsewhere, account suspended or deleted) — or the
    // endpoint used 401 for its own reason ("Invalid password" when deleting the
    // account). Check the key rather than trusting one response; never while we
    // are rotating it ourselves, and at most once per cooldown.
    if (key_rotation_in_flight_ || recovery_in_flight_)
        return;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (last_key_check_ms_ > 0 && now - last_key_check_ms_ < kKeyRecheckCooldownMs)
        return;
    LOG_INFO("Auth", "A request carrying the API key was refused (401) — re-validating the key");
    schedule_key_check();
}

void AuthManager::schedule_key_check() {
    if (recovery_scheduled_)
        return;
    recovery_scheduled_ = true;
    const QString key = session_.api_key;
    QTimer::singleShot(0, this, [this, key]() {
        recovery_scheduled_ = false;
        // The handler of the 401 that triggered this may have dealt with it
        // already (cleared the session, adopted a new key).
        if (!session_.authenticated || session_.api_key != key || recovery_in_flight_ || is_logging_out_ ||
            key_rotation_in_flight_)
            return;
        attempt_session_recovery({});
    });
}

void AuthManager::forget_session_token(const QString& dead_token) {
    if (dead_token.isEmpty() || session_.session_token != dead_token)
        return;
    session_.session_token.clear();
    session_token_unverified_ = false;
    // HttpClient only ever holds session_.session_token or nothing (and has
    // already dropped it when it was the client that saw the 401).
    fincept::HttpClient::instance().clear_session_token();
    save_session(); // also removes it from SecureStorage
    // Readers of session() (ChatModeService, LlmService, ArenaLlmClient) see the
    // change on their next request; CloudClient copies credentials on
    // auth_state_changed. Queued: we may be inside a reply handler.
    QMetaObject::invokeMethod(
        this,
        [this]() {
            if (session_.authenticated)
                emit auth_state_changed();
        },
        Qt::QueuedConnection);
}

void AuthManager::attempt_session_recovery(std::function<void(bool)> cb) {
    if (session_.api_key.isEmpty()) {
        if (cb)
            cb(false);
        return;
    }
    if (cb)
        recovery_waiters_.push_back(std::move(cb));
    if (recovery_in_flight_)
        return; // one check at a time — this caller gets the running one's result
    recovery_in_flight_ = true;

    const QString key = session_.api_key;
    LOG_INFO("Auth", "Session recovery: validating the API key (without the session token)");

    // Key only, whatever HttpClient holds — so a 401 here is about the key and
    // can never be another "session_invalidated" (no recovery loop). The 401 of a
    // refused key is reported by HttpClient as api_key_rejected while this is
    // still in flight, and ignored for that reason.
    AuthApi::instance().validate_api_key([this, key](ApiResponse r) {
        recovery_in_flight_ = false;
        last_key_check_ms_ = QDateTime::currentMSecsSinceEpoch();
        auto waiters = std::move(recovery_waiters_);
        recovery_waiters_.clear();

        bool keep = true;
        if (session_.api_key != key || key_rotation_in_flight_) {
            // Signed out, or the key was (being) rotated, while this was in
            // flight: the answer may be about a key we no longer use.
            LOG_INFO("Auth", "Session recovery: the API key changed meanwhile — result ignored");
        } else if (r.success) {
            // Unconditional fetch (never a 304): a full profile, and its ETag.
            session_.user_info = UserProfile::from_json(unwrap_data(r.data));
            profile_etag_ = {r.etag, key};
            announce_credit_balance();
            LOG_INFO("Auth", "Session recovery: API key is valid — session kept");
        } else if (r.status_code == 401) {
            LOG_WARN("Auth", "Session recovery: API key refused (401) — signing out");
            keep = false;
        } else {
            // Offline, 5xx, or 403 (IP block — never an account verdict).
            LOG_WARN("Auth", QString("Session recovery: inconclusive (HTTP %1) — keeping the session")
                                 .arg(r.status_code));
        }

        for (auto& w : waiters) {
            if (w)
                w(keep);
        }
        if (!keep && session_.authenticated && session_.api_key == key && !key_rotation_in_flight_) {
            LOG_AUDIT("Auth", "Session expired", {{"reason", "api_key_refused"}});
            emit session_expired();
            // The key is dead: POST /user/logout would only add another 401.
            logout_impl(false);
        }
    });
}

// ── API key rotation ─────────────────────────────────────────────────────────

void AuthManager::regenerate_api_key(std::function<void(bool, const QString&)> done) {
    if (!session_.authenticated || session_.api_key.isEmpty()) {
        if (done)
            done(false, tr("You are not signed in."));
        return;
    }
    if (key_rotation_in_flight_) {
        if (done)
            done(false, tr("A new API key is already being generated."));
        return;
    }
    key_rotation_in_flight_ = true;
    const QString old_key = session_.api_key;

    // No context: the server invalidates the old key the moment it answers, so
    // the new one must be adopted even if the screen that asked is gone.
    UserApi::instance().regenerate_api_key([this, old_key, done = std::move(done)](ApiResponse r) {
        key_rotation_in_flight_ = false;
        if (!r.success) {
            QString err = r.error;
            if (r.status_code == 0)
                err = tr("No answer from the server, so it is unknown whether your key was changed. If Fincept "
                         "features stop working, sign out and sign in again.");
            LOG_ERROR("Auth", "API key regeneration failed: " + r.error);
            if (done)
                done(false, err);
            return;
        }
        if (session_.api_key != old_key) {
            // Signed out (or into another account) while this was in flight.
            if (done)
                done(false, tr("The session changed while the key was being regenerated. Please sign in again."));
            return;
        }
        // {"success":true,"data":{"api_key":"<new>","message":...}} — apiback user.py regenerate_api_key
        const QString new_key = unwrap_data(r.data).value("api_key").toString();
        if (new_key.isEmpty()) {
            // The old key is already dead and we were not given the new one;
            // only a fresh sign-in can recover.
            LOG_ERROR("Auth", "API key regenerated but the response carried no key");
            if (done)
                done(false, tr("The server changed your API key but did not return the new one. Please sign out "
                               "and sign in again."));
            return;
        }
        adopt_api_key(new_key);
        if (done)
            done(true, {});
    });
}

void AuthManager::adopt_api_key(const QString& key) {
    session_.api_key = key;
    forget_etags(); // the stored ETags belong to the old key
    fincept::HttpClient::instance().set_auth_header(key);
    save_session(); // SecureStorage "api_key"
    // The event line is ProfileScreen's "API key regenerated" (the only caller).
    LOG_DEBUG("Auth", "Adopted the regenerated API key");
    // Everything else reads the key live (session().api_key / fincept_api_key():
    // LlmService, QuantLibClient, ChatModeService, Alpha Arena); CloudClient
    // copies it on auth_state_changed, which this refresh emits. A refresh
    // still in flight was sent with the old key: drop it, start afresh.
    reset_user_data_refresh();
    refresh_user_data(Refresh::Now);
}

// ── Refresh user data ────────────────────────────────────────────────────────
// Profile + subscription used to be re-fetched every 3 minutes per window, on
// every focus change and on every Plans screen show — most of the API's traffic.
// They now come at sign-in, after the actions that change them, and otherwise
// at most every 30–40 min while someone is using the terminal. Concurrent
// callers share one request.

void AuthManager::refresh_user_data(Refresh mode) {
    if (!session_.authenticated || session_.api_key.isEmpty() || is_logging_out_)
        return;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (refresh_in_flight_ && now - refresh_started_ms_ > kRefreshInFlightTimeoutMs) {
        LOG_WARN("Auth", "User-data refresh got no answer — releasing it");
        reset_user_data_refresh();
    }
    if (mode == Refresh::Now) {
        if (refresh_in_flight_) {
            // The running request may predate what the caller just changed:
            // one more after it, shared by every caller until then.
            refresh_again_ = true;
            return;
        }
    } else {
        if (refresh_in_flight_)
            return; // its answer serves this caller too
        if (mode == Refresh::OnView) {
            if (user_data_fetched_ms_ > 0 && now - user_data_fetched_ms_ < kOnViewMaxAgeMs)
                return;
        } else {
            if (now < next_background_refresh_ms_)
                return;
            // Idle, locked, minimised or hidden: nobody would see the result.
            if (!InactivityGuard::instance().user_active())
                return;
        }
        if (fincept::HttpClient::instance().is_ip_blocked())
            return;
    }
    start_user_data_refresh();
}

void AuthManager::start_user_data_refresh() {
    refresh_in_flight_ = true;
    refresh_again_ = false;
    refresh_started_ms_ = QDateTime::currentMSecsSinceEpoch();
    const quint64 gen = ++refresh_gen_;
    // fetch_user_profile chains into fetch_user_subscription automatically
    fetch_user_profile([this, gen] {
        emit subscription_fetched();
        finish_user_data_refresh(gen);
    });
}

void AuthManager::finish_user_data_refresh(quint64 gen) {
    if (gen != refresh_gen_)
        return; // superseded (sign-out, key rotation, timeout)
    refresh_in_flight_ = false;
    if (!refresh_again_)
        return;
    refresh_again_ = false;
    // Next event-loop turn: the caller of on_done still has auth_state_changed to emit.
    QTimer::singleShot(0, this, [this]() { refresh_user_data(Refresh::Now); });
}

void AuthManager::reset_user_data_refresh() {
    refresh_in_flight_ = false;
    refresh_again_ = false;
    ++refresh_gen_;
}

void AuthManager::set_background_refresh_interval_ms(qint64 base_ms, qint64 jitter_ms) {
    bg_refresh_base_ms_ = std::max<qint64>(1, base_ms);
    bg_refresh_jitter_ms_ = std::max<qint64>(0, jitter_ms);
    if (user_data_fetched_ms_ > 0)
        next_background_refresh_ms_ = user_data_fetched_ms_ + std::max(bg_refresh_base_ms_, profile_poll_hint_ms_);
}

void AuthManager::report_credit_balance(double balance) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, balance]() { report_credit_balance(balance); }, Qt::QueuedConnection);
        return;
    }
    if (!session_.authenticated || !std::isfinite(balance) || balance < 0)
        return;
    session_.user_info.credit_balance = balance;
    session_.subscription.credit_balance = balance;
    // Not persisted here (a SQLite + keychain write per billed call); the next
    // refresh saves the session.
    announce_credit_balance();
}

void AuthManager::announce_credit_balance() {
    if (!session_.authenticated)
        return;
    const double balance = session_.user_info.credit_balance;
    if (announced_balance_ >= 0 && std::abs(balance - announced_balance_) < 0.005)
        return;
    announced_balance_ = balance;
    emit credit_balance_changed(balance);
}

// ── Auto-configure Fincept LLM provider ──────────────────────────────────────

void AuthManager::auto_configure_fincept_llm() {
    if (session_.api_key.isEmpty())
        return;

    // CR-08: do NOT persist the api_key in the plaintext settings table. The
    // key already lives in the in-memory session and in SecureStorage (written
    // by save_session). LlmService and friends resolve it via
    // AuthManager::fincept_api_key(). A plaintext row an older build left is
    // removed by retire_legacy_copies() once its move is confirmed — never here
    // unconditionally (it may hold the only copy when the store is failing).

    // Only create the fincept provider row if it doesn't already exist.
    // This prevents overwriting the user's model/settings choice on every
    // session revalidation (~30s interval).
    auto providers = LlmConfigRepository::instance().list_providers();
    bool fincept_exists = false;
    if (providers.is_ok()) {
        for (const auto& p : providers.value()) {
            if (p.provider.toLower() == "fincept") {
                fincept_exists = true;
                break;
            }
        }
    }

    if (!fincept_exists) {
        LlmConfig fincept_llm;
        fincept_llm.provider = "fincept";
        fincept_llm.model = "MiniMax-M2.7";
        fincept_llm.base_url = {};
        LlmConfigRepository::instance().save_provider(fincept_llm);
        LOG_INFO("Auth", "Created fincept LLM provider config");
    }

    // Set as active if no other provider is currently active
    auto active = LlmConfigRepository::instance().get_active_provider();
    bool has_active = active.is_ok() && !active.value().provider.isEmpty();
    if (!has_active)
        LlmConfigRepository::instance().set_active("fincept");
}

} // namespace fincept::auth
