#include "auth/SessionGuard.h"
#include "auth/AuthManager.h"
#include "auth/AuthApi.h"
#include "core/logging/Logger.h"

namespace fincept::auth {

SessionGuard::SessionGuard(QObject* parent) : QObject(parent) {
    timer_.setSingleShot(false);
    timer_.setInterval(PULSE_INTERVAL_MS);
    connect(&timer_, &QTimer::timeout, this, &SessionGuard::check_pulse);

    connect(&AuthManager::instance(), &AuthManager::auth_state_changed, this, [this]() {
        const auto& s = AuthManager::instance().session();
        if (s.authenticated && !s.session_token.isEmpty()) {
            start();
        } else {
            stop();
        }
    });
}

void SessionGuard::start() {
    if (!timer_.isActive()) {
        check_pulse();
        timer_.start();
    }
}

void SessionGuard::stop() {
    timer_.stop();
}

void SessionGuard::check_pulse() {
    const auto& s = AuthManager::instance().session();
    if (!s.authenticated || s.api_key.isEmpty()) return;
    if (is_checking_) return;

    is_checking_ = true;

    AuthApi::instance().session_pulse([this](ApiResponse r) {
        is_checking_ = false;

        // 401/403 → session definitively expired
        if (!r.success && (r.status_code == 401 || r.status_code == 403)) {
            LOG_WARN("SessionGuard", QString("Session expired (HTTP %1) — logging out").arg(r.status_code));
            stop();
            emit session_expired();
            AuthManager::instance().logout();
            return;
        }

        // Network/server error → don't log out, just skip this pulse
        if (!r.success) {
            LOG_DEBUG("SessionGuard", "Pulse network error — skipping");
            return;
        }

        // Explicit valid=false in response body
        auto data = r.data;
        if (data.contains("data") && data["data"].isObject())
            data = data["data"].toObject();
        if (data.contains("valid") && !data["valid"].toBool()) {
            LOG_WARN("SessionGuard", "Session invalid (valid=false) — logging out");
            stop();
            emit session_expired();
            AuthManager::instance().logout();
        }
    });
}

} // namespace fincept::auth
