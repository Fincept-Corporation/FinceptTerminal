#pragma once

#include <QObject>
#include <QString>

class QTcpServer;
class QTcpSocket;
class QTimer;

namespace fincept::auth {

// Drives the server-side Google sign-in for the desktop app:
//   1. bind a loopback listener on 127.0.0.1:<ephemeral port>
//   2. open the browser to api.fincept.in/user/auth/google/start?desktop_cb=...
//      (the API redirects to Google and does the token exchange server-side)
//   5. catch the redirect GET /?code=<handoff_code>, reply with a small page
//
// Emits exactly one of code_received / failed, then stops listening and
// schedules its own deletion. The caller (AuthManager) redeems the code.
class GoogleDesktopLogin : public QObject {
    Q_OBJECT
  public:
    explicit GoogleDesktopLogin(QObject* parent = nullptr);
    ~GoogleDesktopLogin() override;

    void start();

  signals:
    void code_received(const QString& code);
    void failed(const QString& message);

  private:
    void on_new_connection();
    void finish_success(const QString& code, QTcpSocket* sock);
    void finish_error(const QString& message);
    static void send_page(QTcpSocket* sock, const QString& title, const QString& body);
    void cleanup();

    QTcpServer* server_ = nullptr;
    QTimer* timeout_ = nullptr;
    bool done_ = false;

    // ~5 min: a new Google user is detoured through the website /complete-profile
    // form before the loopback code is issued, so the wait can be well over 2 min.
    static constexpr int kTimeoutMs = 300000;
};

} // namespace fincept::auth
