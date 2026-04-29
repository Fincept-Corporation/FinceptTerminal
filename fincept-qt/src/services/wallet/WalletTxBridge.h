#pragma once

#include "core/result/Result.h"

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QString>

class QTcpServer;
class QTcpSocket;

namespace fincept::wallet {

/// Loopback HTTP bridge for **transaction signing**, distinct from the
/// connect-handshake bridge. Phase 2 introduces this so that the connect
/// flow's threat model (sign-message handshake, single-use connect token,
/// 30 s lifetime) remains untouched by the new sign-tx capability.
///
/// Lifecycle of a single sign request:
///   1. C++ caller invokes `request_signature(tx_b64)` and receives a
///      `request_id`. The bridge mints a single-use UUID, records the
///      pending tx, and starts listening on a random ephemeral loopback
///      port if it isn't already.
///   2. The caller opens `swap.html` in the wallet dialog with
///      `?req=<request_id>&token=<token>`.
///   3. The page fetches `GET  /tx?req=<id>&token=<token>` to retrieve the
///      base64 transaction. The page never receives any other tx.
///   4. The page signs via the wallet adapter and POSTs
///      `{ signature: "<b58>" }` (or `{ error: "..." }`) to
///      `/result?req=<id>&token=<token>`.
///   5. Bridge resolves `request_signature`'s callback with the result and
///      retires the request_id (single-use). UUIDs older than 60 s are
///      auto-purged on every new request.
///
/// Security properties:
///   - listens only on `QHostAddress::LocalHost`.
///   - random port; random 32-byte token in URL query.
///   - per-request UUID; one request_id services exactly one signature
///     attempt; expired ids return 410 Gone, never reuse a previously-seen
///     transaction body.
///   - the page is loaded from `qrc://`, never `https://`. The bridge only
///     ever serves pre-built embedded HTML and the requested tx body.
///   - server self-stops when no in-flight requests remain.
class WalletTxBridge : public QObject {
    Q_OBJECT
  public:
    using SignatureCallback = std::function<void(Result<QString>)>;

    explicit WalletTxBridge(QObject* parent = nullptr);
    ~WalletTxBridge() override;

    /// Register a tx for signing. Returns `Result<QString>` containing the
    /// browser URL to open in the wallet dialog. Calls `cb` later with the
    /// base58 signature on success, or an error string. UUIDs expire after
    /// 60 s if no result is delivered.
    Result<QString> request_signature(const QString& tx_base64,
                                      SignatureCallback cb,
                                      int timeout_seconds = 60);

    /// Cancel a pending request. Idempotent — the cb fires with err
    /// "user_cancelled" if it hadn't already resolved.
    void cancel(const QString& request_id);

    /// True if the loopback server is currently listening for any request.
    bool is_listening() const noexcept;

  signals:
    void bridge_error(QString message);

  private:
    struct PendingRequest {
        QString tx_base64;
        SignatureCallback cb;
        qint64 created_at_ms = 0;
        int timeout_seconds = 60;
    };

    void on_new_connection();
    void handle_request(QTcpSocket* socket, const QByteArray& body,
                        const QByteArray& path, const QByteArray& method);
    QByteArray render_swap_page() const;
    void write_response(QTcpSocket* socket, int status,
                        const QByteArray& content_type, const QByteArray& body);
    bool ensure_server_started();
    void stop_server();
    void purge_expired_requests();
    void resolve_request(const QString& request_id, Result<QString> result);

    QTcpServer* server_ = nullptr;
    QByteArray session_token_;
    QHash<QString, PendingRequest> requests_;
    int sweep_timer_id_ = 0;

  protected:
    void timerEvent(QTimerEvent* e) override;
};

} // namespace fincept::wallet
