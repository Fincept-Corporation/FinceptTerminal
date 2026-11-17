#pragma once
// Loopback HTTP server that catches a single OAuth-style redirect carrying
// ?request_token=...&status=success in the query string.
//
// Binds 127.0.0.1:preferred_port. If that port is busy, binds to an ephemeral
// port and exposes it via port() — unless the caller asks for the fixed port
// only (a broker redirect registered as http://127.0.0.1:<port>/ can never reach
// another port): then start() fails at once, without arming the timeout, and
// bind_error() / bind_error_text() say why.
//
// Owned by the dialog for the duration of one browser-login attempt.
//
// SECURITY — this listener accepts a credential from the browser on a fixed,
// guessable port. Three gates protect it (see auth/LoopbackGuard.h for the
// threat model):
//   1. Loopback bind — no other host can reach it.
//   2. Host + Sec-Fetch-* validation — rejects DNS-rebinding and any
//      cross-origin subresource injection (`<img src="http://127.0.0.1:5010/
//      ?request_token=...">` from any page the user has open).
//   3. A `state` nonce — the only gate that also stops a cross-site *top-level*
//      navigation, which is indistinguishable from the real redirect at the
//      header level. The caller must put state() into the provider's authorize
//      URL and then call set_require_state(true); until it does, a state-less
//      callback is accepted with a loud warning.

#include <QObject>
#include <QTcpServer>
#include <QTimer>

namespace fincept::trading::auth {

class RedirectServer : public QObject {
    Q_OBJECT
  public:
    /// Why the last start() could not bind its port.
    enum class BindError { None, PortInUse, PortBlocked, Other };

    explicit RedirectServer(QObject* parent = nullptr);
    ~RedirectServer() override;

    // Starts listening. Returns true on success. Updates port() on ephemeral fallback.
    // timeout_seconds triggers timeout() if no request arrives in that window.
    // fixed_port_only: never fall back to an ephemeral port; a busy preferred
    // port fails start() immediately (no listener, no timer).
    // Also mints a fresh state() nonce for this attempt.
    bool start(quint16 preferred_port = 5010, int timeout_seconds = 120, bool fixed_port_only = false);
    void stop();

    quint16 port() const { return port_; }

    /// BindError::None after a successful start().
    BindError bind_error() const { return bind_error_; }
    /// Plain, translated sentence for the last bind failure, naming the port and
    /// the likely cause; empty after a successful start().
    QString bind_error_text() const;

    /// Per-attempt CSRF nonce. Append it to the provider's authorize URL as
    /// `state=<value>`; OAuth providers echo it back on the redirect.
    /// Valid only after start().
    QString state() const { return state_; }

    /// When true, a callback without a matching `state` is rejected outright.
    /// Turn this on once the authorize URL actually carries state().
    void set_require_state(bool require) { require_state_ = require; }

  signals:
    void request_token_received(const QString& token);
    void error(const QString& message);
    void timeout();

  private slots:
    void handle_new_connection();
    void handle_timeout();

  private:
    QTcpServer* server_ = nullptr;
    QTimer* timeout_timer_ = nullptr;
    quint16 port_ = 0;
    quint16 failed_port_ = 0;
    BindError bind_error_ = BindError::None;
    QString state_;
    bool require_state_ = false;
};

} // namespace fincept::trading::auth
