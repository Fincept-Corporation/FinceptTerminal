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
    if (!s.authenticated || s.api_key.isEmpty() || s.session_token.isEmpty()) return;
    if (is_checking_) return;

    is_checking_ = true;

    // HttpClient already has tokens set — no need to pass api_key
    AuthApi::instance().session_pulse([this](ApiResponse r) {
        is_checking_ = false;

        if (!r.success && (r.status_code == 401 || r.status_code == 403)) {
            LOG_WARN("SessionGuard", "Session expired (401/403) — logging out");
            stop();
            AuthManager::instance().logout();
            return;
        }

        if (r.success) {
            auto data = r.data;
            if (data.contains("data") && data["data"].isObject()) {
                data = data["data"].toObject();
            }
            if (data.contains("valid") && !data["valid"].toBool()) {
                LOG_WARN("SessionGuard", "Session invalid (valid=false) — logging out");
                stop();
                AuthManager::instance().logout();
            }
        }
    });
}

} // namespace fincept::auth
