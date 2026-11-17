#pragma once
#include <QObject>
#include <QTimer>

namespace fincept::auth {

/// Backstop check of the session (GET /user/session-pulse) for a terminal that
/// is in use but makes no other Fincept request. Stale session tokens are
/// handled on the first 401 of any request (HttpClient → AuthManager::
/// handle_unauthorized), so this only has to catch the case where nothing else
/// would notice: at most one pulse per kPulseIntervalMs (jittered) — or per the
/// server's X-Poll-Interval when that is longer (capped at an hour) — and none
/// while signed out, locked, idle, minimised/hidden or IP-blocked, nor when a
/// profile refresh (which carries the session token) already proved the
/// session within the interval.
/// On 401 → AuthManager re-validates the key and signs out only if it is
/// refused. 403 (IP block) and network errors never sign out.
class SessionGuard : public QObject {
    Q_OBJECT
  public:
    explicit SessionGuard(QObject* parent = nullptr);

    void start();
    void stop();

    /// Pulse cadence: base + random [0, base / 10). For tests.
    void set_pulse_interval_ms(int base_ms);

  signals:
    void session_expired();

  private slots:
    void check_pulse();

  private:
    void arm();
    /// max(own cadence, the server's X-Poll-Interval) — the server's hint can
    /// slow the pulse down (bounded to an hour) but never speed it up.
    qint64 effective_interval_ms() const;

    QTimer timer_;
    bool is_checking_ = false;
    int interval_ms_ = kPulseIntervalMs;
    qint64 last_pulse_ok_ms_ = 0;
    /// X-Poll-Interval of the last pulse answer (HttpClient::poll_interval_ms,
    /// so at most an hour); 0 = none (older server).
    qint64 server_hint_ms_ = 0;

    // 60 minutes. Was 30 s, then 5 min, then 30 min: the pulse alone was ~750k
    // requests in five weeks. The first rejected request now detects a replaced
    // session, so the pulse only covers a terminal in use that talks to no
    // Fincept endpoint — and the background profile refresh (every 30–40 min
    // while in use) already proves the session, so in practice it rarely fires.
    // Inactivity / lock are handled by InactivityGuard, not here.
    static constexpr int kPulseIntervalMs = 60 * 60 * 1000;
};

} // namespace fincept::auth
