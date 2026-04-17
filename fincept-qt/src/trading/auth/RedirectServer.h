#pragma once
// Loopback HTTP server that catches a single OAuth-style redirect carrying
// ?request_token=...&status=success in the query string.
//
// Binds 127.0.0.1:preferred_port. If that port is busy, binds to an ephemeral
// port and exposes it via port().
//
// Owned by the dialog for the duration of one browser-login attempt.

#include <QObject>
#include <QTcpServer>
#include <QTimer>

namespace fincept::trading::auth {

class RedirectServer : public QObject {
    Q_OBJECT
  public:
    explicit RedirectServer(QObject* parent = nullptr);
    ~RedirectServer() override;

    // Starts listening. Returns true on success. Updates port() on ephemeral fallback.
    // timeout_seconds triggers timeout() if no request arrives in that window.
    bool start(quint16 preferred_port = 5010, int timeout_seconds = 120);
    void stop();

    quint16 port() const { return port_; }

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
};

} // namespace fincept::trading::auth
