#include "auth/AuthManager.h"

#include "auth/AuthApi.h"
#include "auth/UserApi.h"
#include "core/config/AppConfig.h"
#include "core/logging/Logger.h"
#include "network/http/HttpClient.h"
#include "storage/repositories/LlmConfigRepository.h"
#include "storage/repositories/SettingsRepository.h"

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
}

void AuthManager::load_session() {
    auto r = fincept::SettingsRepository::instance().get("fincept_session");
    if (r.is_err())
        return;

    QString json = r.value();
    if (json.isEmpty())
        return;

    auto doc = QJsonDocument::fromJson(json.toUtf8());
    if (doc.isNull())
        return;

    session_ = SessionData::from_json(doc.object());
    // Never trust saved authenticated flag — must be re-validated
    session_.authenticated = false;
}

void AuthManager::clear_session() {
    session_ = SessionData{};
    clear_tokens();
    fincept::SettingsRepository::instance().remove("fincept_session");
    fincept::SettingsRepository::instance().remove("fincept_api_key");

    // Clear auto-configured fincept LLM provider and reset LlmService
    LlmConfigRepository::instance().delete_provider("fincept");
}

// ── Initialize ───────────────────────────────────────────────────────────────

void AuthManager::initialize() {
    set_loading(true);
    load_session();

    if (!session_.api_key.isEmpty()) {
        // Restore tokens on HttpClient so API calls work
        apply_tokens(session_.api_key, session_.session_token);
        validate_saved_session();
        return;
    }

    set_loading(false);
    emit auth_state_changed();
}

void AuthManager::validate_saved_session() {
    AuthApi::instance().validate_session_server([this](ApiResponse r) {
        // 401/403 → session truly expired, must re-login
        if (!r.success && (r.status_code == 401 || r.status_code == 403)) {
            LOG_WARN("Auth", "Session expired (HTTP " + QString::number(r.status_code) + ") — clearing");
            clear_session();
            set_loading(false);
            emit auth_state_changed();
            return;
        }

        // Network/server error (500, 0) → treat as transient, keep session alive
        // and proceed to dashboard; worst case the next API call will fail
        if (!r.success) {
            LOG_WARN("Auth", "Session validation network error — restoring session anyway");
            session_.authenticated = true;
            set_loading(false);
            emit auth_state_changed();
            return;
        }

        // Success — check response body
        const auto data = unwrap_data(r.data);
        LOG_DEBUG("Auth", "validate-session response: " +
                              QString::fromUtf8(QJsonDocument(r.data).toJson(QJsonDocument::Compact)));

        // Only clear if server explicitly says valid=false with a 200 OK.
        // Some servers return {"valid": false} only when the token is truly revoked.
        if (r.status_code == 200 && data.contains("valid") && !data["valid"].toBool()) {
            LOG_WARN("Auth", "Session invalid (valid=false in body) — clearing");
            clear_session();
            set_loading(false);
            emit auth_state_changed();
            return;
        }

        // Session valid — refresh profile then proceed
        fetch_user_profile([this] { emit subscription_fetched(); });
    });
}

void AuthManager::fetch_user_profile(std::function<void()> on_done) {
    AuthApi::instance().get_user_profile([this, on_done = std::move(on_done)](ApiResponse r) mutable {
        if (r.success) {
            const auto data = unwrap_data(r.data);
            session_.user_info = UserProfile::from_json(data);
        }
        fetch_user_subscription(std::move(on_done));
    });
}

// ── Shared post-auth completion ───────────────────────────────────────────────
// Fetches subscription, syncs state, saves session, emits success_signal.
// Called at the tail of login / verify_otp / verify_mfa / session restore.

// ── Shared post-auth completion ───────────────────────────────────────────────
// Fetches subscription, syncs state, saves session, then calls on_done().
// Used by login / verify_otp / verify_mfa / session restore to avoid
// duplicating the same 8-line block in four places.

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

    // Store API key in settings for fallback access
    fincept::SettingsRepository::instance().set("fincept_api_key", session_.api_key, "auth");

    // Save/update fincept as an LLM provider (api_key left empty — resolved from
    // SettingsRepository at runtime by LlmService::ensure_config())
    LlmConfig fincept_llm;
    fincept_llm.provider = "fincept";
    fincept_llm.model = "fincept-llm";
    fincept_llm.base_url = "https://api.fincept.in/research/llm";

    LlmConfigRepository::instance().save_provider(fincept_llm);

    // Set as active if no other provider is configured
    auto active = LlmConfigRepository::instance().get_active_provider();
    bool has_active = active.is_ok() && !active.value().provider.isEmpty();
    if (!has_active)
        LlmConfigRepository::instance().set_active("fincept");

    LOG_INFO("Auth", "Fincept LLM auto-configured with session API key");
}

} // namespace fincept::auth
