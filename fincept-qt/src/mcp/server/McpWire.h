#pragma once
// McpWire.h — the byte-level pieces of the terminal's MCP server, QtCore only:
// HTTP/1.1 request framing, response / SSE / chunk builders, JSON-RPC envelopes
// and protocol-version negotiation. No sockets, no threads, no application
// state, so every externally reachable parser here is unit-tested on its own
// (tests/tst_mcp_server_engine.cpp).
//
// Every limit a client controls is bounded here: header block, body, chunk-size
// line, trailer block. Nothing throws; a parse failure is reported as an HTTP
// status the caller answers with before closing the connection.

#include <QByteArray>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>

namespace fincept::mcp::wire {

// ── Limits ───────────────────────────────────────────────────────────────────
inline constexpr qsizetype kMaxHeaderBytes = 64 * 1024;
inline constexpr qint64 kMaxBodyBytes = 8 * 1024 * 1024;
inline constexpr qsizetype kMaxChunkLine = 1024;

// ── HTTP request framing ─────────────────────────────────────────────────────

/// One request as read from a connection. Header names are lower-case; a
/// repeated header is joined with ", " (Content-Length must not disagree).
struct HttpRequest {
    QByteArray head; ///< request line + header block incl. the blank line (for LoopbackGuard)
    QByteArray method;
    QByteArray target;
    QByteArray version;
    QHash<QByteArray, QByteArray> headers;
    qint64 content_length = 0;
    bool chunked = false;
    bool keep_alive = true;
    bool expect_continue = false;
    QByteArray body;

    QByteArray header(const char* lower_name) const { return headers.value(QByteArray(lower_name)); }
};

/// Incremental parser for one request at a time on one connection. Feed it the
/// connection's receive buffer; it consumes exactly the bytes of the request it
/// completes and leaves pipelined bytes behind.
class HttpRequestParser {
  public:
    enum class State { NeedMore, HeadReady, Complete, Error };

    /// Consume from `buf`. Returns HeadReady once (after the header block —
    /// before any body byte is buffered, so the caller can refuse early), then
    /// NeedMore / Complete / Error. After Complete or Error call reset().
    State feed(QByteArray& buf);

    void reset();
    const HttpRequest& request() const { return req_; }
    HttpRequest& request() { return req_; }
    int error_status() const { return error_status_; }
    QString error_text() const { return error_text_; }
    bool head_done() const { return head_done_; }
    bool has_partial_input() const { return head_done_ || !partial_.isEmpty(); }

  private:
    State fail(int status, const QString& why);
    bool parse_head();
    /// 1 = body complete, 0 = need more, -1 = error.
    int decode_chunked(QByteArray& buf);

    HttpRequest req_;
    bool head_done_ = false;
    bool head_reported_ = false;
    int chunk_state_ = 0; // 0 size line · 1 data · 2 CRLF after data · 3 trailers
    qint64 chunk_left_ = 0;
    qsizetype trailer_bytes_ = 0;
    QByteArray partial_; // header bytes seen so far (only to answer has_partial_input)
    int error_status_ = 400;
    QString error_text_;
};

/// Path component of a request target: absolute-form authority, query and
/// fragment removed, one trailing '/' stripped.
QByteArray request_path(const QByteArray& target);

const char* reason_phrase(int status);

/// True when an Accept header admits `mime` ("type/sub"), "type/*" or "*/*".
/// An absent header admits everything (RFC 9110 §12.5.1).
bool accepts(const QByteArray& accept_header, const char* mime);

// ── Responses ────────────────────────────────────────────────────────────────

using HeaderList = QList<QPair<QByteArray, QByteArray>>;

/// A complete response with Content-Length. `content_type` empty = none.
QByteArray build_response(int status, const QByteArray& content_type, const QByteArray& body,
                          const HeaderList& extra_headers, bool close_after, int keep_alive_secs, bool head_only);

/// Response head that opens a chunked text/event-stream body.
QByteArray build_sse_head(const HeaderList& extra_headers, bool close_after, int keep_alive_secs);

/// One HTTP chunk carrying `data` (empty data is not allowed — use kLastChunk).
QByteArray http_chunk(const QByteArray& data);
inline constexpr const char kLastChunk[] = "0\r\n\r\n";

/// One SSE `message` event carrying a JSON-RPC message (no newlines in compact JSON).
QByteArray sse_message(const QByteArray& compact_json);
/// An SSE comment line — ignored by clients, keeps idle timers from firing.
QByteArray sse_comment(const QByteArray& text);

// ── JSON ─────────────────────────────────────────────────────────────────────

QByteArray compact_json(const QJsonObject& object);
QByteArray compact_json(const QJsonArray& array);
/// Any JSON value as compact text (a scalar is wrapped and unwrapped).
QByteArray compact_json_value(const QJsonValue& value);

// ── JSON-RPC 2.0 ─────────────────────────────────────────────────────────────

inline constexpr int kParseError = -32700;
inline constexpr int kInvalidRequest = -32600;
inline constexpr int kMethodNotFound = -32601;
inline constexpr int kInvalidParams = -32602;
inline constexpr int kInternalError = -32603;
inline constexpr int kServerError = -32000;     ///< implementation-defined: server stopping / busy
inline constexpr int kRunTokenExpired = -32001; ///< the run-scoped bearer token died after the request's auth
inline constexpr int kResourceNotFound = -32002;

QJsonObject rpc_result(const QJsonValue& id, const QJsonValue& result);
QJsonObject rpc_error_body(int code, const QString& message);
QJsonObject rpc_error(const QJsonValue& id, int code, const QString& message);

/// A request id as MCP allows it: a string or a number (never null, boolean,
/// object or array). Ids are echoed back exactly as received.
bool valid_request_id(const QJsonValue& id);
/// Stable map key for an id: "s:<text>" / "n:<number>" (1 and 1.0 are the same id).
QString request_id_key(const QJsonValue& id);

/// MCP CallToolResult carrying only an error text (isError = true).
QJsonObject error_call_result(const QString& message);
QJsonObject text_block(const QString& text);

// ── Protocol versions ────────────────────────────────────────────────────────

/// Newest first. The server answers `initialize` with the client's version when
/// it is in this list, else with the newest (the client then decides).
const QStringList& supported_protocol_versions();
QString negotiate_protocol_version(const QString& requested);
inline constexpr const char kLatestProtocol[] = "2025-11-25";

/// The protocol era a request asks for, from its MCP-Protocol-Version header
/// and (MCP 2026-07-28) its params._meta["io.modelcontextprotocol/protocolVersion"].
/// Returns the unsupported version a request names, or an empty string when it
/// names none or one this server speaks. MCP 2026-07-28 dropped `initialize`;
/// a dual-era client probes with a modern request first and falls back to
/// `initialize` when the server answers 400 without a modern error body —
/// which is what this server does for such a request (2025-11-25 already
/// requires 400 for an unsupported MCP-Protocol-Version).
QString unsupported_protocol_requested(const QByteArray& header_value, const QJsonObject& params);

/// Session ids: visible ASCII (0x21–0x7E) only, at most 128 characters.
bool valid_session_id(const QByteArray& id);

} // namespace fincept::mcp::wire
