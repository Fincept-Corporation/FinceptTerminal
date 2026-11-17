#pragma once
// QuantLibClient.h — HTTP client for the api.fincept.in/quantlib/* routes.
// Used by the QuantLib screen (src/screens/quantlib).

#include "mcp/McpTypes.h"

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QPointer>

#include <functional>

class QNetworkAccessManager;
class QNetworkReply;

namespace fincept::services {

/// Callback type: delivers unwrapped data payload or error string.
/// On success: result.success == true, result.data has the payload.
/// On failure: result.success == false, result.error has a plain-language
/// message and result.data is an object describing the failure:
///   http_status   (int, 0 = no HTTP answer)
///   error_code    (string, the server's error_code: HTTP_ERROR, VALIDATION_ERROR, IP_BLOCKED, ...)
///   network_error (int, QNetworkReply::NetworkError when there was no HTTP answer)
///   timed_out     (bool, no answer within the transfer timeout)
///   cancelled     (bool, the call was aborted with cancel())
using QuantLibCallback = std::function<void(mcp::ToolResult)>;

class QuantLibClient : public QObject {
    Q_OBJECT
  public:
    static QuantLibClient& instance();

    /// Async call. The callback runs exactly once, on the client's (GUI) thread.
    /// Returns an id for cancel(), or 0 when the answer came from the reference-
    /// data cache (the callback has already run) or the call was made from
    /// another thread (it is re-posted to the client's thread first).
    quint64 call(const QString& endpoint, const QJsonObject& body, QuantLibCallback callback);

    /// Aborts an in-flight call: the transfer stops and its callback runs once
    /// with result.data.cancelled == true. Unknown or finished ids are ignored.
    void cancel(quint64 request_id);

    // call_sync() was removed: it had zero call sites and blocked on an unbounded
    // QEventLoop, so any future caller would have inherited a hang with no timeout.
    // Use call() and continue from the callback.

    /// GET-only endpoints (no request body).
    static bool is_get_endpoint(const QString& endpoint);

    /// Query-param endpoints: body fields become URL query params, POST with empty body.
    static bool is_query_param_endpoint(const QString& endpoint);

    /// Seconds the client waits for an answer before giving up (shown in the timeout message).
    static int transfer_timeout_seconds();

    QuantLibClient(const QuantLibClient&) = delete;
    QuantLibClient& operator=(const QuantLibClient&) = delete;

  private:
    explicit QuantLibClient(QObject* parent = nullptr);

    /// One long-lived QNAM for every call (created on first use, owned by this
    /// object, lives on its thread): keep-alive TCP/TLS connections to the API
    /// host are reused instead of a fresh handshake per QuantLib request.
    QNetworkAccessManager* network();

    /// Common completion path of every network call: failure mapping, credit
    /// balance, parsing (off the GUI thread for large bodies), caching.
    void on_finished(quint64 id, QNetworkReply* reply, const QString& endpoint, const QString& cache_key,
                     const QuantLibCallback& callback);

    /// Parses raw HTTP response bytes into a ToolResult, unwrapping the
    /// {"success", "message", "data"} envelope. Pure: safe on a worker thread.
    static mcp::ToolResult parse_response(int http_status, const QByteArray& raw);

    QNetworkAccessManager* nam_ = nullptr;
    QHash<quint64, QPointer<QNetworkReply>> in_flight_;
    quint64 next_id_ = 0;
};

} // namespace fincept::services
