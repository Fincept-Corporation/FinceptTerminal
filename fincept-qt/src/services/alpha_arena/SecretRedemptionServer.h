#pragma once
// SecretRedemptionServer — local IPC for the llm_call.py subprocess to fetch
// API keys without ever seeing them on argv or in the request file on disk.
//
// Model:
//   1. ModelDispatcher generates a per-tick `pipe_name` (uuid-based).
//   2. SecretRedemptionServer.start(pipe_name) opens a QLocalServer on that
//      name. Stays open for the duration of the tick (~minute), then closes.
//   3. Subprocess connects, sends {"handle":"<agent_secret_handle>"},
//      receives {"api_key":"<plaintext>"}, then disconnects.
//   4. Server resolves handles by looking them up in SecureStorage. The
//      handle IS the SecureStorage key; we don't keep an in-memory plaintext
//      cache.
//
// A handle is single-use per server lifetime — once redeemed it is added to a
// blacklist and subsequent reads on the same socket are rejected with
// {"error":"handle_already_redeemed"}. This means a malicious second
// connection in the same tick window can't replay.
//
// Reference: fincept-qt/.grill-me/alpha-arena-grill.md §9.1 (API key storage).

#include <QLocalServer>
#include <QLocalSocket>
#include <QObject>
#include <QSet>
#include <QString>

namespace fincept::services::alpha_arena {

class SecretRedemptionServer : public QObject {
    Q_OBJECT
  public:
    explicit SecretRedemptionServer(QObject* parent = nullptr);
    ~SecretRedemptionServer() override;

    /// Start a fresh server with the given pipe name. Closes any previous
    /// server first. Returns true on success.
    bool start(const QString& pipe_name);

    /// Close the server (disconnects all sockets).
    void stop();

    /// Currently running pipe name (empty if not running).
    QString pipe_name() const { return pipe_name_; }

  private slots:
    void on_new_connection();
    void on_socket_ready_read();

  private:
    QLocalServer* server_;
    QString pipe_name_;
    QSet<QString> redeemed_;  // handles already used during this server lifetime
};

} // namespace fincept::services::alpha_arena
