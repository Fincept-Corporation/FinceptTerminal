// McpHttpServer_Http.cpp — HTTP/1.1 framing for the external MCP server.
//
// Hand-rolled on QTcpServer (Qt6::HttpServer is not part of this Qt build — the
// same reason TerminalMcpBridge frames by hand). Unlike that bridge, this
// listener serves third-party HTTP stacks (Node undici, reqwest, urllib), so it
// supports keep-alive and pipelining, chunked request bodies and
// `Expect: 100-continue`, and bounds everything a client controls (64 KB header
// block, 8 MB body, 64 connections, 60 s idle).
//
// Order of checks for every request, before any body is buffered:
//   1. auth/LoopbackGuard.h — Host must be our loopback authority (DNS
//      rebinding), no cross-site fetch metadata, no foreign Origin.
//   2. /mcp requires `Authorization: Bearer <token>` (constant-time compare);
//      401 + WWW-Authenticate otherwise.
//   3. framing limits (413 / 411 / 431 / 501 / 505).
// /health answers without a token and carries nothing sensitive.

#include "auth/ConstantTime.h"
#include "auth/LoopbackGuard.h"
#include "core/logging/Logger.h"
#include "mcp/server/McpHttpServer_p.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QTcpServer>
#include <QTcpSocket>

#include <algorithm>

namespace fincept::mcp {

namespace mcp_http_detail {

namespace {

QList<QByteArray> header_tokens(const QByteArray& value) {
    QList<QByteArray> out;
    const QList<QByteArray> parts = value.split(',');
    for (const QByteArray& part : parts) {
        const QByteArray token = part.trimmed().toLower();
        if (!token.isEmpty())
            out.append(token);
    }
    return out;
}

bool fail_head(int* status, QString* why, int code, const QString& message) {
    *status = code;
    *why = message;
    return false;
}

QByteArray health_body() {
    return compact_json(QJsonObject{
        {QStringLiteral("ok"), true},
        {QStringLiteral("name"), QStringLiteral("fincept-terminal")},
        {QStringLiteral("version"), QCoreApplication::applicationVersion()},
    });
}

} // namespace

void reset_request(Conn& c) {
    c.head_done = false;
    c.head.clear();
    c.method.clear();
    c.target.clear();
    c.version.clear();
    c.headers.clear();
    c.content_length = 0;
    c.chunked = false;
    c.chunk_state = 0;
    c.chunk_left = 0;
    c.body.clear();
    c.expect_continue = false;
}

bool parse_request_head(Conn& c, int* status, QString* why) {
    const QList<QByteArray> lines = c.head.split('\n');
    QByteArray request_line = lines.value(0);
    if (request_line.endsWith('\r'))
        request_line.chop(1);
    const QList<QByteArray> parts = request_line.split(' ');
    if (parts.size() != 3 || parts[0].isEmpty() || parts[1].isEmpty())
        return fail_head(status, why, 400, QStringLiteral("Malformed request line"));
    for (const char ch : parts[0]) {
        if (ch < 'A' || ch > 'Z')
            return fail_head(status, why, 400, QStringLiteral("Malformed request method"));
    }
    c.method = parts[0];
    c.target = parts[1];
    c.version = parts[2];
    if (c.version != "HTTP/1.1" && c.version != "HTTP/1.0")
        return fail_head(status, why, 505, QStringLiteral("HTTP version not supported"));

    c.headers.clear();
    for (qsizetype i = 1; i < lines.size(); ++i) {
        QByteArray line = lines.at(i);
        if (line.endsWith('\r'))
            line.chop(1);
        if (line.isEmpty())
            continue;
        if (line.startsWith(' ') || line.startsWith('\t'))
            return fail_head(status, why, 400, QStringLiteral("Obsolete header line folding is not accepted"));
        const qsizetype colon = line.indexOf(':');
        if (colon <= 0)
            return fail_head(status, why, 400, QStringLiteral("Malformed header line"));
        const QByteArray name = line.left(colon).toLower();
        for (const char ch : name) {
            const auto u = static_cast<unsigned char>(ch);
            if (u <= 0x20 || u >= 0x7f)
                return fail_head(status, why, 400, QStringLiteral("Malformed header name"));
        }
        const QByteArray value = line.mid(colon + 1).trimmed();
        if (name == "content-length") {
            const auto existing = c.headers.constFind(name);
            if (existing != c.headers.constEnd() && existing.value() != value)
                return fail_head(status, why, 400, QStringLiteral("Conflicting Content-Length headers"));
            c.headers.insert(name, value);
        } else if (c.headers.contains(name)) {
            c.headers[name] += QByteArrayLiteral(", ") + value;
        } else {
            c.headers.insert(name, value);
        }
    }

    // Framing. Both TE and CL is the classic request-smuggling shape — refuse it.
    c.chunked = false;
    c.content_length = 0;
    const QByteArray te = c.headers.value("transfer-encoding").trimmed().toLower();
    const bool has_length = c.headers.contains("content-length");
    if (!te.isEmpty()) {
        if (has_length)
            return fail_head(status, why, 400, QStringLiteral("Both Transfer-Encoding and Content-Length present"));
        if (te != "chunked")
            return fail_head(status, why, 501, QStringLiteral("Only chunked Transfer-Encoding is supported"));
        c.chunked = true;
    } else if (has_length) {
        const QByteArray v = c.headers.value("content-length");
        bool digits = !v.isEmpty() && v.size() <= 18;
        for (const char ch : v) {
            if (ch < '0' || ch > '9')
                digits = false;
        }
        if (!digits)
            return fail_head(status, why, 400, QStringLiteral("Invalid Content-Length"));
        c.content_length = v.toLongLong();
        if (c.content_length > kMaxBodyBytes)
            return fail_head(status, why, 413, QStringLiteral("Request body too large (max 8 MB)"));
    }

    const QList<QByteArray> connection = header_tokens(c.headers.value("connection"));
    c.keep_alive = c.version == "HTTP/1.1" ? !connection.contains("close") : connection.contains("keep-alive");
    c.expect_continue = c.headers.value("expect").trimmed().toLower() == "100-continue";
    return true;
}

int decode_chunked_body(Conn& c, int* status) {
    for (;;) {
        switch (c.chunk_state) {
            case 0: { // chunk-size [; ext] CRLF
                const qsizetype eol = c.buf.indexOf("\r\n");
                if (eol < 0) {
                    if (c.buf.size() > 1024) {
                        *status = 400;
                        return -1;
                    }
                    return 0;
                }
                QByteArray size_line = c.buf.left(eol);
                c.buf.remove(0, eol + 2);
                const qsizetype semi = size_line.indexOf(';');
                if (semi >= 0)
                    size_line.truncate(semi);
                size_line = size_line.trimmed();
                bool ok = false;
                const qint64 n = size_line.toLongLong(&ok, 16);
                if (!ok || n < 0 || size_line.isEmpty() || size_line.size() > 15) {
                    *status = 400;
                    return -1;
                }
                if (c.body.size() + n > kMaxBodyBytes) {
                    *status = 413;
                    return -1;
                }
                if (n == 0) {
                    c.chunk_state = 3;
                } else {
                    c.chunk_left = n;
                    c.chunk_state = 1;
                }
                break;
            }
            case 1: { // chunk data
                const qint64 take = std::min<qint64>(c.chunk_left, c.buf.size());
                if (take > 0) {
                    c.body.append(c.buf.constData(), static_cast<qsizetype>(take));
                    c.buf.remove(0, static_cast<qsizetype>(take));
                    c.chunk_left -= take;
                }
                if (c.chunk_left > 0)
                    return 0;
                c.chunk_state = 2;
                break;
            }
            case 2: { // CRLF after the data
                if (c.buf.size() < 2)
                    return 0;
                if (!c.buf.startsWith("\r\n")) {
                    *status = 400;
                    return -1;
                }
                c.buf.remove(0, 2);
                c.chunk_state = 0;
                break;
            }
            default: { // trailer fields, ended by an empty line
                const qsizetype eol = c.buf.indexOf("\r\n");
                if (eol < 0) {
                    if (c.buf.size() > kMaxHeaderBytes) {
                        *status = 431;
                        return -1;
                    }
                    return 0;
                }
                c.buf.remove(0, eol + 2);
                if (eol == 0) {
                    c.chunk_state = 0;
                    return 1;
                }
                break; // trailer fields are ignored
            }
        }
    }
}

QByteArray request_path(const QByteArray& target) {
    QByteArray t = target;
    if (t.startsWith("http://") || t.startsWith("https://")) { // absolute-form
        const qsizetype authority = t.indexOf("://") + 3;
        const qsizetype slash = t.indexOf('/', authority);
        t = slash < 0 ? QByteArrayLiteral("/") : t.mid(slash);
    }
    const qsizetype q = t.indexOf('?');
    if (q >= 0)
        t.truncate(q);
    const qsizetype h = t.indexOf('#');
    if (h >= 0)
        t.truncate(h);
    if (t.size() > 1 && t.endsWith('/'))
        t.chop(1);
    return t;
}

const char* reason_phrase(int status) {
    switch (status) {
        case 100:
            return "Continue";
        case 200:
            return "OK";
        case 202:
            return "Accepted";
        case 400:
            return "Bad Request";
        case 401:
            return "Unauthorized";
        case 403:
            return "Forbidden";
        case 404:
            return "Not Found";
        case 405:
            return "Method Not Allowed";
        case 408:
            return "Request Timeout";
        case 411:
            return "Length Required";
        case 413:
            return "Payload Too Large";
        case 431:
            return "Request Header Fields Too Large";
        case 500:
            return "Internal Server Error";
        case 501:
            return "Not Implemented";
        case 503:
            return "Service Unavailable";
        case 505:
            return "HTTP Version Not Supported";
        default:
            return "Unknown";
    }
}

} // namespace mcp_http_detail

// ── Connections ──────────────────────────────────────────────────────────────

void McpHttpServer::Impl::on_new_connection() {
    using namespace mcp_http_detail;
    while (server && server->hasPendingConnections()) {
        QTcpSocket* sock = server->nextPendingConnection();
        if (!sock)
            break;
        int live = 0;
        for (auto it = conns.cbegin(); it != conns.cend(); ++it) {
            if (!it->closing)
                ++live;
        }
        if (live >= kMaxConnections) {
            LOG_WARN(kTag, QStringLiteral("Refusing a connection — %1 already open").arg(live));
            sock->abort();
            sock->deleteLater();
            continue;
        }
        Conn c;
        c.id = next_conn_id++;
        c.last_activity_ms = now_ms();
        conns.insert(sock, c);
        const quint64 id = c.id;
        QObject::connect(sock, &QTcpSocket::readyRead, q, [this, sock]() { on_ready_read(sock); });
        QObject::connect(sock, &QTcpSocket::disconnected, q, [this, sock, id]() { on_disconnected(sock, id); });
    }
}

void McpHttpServer::Impl::on_ready_read(QTcpSocket* sock) {
    using namespace mcp_http_detail;
    const QByteArray data = sock->readAll();
    auto it = conns.find(sock);
    if (it == conns.end() || it->closing)
        return;
    it->buf.append(data);
    it->last_activity_ms = now_ms();
    // Also caps what a client may pipeline behind a request that is still running.
    if (it->buf.size() > kMaxBodyBytes + kMaxHeaderBytes) {
        fail_and_close(sock, 413, QStringLiteral("Request too large"));
        return;
    }
    process(sock);
}

void McpHttpServer::Impl::on_disconnected(QTcpSocket* sock, quint64 conn_id) {
    // Never erase here: this can fire synchronously from inside respond() while
    // process() holds a reference into `conns`. Mark it, and clean up next turn.
    auto it = conns.find(sock);
    if (it != conns.end() && it->id == conn_id)
        it->closing = true;
    QPointer<QTcpSocket> guard(sock);
    QMetaObject::invokeMethod(
        q,
        [this, sock, conn_id, guard]() {
            const auto found = conns.find(sock);
            if (found != conns.end() && found->id == conn_id)
                conns.erase(found);
            if (guard)
                guard->deleteLater();
        },
        Qt::QueuedConnection);
}

void McpHttpServer::Impl::process(QTcpSocket* sock) {
    using namespace mcp_http_detail;
    for (;;) {
        auto it = conns.find(sock);
        if (it == conns.end())
            return;
        Conn& c = it.value();
        if (c.busy || c.closing)
            return;

        if (!c.head_done) {
            while (c.buf.startsWith("\r\n")) // stray CRLF between pipelined requests (RFC 9112 §2.2)
                c.buf.remove(0, 2);
            const qsizetype end = c.buf.indexOf("\r\n\r\n");
            if (end < 0) {
                if (c.buf.size() > kMaxHeaderBytes)
                    fail_and_close(sock, 431, QStringLiteral("Request header block too large"));
                return;
            }
            if (end + 4 > kMaxHeaderBytes) {
                fail_and_close(sock, 431, QStringLiteral("Request header block too large"));
                return;
            }
            c.head = c.buf.left(end + 4);
            c.buf.remove(0, end + 4);
            int status = 400;
            QString why;
            if (!parse_request_head(c, &status, &why)) {
                fail_and_close(sock, status, why);
                return;
            }
            c.head_done = true;
            if (reject_early(sock))
                return; // answered with Connection: close; the body is never read
            if (c.expect_continue && sock->state() == QAbstractSocket::ConnectedState)
                sock->write("HTTP/1.1 100 Continue\r\n\r\n");
        }

        if (c.chunked) {
            int status = 400;
            const int r = decode_chunked_body(c, &status);
            if (r < 0) {
                fail_and_close(sock, status,
                               status == 413 ? QStringLiteral("Request body too large (max 8 MB)")
                                             : QStringLiteral("Malformed chunked request body"));
                return;
            }
            if (r == 0)
                return;
        } else if (c.body.size() < c.content_length) {
            const qint64 need = c.content_length - c.body.size();
            const auto take = static_cast<qsizetype>(std::min<qint64>(need, c.buf.size()));
            c.body.append(c.buf.constData(), take);
            c.buf.remove(0, take);
            if (c.body.size() < c.content_length)
                return;
        }

        c.busy = true;
        handle_request(sock);
        // A synchronous reply cleared `busy` (keep-alive): loop for a pipelined
        // request. A tools/call reply arrives later and re-enters process().
    }
}

bool McpHttpServer::Impl::reject_early(QTcpSocket* sock) {
    using namespace mcp_http_detail;
    auto it = conns.find(sock);
    if (it == conns.end())
        return true;
    const Conn& c = it.value();

    const quint16 bound = server ? static_cast<quint16>(server->serverPort()) : 0;
    const auth::LoopbackCheck origin = auth::check_loopback_request(c.head, bound, /*allow_cross_site_navigation=*/false);
    if (!origin.allowed) {
        LOG_WARN(kTag, QStringLiteral("Rejected request — %1").arg(origin.reason));
        fail_and_close(sock, 403, QStringLiteral("Request rejected by origin policy"));
        return true;
    }

    if (request_path(c.target) != "/mcp")
        return false;

    const QByteArray auth_header = c.headers.value("authorization").trimmed();
    bool authorized = false;
    if (auth_header.size() > 7 && qstrnicmp(auth_header.constData(), "bearer ", 7) == 0) {
        const QString supplied = QString::fromLatin1(auth_header.mid(7).trimmed());
        const QString expected = effective_token();
        authorized = !expected.isEmpty() && auth::constant_time_equals(supplied, expected);
    }
    if (!authorized) {
        LOG_WARN(kTag, auth_header.isEmpty() ? QStringLiteral("Rejected /mcp request — no bearer token")
                                             : QStringLiteral("Rejected /mcp request — invalid bearer token"));
        const QByteArray challenge = auth_header.isEmpty()
                                         ? QByteArrayLiteral("Bearer realm=\"fincept-terminal\"")
                                         : QByteArrayLiteral("Bearer realm=\"fincept-terminal\", error=\"invalid_token\"");
        respond_json(sock, 401,
                     QJsonObject{{QStringLiteral("error"), QStringLiteral("unauthorized")},
                                 {QStringLiteral("message"),
                                  QStringLiteral("A valid 'Authorization: Bearer <token>' header is required. Copy the "
                                                 "current token from Fincept Terminal → Agent Workbench → Connect.")}},
                     {{QByteArrayLiteral("WWW-Authenticate"), challenge}}, /*force_close=*/true);
        return true;
    }
    if (c.method == "POST" && !c.chunked && !c.headers.contains("content-length")) {
        fail_and_close(sock, 411, QStringLiteral("Content-Length or chunked Transfer-Encoding is required"));
        return true;
    }
    return false;
}

void McpHttpServer::Impl::handle_request(QTcpSocket* sock) {
    using namespace mcp_http_detail;
    auto it = conns.find(sock);
    if (it == conns.end())
        return;
    const QByteArray path = request_path(it->target);
    const QByteArray method = it->method;

    if (path == "/health") {
        if (method != "GET" && method != "HEAD") {
            respond_json(sock, 405, QJsonObject{{QStringLiteral("error"), QStringLiteral("method not allowed")}},
                         {{QByteArrayLiteral("Allow"), QByteArrayLiteral("GET, HEAD")}});
            return;
        }
        respond(sock, 200, health_body());
        return;
    }
    if (path == "/mcp") {
        if (method != "POST") {
            respond_json(sock, 405,
                         QJsonObject{{QStringLiteral("error"), QStringLiteral("method not allowed")},
                                     {QStringLiteral("message"),
                                      QStringLiteral("This MCP server is stateless: POST JSON-RPC messages to /mcp. "
                                                     "There is no server-initiated event stream.")}},
                         {{QByteArrayLiteral("Allow"), QByteArrayLiteral("POST")}});
            return;
        }
        handle_mcp_post(sock);
        return;
    }
    respond_json(sock, 404, QJsonObject{{QStringLiteral("error"), QStringLiteral("not found")}});
}

void McpHttpServer::Impl::respond(QTcpSocket* sock, int status, const QByteArray& body,
                                  const QList<QPair<QByteArray, QByteArray>>& extra_headers, bool force_close) {
    using namespace mcp_http_detail;
    auto it = conns.find(sock);
    if (it == conns.end())
        return;
    Conn& c = it.value();
    const bool close_after = force_close || !c.keep_alive || c.closing;
    const bool head_only = c.method == "HEAD";

    QByteArray out;
    out.reserve(body.size() + 320);
    out += "HTTP/1.1 " + QByteArray::number(status) + ' ' + reason_phrase(status) + "\r\n";
    if (status != 202)
        out += "Content-Type: application/json\r\n";
    out += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
    out += "Cache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\n";
    for (const auto& h : extra_headers)
        out += h.first + ": " + h.second + "\r\n";
    if (close_after)
        out += "Connection: close\r\n";
    else
        out += "Connection: keep-alive\r\nKeep-Alive: timeout=" + QByteArray::number(kKeepAliveHintSecs) + "\r\n";
    out += "\r\n";
    if (!head_only)
        out += body;

    c.last_activity_ms = now_ms();
    reset_request(c);
    c.busy = false;
    if (close_after)
        c.closing = true;
    // `c` must not be touched past this point: disconnectFromHost() can emit
    // disconnected synchronously (see on_disconnected).
    if (sock->state() == QAbstractSocket::ConnectedState)
        sock->write(out);
    if (close_after)
        sock->disconnectFromHost();
}

void McpHttpServer::Impl::respond_json(QTcpSocket* sock, int status, const QJsonObject& body,
                                       const QList<QPair<QByteArray, QByteArray>>& extra_headers, bool force_close) {
    respond(sock, status, mcp_http_detail::compact_json(body), extra_headers, force_close);
}

void McpHttpServer::Impl::fail_and_close(QTcpSocket* sock, int status, const QString& message) {
    respond_json(sock, status, QJsonObject{{QStringLiteral("error"), message}}, {}, /*force_close=*/true);
}

void McpHttpServer::Impl::sweep() {
    using namespace mcp_http_detail;
    const qint64 now = now_ms();
    QList<QTcpSocket*> stale;
    for (auto it = conns.cbegin(); it != conns.cend(); ++it) {
        if (it->closing || it->busy)
            continue;
        if (now - it->last_activity_ms > kIdleTimeoutMs)
            stale.append(it.key());
    }
    for (QTcpSocket* sock : stale) {
        auto it = conns.find(sock);
        if (it == conns.end())
            continue;
        if (it->head_done || !it->buf.isEmpty()) {
            fail_and_close(sock, 408, QStringLiteral("Request timed out"));
        } else {
            it->closing = true;
            sock->disconnectFromHost();
        }
    }
}

} // namespace fincept::mcp
