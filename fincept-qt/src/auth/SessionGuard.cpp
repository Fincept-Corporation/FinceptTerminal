#include "auth/SessionGuard.h"

#include "auth/AuthApi.h"
#include "auth/AuthManager.h"
#include "auth/InactivityGuard.h"
#include "core/logging/Logger.h"
#include "network/http/HttpClient.h"

#include <QDateTime>
#include <QRandomGenerator>

#include <algorithm>

namespace fincept::auth {

SessionGuard::SessionGuard(QObject* parent) : QObject(parent) {
    timer_.setSingleShot(true); // re-armed with fresh jitter on every check
    connect(&timer_, &QTimer::timeout, this, &SessionGuard::check_pulse);

    // Background polls (this pulse, AuthManager's account refresh) skip an idle
    // terminal, which needs input tracked even with auto-lock off.
    InactivityGuard::instance().ensure_input_tracking();

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
    // No pulse at start: sign-in has just issued the token, and startup
    // validation checks a saved one before reusing it
    // (AuthManager::verify_saved_session_token). The old 15-second pulse here
    // repeated that check on every launch.
    arm();
}

void SessionGuard::stop() {
    timer_.stop();
}

qint64 SessionGuard::effective_interval_ms() const {
    return std::max<qint64>(interval_ms_, server_hint_ms_);
}

void SessionGuard::arm() {
    // Both terms are at most an hour (HttpClient caps the hint), so this fits an int.
    const int base = static_cast<int>(effective_interval_ms());
    const int jitter = base >= 10 ? static_cast<int>(QRandomGenerator::global()->bounded(base / 10)) : 0;
    timer_.start(base + jitter);
}

void SessionGuard::set_pulse_interval_ms(int base_ms) {
    interval_ms_ = std::max(1, base_ms);
    if (timer_.isActive())
        arm();
}

void SessionGuard::check_pulse() {
    const auto& s = AuthManager::instance().session();
    if (!s.authenticated || s.api_key.isEmpty())
        return; // stopped; the next sign-in re-arms through auth_state_changed
    arm(); // the next check, whatever this one decides
    if (is_checking_)
        return;
    // Locked, idle, minimised or hidden: skip. While locked a 401 here would
    // force a logout behind the lock UI (the PIN unlock would then land on the
    // login screen); while idle nobody is there, and the first request after
    // the user returns detects a replaced session by itself.
    if (!InactivityGuard::instance().user_active())
        return;
    // While the API refuses this IP every pulse is a 403 that says nothing about
    // the session; skip until the block lifts.
    if (fincept::HttpClient::instance().is_ip_blocked()) {
        LOG_DEBUG("SessionGuard", "Fincept API has blocked this IP — skipping pulse");
        return;
    }
    // A profile refresh carries the session token, so a recent one already
    // proved what this pulse would.
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 last_ok = std::max(last_pulse_ok_ms_, AuthManager::instance().user_data_fetched_ms());
    if (last_ok > 0 && now - last_ok < effective_interval_ms())
        return;

    is_checking_ = true;

    AuthApi::instance().session_pulse([this](ApiResponse r) {
        is_checking_ = false;

        // The server's pacing (X-Poll-Interval; success and error answers alike).
        // A slower hint than the cadence the timer was armed with re-arms it now.
        if (r.poll_interval_ms > 0 && r.poll_interval_ms != server_hint_ms_) {
            const qint64 before = effective_interval_ms();
            server_hint_ms_ = r.poll_interval_ms;
            if (effective_interval_ms() > before && timer_.isActive())
                arm();
        }

        // 401. A stale session token never gets here: HttpClient drops it on the
        // first "session_invalidated" 401 (from this pulse or any other request),
        // tells AuthManager, and re-sends this pulse with the key alone. So a 401
        // here means the API key itself was refused. AuthManager confirms that
        // with one key-only check (shared with any check already running) and,
        // if the key really is dead, signs out by itself.
        if (!r.success && r.status_code == 401) {
            LOG_WARN("SessionGuard", "Pulse returned HTTP 401 — re-validating the API key");
            is_checking_ = true;
            AuthManager::instance().attempt_session_recovery([this](bool recovered) {
                is_checking_ = false;
                if (recovered) {
                    LOG_INFO("SessionGuard", "Session kept — api_key still valid (or check inconclusive)");
                    return;
                }
                LOG_WARN("SessionGuard", "API key refused — AuthManager is signing out");
                stop();
                emit session_expired();
            });
            return;
        }

        // 403 (IP block or a proxy refusing us — never an account verdict),
        // network or server error → don't log out, just skip this pulse.
        if (!r.success) {
            LOG_DEBUG("SessionGuard", QString("Pulse failed (HTTP %1) — skipping").arg(r.status_code));
            return;
        }
        last_pulse_ok_ms_ = QDateTime::currentMSecsSinceEpoch();

        // Explicit valid=false in the body (the current API answers {"ok":true}
        // or 401, but honour it): the session token is no longer ours.
        auto data = r.data;
        if (data.contains("data") && data["data"].isObject())
            data = data["data"].toObject();
        if (data.contains("valid") && !data["valid"].toBool()) {
            // AuthManager::handle_unauthorized() writes the "Session token rejected" WARN.
            LOG_DEBUG("SessionGuard", "Pulse returned valid=false — dropping the session token");
            const QString token = AuthManager::instance().session().session_token;
            if (!token.isEmpty())
                AuthManager::instance().handle_unauthorized(token, true);
        }
    });
}

} // namespace fincept::auth
