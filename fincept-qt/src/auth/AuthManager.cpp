#include "auth/AuthManager.h"

#include "auth/AuthApi.h"
#include "auth/PhaseOneAuthFlowBridge.h"
#include "auth/PhaseOneAuthRecoveryBridge.h"
#include "auth/PhaseOneSessionAuthBridge.h"
#include "auth/PinManager.h"
#include "auth/UserApi.h"
#include "core/logging/Logger.h"
#include "multiuser/client/PhaseOneClientTransport.h"
#include "multiuser/client/PhaseOneSessionStateGuard.h"
#include "multiuser/contracts/PhaseOneAuthTypes.h"
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

AuthManager::AuthManager() {
    multiuser::PhaseOneSessionStateGuard::set_invalid_session_handler(
        [this]() { handle_phase_one_session_invalidation(); });
}

QString AuthManager::fincept_provider_api_key() const {
    auto stored = fincept::SettingsRepository::instance().get("fincept_api_key");
    auto owner = fincept::SettingsRepository::instance().get("fincept_api_key_owner");
    auto secure_key = fincept::SecureStorage::instance().retrieve("api_key");
    return PhaseOneSessionAuthBridge::resolve_fincept_provider_api_key(
        session_, stored.is_ok() ? stored.value() : QString{}, secure_key.is_ok() ? secure_key.value() : QString{},
        owner.is_ok() ? owner.value() : QString{});
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

void AuthManager::save_session() {
    if (session_.is_phase_one_session()) {
        PhaseOneAuthRecoveryBridge::persist_phase_one_session(session_);
        return;
    }

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
        fincept::SettingsRepository::instance().set("fincept_api_key_owner", session_.username, "auth");
    }
}

void AuthManager::load_session() {
    const auto saved_session = fincept::SettingsRepository::instance().get("fincept_session");
    const auto saved_api_key = fincept::SettingsRepository::instance().get("fincept_api_key");
    const auto saved_api_key_owner = fincept::SettingsRepository::instance().get("fincept_api_key_owner");
    const auto secure_api_key = fincept::SecureStorage::instance().retrieve("api_key");

    if ((!saved_api_key_owner.is_ok() || saved_api_key_owner.value().trimmed().isEmpty()) &&
        (saved_api_key.is_ok() || secure_api_key.is_ok()) && saved_session.is_ok() && !saved_session.value().trimmed().isEmpty()) {
        const QJsonDocument document = QJsonDocument::fromJson(saved_session.value().toUtf8());
        if (document.isObject()) {
            const SessionData restored = SessionData::from_json(document.object());
            if (restored.has_hosted_api_key() && !restored.username.trimmed().isEmpty())
                fincept::SettingsRepository::instance().set("fincept_api_key_owner", restored.username, "auth");
        }
    }

    if (!PhaseOneAuthRecoveryBridge::should_restore_startup_auth(
            saved_session.is_ok() ? saved_session.value() : QString{},
            saved_api_key.is_ok() ? saved_api_key.value() : QString{},
            secure_api_key.is_ok() ? secure_api_key.value() : QString{})) {
        PhaseOneAuthRecoveryBridge::clear_startup_auth_state();
        session_ = SessionData{};
        return;
    }

    session_ = SessionData{};
}

void AuthManager::clear_session(bool clear_provider_credentials) {
    session_ = SessionData{};
    multiuser::PhaseOneClientTransport::instance().set_session_id({});
    clear_tokens();
    fincept::SettingsRepository::instance().remove("fincept_session");
    if (clear_provider_credentials) {
        fincept::SettingsRepository::instance().remove("fincept_api_key");
        fincept::SettingsRepository::instance().remove("fincept_api_key_owner");
        fincept::SecureStorage::instance().remove("api_key");
    }

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
    return session_.authenticated && !PinManager::instance().has_pin();
}

// ── Initialize ───────────────────────────────────────────────────────────────

void AuthManager::initialize() {
    set_loading(true);
    load_session();

    clear_tokens();

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

void AuthManager::phase_one_login(const QString& username, const QString& password) {
    set_loading(true);

    PhaseOneAuthFlowBridge::submit_login(sanitize_input(username).toLower(), password, [this](ApiResponse r) {
        if (!r.success) {
            set_loading(false);
            emit login_failed(r.error.isEmpty() ? "Login failed" : r.error);
            return;
        }

        apply_phase_one_session(r.data);
        save_session();
        set_loading(false);
        emit login_succeeded();
        emit auth_state_changed();
    });
}

// ── Signup ───────────────────────────────────────────────────────────────────

void AuthManager::signup(const QString& username, const QString& email, const QString& password, const QString& phone,
                         const QString& country, const QString& country_code) {
    if (!PhaseOneAuthFlowBridge::supports_self_registration()) {
        emit signup_failed("Self-registration is unavailable in phase one.");
        return;
    }

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
    if (!PhaseOneAuthFlowBridge::supports_password_recovery()) {
        emit forgot_password_failed("Password recovery is unavailable in phase one.");
        return;
    }

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
    if (!PhaseOneAuthFlowBridge::supports_password_recovery()) {
        emit password_reset_failed("Password recovery is unavailable in phase one.");
        return;
    }

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

    if (session_.is_phase_one_session() || !session_.session_id.isEmpty()) {
        AuthApi::instance().phase_one_logout([this](ApiResponse response) {
            const bool server_already_invalid = !response.success &&
                (response.error_code == QStringLiteral("session_invalid") ||
                 response.error_code == QStringLiteral("session_revoked") ||
                 response.error_code == QStringLiteral("session_expired") ||
                 response.error_code == QStringLiteral("user_disabled"));

            if (!response.success && !server_already_invalid) {
                LOG_ERROR("Auth", "Phase one logout failed: " +
                                      (response.error.isEmpty() ? QStringLiteral("server session still active")
                                                                : response.error));
            }

            clear_session(false);
            emit logged_out();
            emit auth_state_changed();
            is_logging_out_ = false;
        });
        return;
    }

    if (!session_.api_key.isEmpty()) {
        AuthApi::instance().logout([](ApiResponse) {});
    }

    clear_session();
    is_logging_out_ = false;

    emit logged_out();
    emit auth_state_changed();
}

void AuthManager::phase_one_logout() {
    if (is_logging_out_)
        return;
    is_logging_out_ = true;

    if (!session_.session_id.isEmpty()) {
        AuthApi::instance().phase_one_logout([this](ApiResponse response) {
            const bool server_already_invalid = !response.success &&
                (response.error_code == QStringLiteral("session_invalid") ||
                 response.error_code == QStringLiteral("session_revoked") ||
                 response.error_code == QStringLiteral("session_expired") ||
                 response.error_code == QStringLiteral("user_disabled"));

            if (!response.success && !server_already_invalid) {
                LOG_ERROR("Auth", "Phase one logout failed: " +
                                      (response.error.isEmpty() ? QStringLiteral("server session still active")
                                                                : response.error));
            }

            clear_session(false);
            emit logged_out();
            emit auth_state_changed();
            is_logging_out_ = false;
        });
        return;
    }

    clear_session(false);
    is_logging_out_ = false;

    emit logged_out();
    emit auth_state_changed();
}

void AuthManager::phase_one_current_session(std::function<void(bool)> cb) {
    AuthApi::instance().phase_one_current_session([this, cb = std::move(cb)](ApiResponse r) mutable {
        if (!r.success) {
            if (cb)
                cb(false);
            return;
        }

        apply_phase_one_session(r.data);
        save_session();
        emit auth_state_changed();
        if (cb)
            cb(true);
    });
}

void AuthManager::handle_phase_one_session_invalidation() {
    if (!session_.is_phase_one_session() && session_.session_id.isEmpty())
        return;

    clear_session(false);
    emit session_expired();
    emit logged_out();
    emit auth_state_changed();
}

// ── Session recovery ─────────────────────────────────────────────────────────
// Called by SessionGuard when it gets 401. Instead of immediately logging out,
// we strip the stale session_token and re-validate with api_key only.
// If the api_key is still valid → session recovered, no disruption.
// If api_key is also invalid → truly expired, must re-login.

void AuthManager::attempt_session_recovery(std::function<void(bool)> cb) {
    if (session_.is_phase_one_session()) {
        if (cb)
            cb(false);
        return;
    }

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
    if (session_.is_phase_one_session())
        return;

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
    fincept::SettingsRepository::instance().set("fincept_api_key_owner", session_.username, "auth");

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

void AuthManager::apply_phase_one_session(const QJsonObject& raw) {
    const auto data = unwrap_data(raw);
    const auto info = multiuser::PhaseOneSessionInfo::from_json(data);

    session_.authenticated = true;
    session_.api_key.clear();
    session_.session_token = info.session_id;
    session_.user_id = info.user_id;
    session_.username = info.username;
    session_.role = info.role;
    session_.session_id = info.session_id;
    session_.expires_at = info.expires_at;
    session_.device_id = generate_device_id();
    session_.user_info.username = info.username;
    session_.user_info.is_admin = info.role.compare("admin", Qt::CaseInsensitive) == 0;

    multiuser::PhaseOneClientTransport::instance().set_session_id(info.session_id);
}

} // namespace fincept::auth
