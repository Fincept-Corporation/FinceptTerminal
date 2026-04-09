#include "auth/AuthManager.h"

#include "auth/AuthApi.h"
#include "auth/PinManager.h"
#include "auth/UserApi.h"
#include "core/logging/Logger.h"
#include "network/http/HttpClient.h"
#include "storage/repositories/LlmConfigRepository.h"
#include "storage/repositories/SettingsRepository.h"
#include "storage/secure/SecureStorage.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonDocument>
#include <QRandomGenerator>
#include <QSysInfo>

namespace fincept::auth {

AuthManager& AuthManager::instance() {
    static AuthManager s;
    return s;
}

AuthManager::AuthManager() {}

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

void AuthManager::save_session() {
    QJsonDocument doc(session_.to_json());
    QString json = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
    auto r = fincept::SettingsRepository::instance().set("fincept_session", json, "auth");
    if (r.is_err()) {
        LOG_ERROR("Auth", "Failed to save session: " + QString::fromStdString(r.error()));
    }

    // Persist api_key to OS-native encrypted storage (DPAPI / Keychain) as the
    // durable credential. SQLite session JSON is the fallback.
    if (!session_.api_key.isEmpty()) {
        auto sr = fincept::SecureStorage::instance().store("api_key", session_.api_key);
        if (sr.is_err())
            LOG_WARN("Auth", "SecureStorage: failed to persist api_key — using SQLite fallback");
    }
}

void AuthManager::load_session() {
    auto r = fincept::SettingsRepository::instance().get("fincept_session");
    if (r.is_ok() && !r.value().isEmpty()) {
        auto doc = QJsonDocument::fromJson(r.value().toUtf8());
        if (!doc.isNull())
            session_ = SessionData::from_json(doc.object());
    }

    // Try to recover api_key from SecureStorage (DPAPI / Keychain) — this is the
    // most reliable source since it survives SQLite corruption and DB migrations.
    auto secure_key = fincept::SecureStorage::instance().retrieve("api_key");
    if (secure_key.is_ok() && !secure_key.value().isEmpty()) {
        if (session_.api_key.isEmpty() || session_.api_key != secure_key.value()) {
            LOG_INFO("Auth", "Restored api_key from SecureStorage");
            session_.api_key = secure_key.value();
        }
    }

    // Never trust saved authenticated flag — must be re-validated
    session_.authenticated = false;
}

void AuthManager::clear_session() {
    session_ = SessionData{};
    clear_tokens();
    fincept::SettingsRepository::instance().remove("fincept_session");
    fincept::SettingsRepository::instance().remove("fincept_api_key");
    fincept::SecureStorage::instance().remove("api_key");

    // Clear PIN and lockout state on logout — user must set up again after re-login
    PinManager::instance().clear_pin();

    // Clear auto-configured fincept LLM provider and reset LlmService
    LlmConfigRepository::instance().delete_provider("fincept");
}

bool AuthManager::needs_pin_setup() const {
    return session_.authenticated && !PinManager::instance().has_pin();
}

// ── Initialize ───────────────────────────────────────────────────────────────

void AuthManager::initialize() {
    set_loading(true);
    load_session();

    if (!session_.api_key.isEmpty()) {
        // Apply api_key ONLY — do NOT send the stale session_token during
        // startup validation. The server enforces single-session via
        // X-Session-Token; sending a stale one triggers 401 even though
        // the api_key is perfectly valid and permanent.
        auto& http = fincept::HttpClient::instance();
        http.set_auth_header(session_.api_key);
        http.clear_session_token();

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
    //   api_key valid (200) → fetch subscription → mark authenticated → save
    //   api_key revoked (401/403) → clear everything → show login
    //   network error → trust cached session data (offline-friendly)
    LOG_INFO("Auth", "Validating saved session via profile fetch (api_key only, no session_token)");
    fetch_user_profile([this] { emit subscription_fetched(); });
}

void AuthManager::fetch_user_profile(std::function<void()> on_done) {
    AuthApi::instance().get_user_profile([this, on_done = std::move(on_done)](ApiResponse r) mutable {
        if (!r.success && (r.status_code == 401 || r.status_code == 403)) {
            // API key is revoked or invalid — force re-login
            LOG_WARN("Auth", "Profile fetch returned 401/403 — API key invalid, clearing session");
            clear_session();
            set_loading(false);
            emit auth_state_changed();
            return;
        }
        if (r.success) {
            const auto data = unwrap_data(r.data);
            session_.user_info = UserProfile::from_json(data);

            // api_key confirmed valid — now restore session_token on HttpClient
            // so subsequent authenticated requests include it. If the old
            // session_token turns out to be stale during usage, SessionGuard
            // will handle recovery without clearing the api_key.
            if (!session_.session_token.isEmpty()) {
                fincept::HttpClient::instance().set_session_token(session_.session_token);
            }
        } else {
            // Network/server error — keep going with cached session data
            LOG_WARN("Auth", "Profile fetch failed (non-auth error) — using cached data");
            session_.authenticated = !session_.api_key.isEmpty();
            // Restore session_token for subsequent requests even in offline mode
            if (!session_.session_token.isEmpty()) {
                fincept::HttpClient::instance().set_session_token(session_.session_token);
            }
        }
        fetch_user_subscription(std::move(on_done));
    });
}

// ── Shared post-auth completion ───────────────────────────────────────────────
// Fetches subscription, syncs state, saves session, then calls on_done().
// Used by login / verify_otp / verify_mfa / session restore.

void AuthManager::complete_auth_flow(std::function<void()> on_done) {
    UserApi::instance().get_user_subscription([this, on_done = std::move(on_done)](ApiResponse r) {
        if (r.success) {
            const auto sub_data = unwrap_data(r.data);
            session_.subscription = UserSubscription::from_json(sub_data);
            session_.has_subscription = !session_.subscription.account_type.isEmpty();
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

        if (!session_.api_key.isEmpty())
            session_.authenticated = true;
        save_session();
        auto_configure_fincept_llm();
        set_loading(false);
        if (on_done)
            on_done();
        emit auth_state_changed();
    });
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

    AuthApi::instance().login(req, [this](ApiResponse r) {
        if (!r.success) {
            set_loading(false);
            emit login_failed(r.error.isEmpty() ? "Login failed" : r.error);
            return;
        }

        const auto data = unwrap_data(r.data);

        if (data["active_session"].toBool()) {
            set_loading(false);
            emit login_active_session(data["message"].toString("You are already logged in on another device."));
            return;
        }

        if (data["mfa_required"].toBool()) {
            set_loading(false);
            emit login_mfa_required();
            return;
        }

        const QString api_key = data["api_key"].toString();
        if (api_key.isEmpty()) {
            set_loading(false);
            emit login_failed("No API key returned from server");
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
        fetch_user_profile([this] { emit login_succeeded(); });
    });
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
            emit signup_failed(r.error.isEmpty() ? "Registration failed" : r.error);
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
            emit otp_failed(r.error.isEmpty() ? "Verification failed" : r.error);
            return;
        }

        const auto data = unwrap_data(r.data);
        const QString api_key = data["api_key"].toString();
        if (api_key.isEmpty()) {
            set_loading(false);
            emit otp_failed("No API key returned");
            return;
        }

        const QString session_token = data["session_token"].toString();
        apply_tokens(api_key, session_token);

        session_.authenticated = true;
        session_.api_key = api_key;
        session_.session_token = session_token;
        session_.device_id = generate_device_id();

        LOG_INFO("Auth", "OTP verified successfully");
        fetch_user_profile([this] { emit otp_verified(); });
    });
}

// ── MFA verification ─────────────────────────────────────────────────────────

void AuthManager::verify_mfa(const QString& email, const QString& otp) {
    set_loading(true);

    AuthApi::instance().verify_mfa(sanitize_input(email).toLower(), sanitize_input(otp), [this](ApiResponse r) {
        if (!r.success) {
            set_loading(false);
            emit mfa_failed(r.error.isEmpty() ? "MFA verification failed" : r.error);
            return;
        }

        const auto data = unwrap_data(r.data);
        const QString api_key = data["api_key"].toString();
        if (api_key.isEmpty()) {
            set_loading(false);
            emit mfa_failed("No API key returned");
            return;
        }

        const QString session_token = data["session_token"].toString();
        apply_tokens(api_key, session_token);

        session_.authenticated = true;
        session_.api_key = api_key;
        session_.session_token = session_token;
        session_.device_id = generate_device_id();

        LOG_INFO("Auth", "MFA verified successfully");
        fetch_user_profile([this] { emit mfa_verified(); });
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
            emit forgot_password_failed(r.error.isEmpty() ? "Failed to send reset code" : r.error);
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
            emit password_reset_failed(r.error.isEmpty() ? "Password reset failed" : r.error);
    });
}

// ── Logout ───────────────────────────────────────────────────────────────────

void AuthManager::logout() {
    if (is_logging_out_)
        return;
    is_logging_out_ = true;

    if (!session_.api_key.isEmpty()) {
        AuthApi::instance().logout([](ApiResponse) {});
    }

    clear_session();
    is_logging_out_ = false;

    emit logged_out();
    emit auth_state_changed();
}

// ── Session recovery ─────────────────────────────────────────────────────────
// Called by SessionGuard when it gets 401. Instead of immediately logging out,
// we strip the stale session_token and re-validate with api_key only.
// If the api_key is still valid → session recovered, no disruption.
// If api_key is also invalid → truly expired, must re-login.

void AuthManager::attempt_session_recovery(std::function<void(bool)> cb) {
    if (session_.api_key.isEmpty()) {
        if (cb)
            cb(false);
        return;
    }

    LOG_INFO("Auth", "Attempting session recovery with api_key only");

    // Temporarily strip session_token so the validation request doesn't 401
    fincept::HttpClient::instance().clear_session_token();

    AuthApi::instance().get_user_profile([this, cb = std::move(cb)](ApiResponse r) mutable {
        if (r.success) {
            LOG_INFO("Auth", "Session recovery succeeded — api_key is valid");
            const auto data = unwrap_data(r.data);
            session_.user_info = UserProfile::from_json(data);
            session_.authenticated = true;

            // The old session_token was stale. Clear it from our saved state.
            // The app continues with api_key-only auth. A fresh session_token
            // will be obtained on the next explicit login.
            session_.session_token.clear();
            save_session();

            // Don't restore session_token — operate with api_key only
            if (cb)
                cb(true);
        } else if (r.status_code == 401 || r.status_code == 403) {
            LOG_WARN("Auth", "Session recovery failed — api_key is invalid");
            if (cb)
                cb(false);
        } else {
            // Network error — don't force logout, assume transient
            LOG_WARN("Auth", "Session recovery: network error — keeping session alive");
            // Restore session_token since we can't determine if it's stale
            if (!session_.session_token.isEmpty())
                fincept::HttpClient::instance().set_session_token(session_.session_token);
            if (cb)
                cb(true);
        }
    });
}

// ── Refresh user data ────────────────────────────────────────────────────────

void AuthManager::refresh_user_data() {
    if (!session_.authenticated || session_.api_key.isEmpty())
        return;
    // fetch_user_profile chains into fetch_user_subscription automatically
    fetch_user_profile([this] { emit subscription_fetched(); });
}

// ── Auto-configure Fincept LLM provider ──────────────────────────────────────

void AuthManager::auto_configure_fincept_llm() {
    if (session_.api_key.isEmpty())
        return;

    // Always store API key in settings — LlmService resolves it at runtime
    fincept::SettingsRepository::instance().set("fincept_api_key", session_.api_key, "auth");

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
