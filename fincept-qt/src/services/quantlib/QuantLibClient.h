#pragma once
// QuantLibClient.h — Shared HTTP client for all QuantLib API calls.
// Used by QuantLibScreen (async) and MCP QuantLibTools (sync-wrapped).

#include "mcp/McpTypes.h"

#include <QJsonObject>
#include <QObject>

#include <functional>

namespace fincept::services {

/// Callback type: delivers unwrapped data payload or error string.
/// On success: result.success == true, result.data has the payload.
/// On failure: result.success == false, result.error has the message.
using QuantLibCallback = std::function<void(mcp::ToolResult)>;

class QuantLibClient : public QObject {
    Q_OBJECT
  public:
    static QuantLibClient& instance();

    /// Async call — callback posted on Qt event loop (use from UI thread).
    void call(const QString& endpoint, const QJsonObject& body, QuantLibCallback callback);

    /// Sync call — blocks via QEventLoop (use from MCP tool handlers only, never from UI thread).
    mcp::ToolResult call_sync(const QString& endpoint, const QJsonObject& body);

    static const QString API_BASE;

    /// GET-only endpoints (no request body).
    static bool is_get_endpoint(const QString& endpoint);

    QuantLibClient(const QuantLibClient&) = delete;
    QuantLibClient& operator=(const QuantLibClient&) = delete;

  private:
    explicit QuantLibClient(QObject* parent = nullptr);

    /// Parses raw HTTP response bytes into a ToolResult, unwrapping the
    /// {"success", "message", "data"} envelope and handling 422/4xx errors.
    static mcp::ToolResult parse_response(int http_status, const QByteArray& raw);
};

} // namespace fincept::services
