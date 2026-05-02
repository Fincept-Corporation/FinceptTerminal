#include "auth/SessionGuard.h"

#include "auth/AuthApi.h"
#include "auth/AuthManager.h"
#include "auth/InactivityGuard.h"
#include "core/logging/Logger.h"

namespace fincept::auth {

SessionGuard::SessionGuard(QObject* parent) : QObject(parent) {
    timer_.setSingleShot(false);
    timer_.setInterval(PULSE_INTERVAL_MS);
    connect(&timer_, &QTimer::timeout, this, &SessionGuard::check_pulse);

    connect(&AuthManager::instance(), &AuthManager::auth_state_changed, this, [this]() {
        const auto& s = AuthManager::instance().session();
        if (s.authenticated && !s.api_key.isEmpty()) {
            start();
        } else {
            stop();
        }
    });
}

void SessionGuard::start() {
    if (timer_.isActive())
        return;
    // Do NOT pulse synchronously here. On startup the PIN gate routes the
    // user to the lock screen via auth_state_changed; firing a pulse on
    // the same signal raced ahead and could log the user out (via stale
    // session_token + recovery failure) before they ever saw the PIN
    // screen. Defer the first pulse by 15s so the PIN gate has time to
    // settle and InactivityGuard::is_terminal_locked() answers truthfully
    // when check_pulse() consults it.
    QTimer::singleShot(15000, this, &SessionGuard::check_pulse);
    timer_.start();
}

void SessionGuard::stop() {
    timer_.stop();
}

void SessionGuard::check_pulse() {
    const auto& s = AuthManager::instance().session();
    if (!s.authenticated || s.api_key.isEmpty())
        return;
    if (is_checking_)
        return;
    // Skip while the terminal is on the lock screen. A 401 here would force
    // a logout-to-login-screen behind the lock UI, so when the user enters
    // their PIN the unlock would land on the login screen instead of the
    // dashboard. Validation will resume on the next tick after unlock.
    if (InactivityGuard::instance().is_terminal_locked())
        return;

    is_checking_ = true;

    AuthApi::instance().session_pulse([this](ApiResponse r) {
        is_checking_ = false;

        // 401/403 → session_token is stale. But the api_key may still be valid.
        // Attempt recovery before logging the user out.
        if (!r.success && (r.status_code == 401 || r.status_code == 403)) {
            LOG_WARN("SessionGuard",
                     QString("Pulse returned HTTP %1 — attempting session recovery").arg(r.status_code));
            is_checking_ = true;
            AuthManager::instance().attempt_session_recovery([this](bool recovered) {
                is_checking_ = false;
                if (recovered) {
                    LOG_INFO("SessionGuard", "Session recovered — api_key still valid");
                    return;
                }
                // api_key is truly invalid — must re-login
                LOG_WARN("SessionGuard", "Session recovery failed — api_key invalid, logging out");
                stop();
                emit session_expired();
                AuthManager::instance().logout();
            });
            return;
        }

        // Network/server error → don't log out, just skip this pulse
        if (!r.success) {
            LOG_DEBUG("SessionGuard", "Pulse network error — skipping");
            return;
        }

        // Explicit valid=false in response body — same recovery path
        auto data = r.data;
        if (data.contains("data") && data["data"].isObject())
            data = data["data"].toObject();
        if (data.contains("valid") && !data["valid"].toBool()) {
            LOG_WARN("SessionGuard", "Pulse returned valid=false — attempting session recovery");
            is_checking_ = true;
            AuthManager::instance().attempt_session_recovery([this](bool recovered) {
                is_checking_ = false;
                if (recovered) {
                    LOG_INFO("SessionGuard", "Session recovered after valid=false");
                    return;
                }
                LOG_WARN("SessionGuard", "Session recovery failed — logging out");
                stop();
                emit session_expired();
                AuthManager::instance().logout();
            });
        }
    });
}

} // namespace fincept::auth
