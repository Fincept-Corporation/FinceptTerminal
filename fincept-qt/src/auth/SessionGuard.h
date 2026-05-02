#pragma once
#include <QObject>
#include <QTimer>

namespace fincept::auth {

/// Polls /user/validate-session every 30s.
/// On 401/403 or valid=false → triggers auto-logout.
class SessionGuard : public QObject {
    Q_OBJECT
  public:
    explicit SessionGuard(QObject* parent = nullptr);

    void start();
    void stop();

  signals:
    void session_expired();

  private slots:
    void check_pulse();

  private:
    QTimer timer_;
    bool is_checking_ = false;

    // 30 minutes — long enough that a transient backend hiccup or a stale
    // session_token doesn't yank a working user back to the login screen
    // mid-session, short enough to catch a truly revoked api_key within the
    // hour. Inactivity / lock are handled by InactivityGuard, not here.
    static constexpr int PULSE_INTERVAL_MS = 30 * 60 * 1000;
};

} // namespace fincept::auth
