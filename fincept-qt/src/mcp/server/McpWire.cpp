// McpWire.cpp — see McpWire.h.

#include "mcp/server/McpWire.h"

#include <QJsonDocument>

#include <algorithm>
#include <cmath>

namespace fincept::mcp::wire {

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

bool all_digits(const QByteArray& v) {
    if (v.isEmpty())
        return false;
    return std::all_of(v.cbegin(), v.cend(), [](char ch) { return ch >= '0' && ch <= '9'; });
}

} // namespace

// ── HttpRequestParser ────────────────────────────────────────────────────────

void HttpRequestParser::reset() {
    req_ = HttpRequest{};
    head_done_ = false;
    head_reported_ = false;
    chunk_state_ = 0;
    chunk_left_ = 0;
    trailer_bytes_ = 0;
    partial_.clear();
    error_status_ = 400;
    error_text_.clear();
}

HttpRequestParser::State HttpRequestParser::fail(int status, const QString& why) {
    error_status_ = status;
    error_text_ = why;
    return State::Error;
}

HttpRequestParser::State HttpRequestParser::feed(QByteArray& buf) {
    if (!head_done_) {
        // Stray CRLFs between pipelined requests are allowed (RFC 9112 §2.2).
        qsizetype skip = 0;
        while (skip + 1 < buf.size() && buf.at(skip) == '\r' && buf.at(skip + 1) == '\n')
            skip += 2;
        if (skip > 0)
            buf.remove(0, skip);
        const qsizetype end = buf.indexOf("\r\n\r\n");
        if (end < 0) {
            if (buf.size() > kMaxHeaderBytes)
                return fail(431, QStringLiteral("Request header block too large"));
            partial_ = buf.isEmpty() ? QByteArray() : QByteArray("1");
            return State::NeedMore;
        }
        if (end + 4 > kMaxHeaderBytes)
            return fail(431, QStringLiteral("Request header block too large"));
        req_.head = buf.left(end + 4);
        buf.remove(0, end + 4);
        if (!parse_head())
            return State::Error;
        head_done_ = true;
    }
    if (!head_reported_) {
        head_reported_ = true;
        return State::HeadReady;
    }

    if (req_.chunked) {
        const int r = decode_chunked(buf);
        if (r < 0)
            return State::Error;
        return r == 0 ? State::NeedMore : State::Complete;
    }
    if (req_.body.size() < req_.content_length) {
        const qint64 need = req_.content_length - req_.body.size();
        const auto take = static_cast<qsizetype>(std::min<qint64>(need, buf.size()));
        if (take > 0) {
            req_.body.append(buf.constData(), take);
            buf.remove(0, take);
        }
        if (req_.body.size() < req_.content_length)
            return State::NeedMore;
    }
    return State::Complete;
}

bool HttpRequestParser::parse_head() {
    const QList<QByteArray> lines = req_.head.split('\n');
    QByteArray request_line = lines.value(0);
    if (request_line.endsWith('\r'))
        request_line.chop(1);
    const QList<QByteArray> parts = request_line.split(' ');
    if (parts.size() != 3 || parts[0].isEmpty() || parts[1].isEmpty() || parts[0].size() > 16 ||
        parts[1].size() > 8192) {
        fail(400, QStringLiteral("Malformed request line"));
        return false;
    }
    for (const char ch : parts[0]) {
        if (ch < 'A' || ch > 'Z') {
            fail(400, QStringLiteral("Malformed request method"));
            return false;
        }
    }
    req_.method = parts[0];
    req_.target = parts[1];
    req_.version = parts[2];
    if (req_.version != "HTTP/1.1" && req_.version != "HTTP/1.0") {
        fail(505, QStringLiteral("HTTP version not supported"));
        return false;
    }

    req_.headers.clear();
    int count = 0;
    for (qsizetype i = 1; i < lines.size(); ++i) {
        QByteArray line = lines.at(i);
        if (line.endsWith('\r'))
            line.chop(1);
        if (line.isEmpty())
            continue;
        if (++count > 200) {
            fail(431, QStringLiteral("Too many request header fields"));
            return false;
        }
        if (line.startsWith(' ') || line.startsWith('\t')) {
            fail(400, QStringLiteral("Obsolete header line folding is not accepted"));
            return false;
        }
        const qsizetype colon = line.indexOf(':');
        if (colon <= 0) {
            fail(400, QStringLiteral("Malformed header line"));
            return false;
        }
        const QByteArray name = line.left(colon).toLower();
        for (const char ch : name) {
            const auto u = static_cast<unsigned char>(ch);
            if (u <= 0x20 || u >= 0x7f) {
                fail(400, QStringLiteral("Malformed header name"));
                return false;
            }
        }
        const QByteArray value = line.mid(colon + 1).trimmed();
        if (name == "content-length") {
            const auto existing = req_.headers.constFind(name);
            if (existing != req_.headers.constEnd() && existing.value() != value) {
                fail(400, QStringLiteral("Conflicting Content-Length headers"));
                return false;
            }
            req_.headers.insert(name, value);
        } else if (req_.headers.contains(name)) {
            req_.headers[name] += QByteArrayLiteral(", ") + value;
        } else {
            req_.headers.insert(name, value);
        }
    }

    // Framing. Both TE and CL is the classic request-smuggling shape — refuse it.
    req_.chunked = false;
    req_.content_length = 0;
    const QByteArray te = req_.headers.value("transfer-encoding").trimmed().toLower();
    const bool has_length = req_.headers.contains("content-length");
    if (!te.isEmpty()) {
        if (has_length) {
            fail(400, QStringLiteral("Both Transfer-Encoding and Content-Length present"));
            return false;
        }
        if (te != "chunked") {
            fail(501, QStringLiteral("Only chunked Transfer-Encoding is supported"));
            return false;
        }
        req_.chunked = true;
    } else if (has_length) {
        const QByteArray v = req_.headers.value("content-length");
        if (!all_digits(v) || v.size() > 18) {
            fail(400, QStringLiteral("Invalid Content-Length"));
            return false;
        }
        req_.content_length = v.toLongLong();
        if (req_.content_length > kMaxBodyBytes) {
            fail(413, QStringLiteral("Request body too large (max 8 MB)"));
            return false;
        }
    }

    const QList<QByteArray> connection = header_tokens(req_.headers.value("connection"));
    req_.keep_alive = req_.version == "HTTP/1.1" ? !connection.contains("close") : connection.contains("keep-alive");
    req_.expect_continue = req_.headers.value("expect").trimmed().toLower() == "100-continue";
    return true;
}

int HttpRequestParser::decode_chunked(QByteArray& buf) {
    for (;;) {
        switch (chunk_state_) {
            case 0: { // chunk-size [; ext] CRLF
                const qsizetype eol = buf.indexOf("\r\n");
                if (eol < 0) {
                    if (buf.size() > kMaxChunkLine) {
                        fail(400, QStringLiteral("Malformed chunked request body"));
                        return -1;
                    }
                    return 0;
                }
                if (eol > kMaxChunkLine) {
                    fail(400, QStringLiteral("Malformed chunked request body"));
                    return -1;
                }
                QByteArray size_line = buf.left(eol);
                buf.remove(0, eol + 2);
                const qsizetype semi = size_line.indexOf(';');
                if (semi >= 0)
                    size_line.truncate(semi);
                size_line = size_line.trimmed();
                bool ok = false;
                const qint64 n = size_line.isEmpty() || size_line.size() > 15 ? -1 : size_line.toLongLong(&ok, 16);
                if (!ok || n < 0) {
                    fail(400, QStringLiteral("Malformed chunked request body"));
                    return -1;
                }
                if (req_.body.size() + n > kMaxBodyBytes) {
                    fail(413, QStringLiteral("Request body too large (max 8 MB)"));
                    return -1;
                }
                if (n == 0) {
                    chunk_state_ = 3;
                } else {
                    chunk_left_ = n;
                    chunk_state_ = 1;
                }
                break;
            }
            case 1: { // chunk data
                const qint64 take = std::min<qint64>(chunk_left_, buf.size());
                if (take > 0) {
                    req_.body.append(buf.constData(), static_cast<qsizetype>(take));
                    buf.remove(0, static_cast<qsizetype>(take));
                    chunk_left_ -= take;
                }
                if (chunk_left_ > 0)
                    return 0;
                chunk_state_ = 2;
                break;
            }
            case 2: { // CRLF after the data
                if (buf.size() < 2)
                    return 0;
                if (!buf.startsWith("\r\n")) {
                    fail(400, QStringLiteral("Malformed chunked request body"));
                    return -1;
                }
                buf.remove(0, 2);
                chunk_state_ = 0;
                break;
            }
            default: { // trailer fields, ended by an empty line
                const qsizetype eol = buf.indexOf("\r\n");
                if (eol < 0) {
                    if (trailer_bytes_ + buf.size() > kMaxHeaderBytes) {
                        fail(431, QStringLiteral("Trailer block too large"));
                        return -1;
                    }
                    return 0;
                }
                trailer_bytes_ += eol + 2;
                if (trailer_bytes_ > kMaxHeaderBytes) {
                    fail(431, QStringLiteral("Trailer block too large"));
                    return -1;
                }
                buf.remove(0, eol + 2);
                if (eol == 0) {
                    chunk_state_ = 0;
                    return 1;
                }
                break; // trailer fields are ignored
            }
        }
    }
}

// ── Paths, statuses, Accept ──────────────────────────────────────────────────

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
        case 204:
            return "No Content";
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
        case 406:
            return "Not Acceptable";
        case 408:
            return "Request Timeout";
        case 411:
            return "Length Required";
        case 413:
            return "Payload Too Large";
        case 415:
            return "Unsupported Media Type";
        case 429:
            return "Too Many Requests";
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

bool accepts(const QByteArray& accept_header, const char* mime) {
    const QByteArray value = accept_header.trimmed();
    if (value.isEmpty())
        return true;
    const QByteArray want = QByteArray(mime).toLower();
    const qsizetype slash = want.indexOf('/');
    const QByteArray want_type = slash < 0 ? want : want.left(slash);
    for (const QByteArray& raw : value.split(',')) {
        QByteArray range = raw.trimmed().toLower();
        // Parameters: drop all but notice an explicit q=0 ("not acceptable").
        bool refused = false;
        const QList<QByteArray> params = range.split(';');
        range = params.value(0).trimmed();
        for (qsizetype i = 1; i < params.size(); ++i) {
            const QByteArray p = params.at(i).trimmed();
            if (p.startsWith("q=")) {
                bool ok = false;
                const double q = p.mid(2).toDouble(&ok);
                refused = ok && q <= 0.0;
            }
        }
        if (refused)
            continue;
        if (range == "*/*" || range == want || range == want_type + "/*")
            return true;
    }
    return false;
}

// ── Responses ────────────────────────────────────────────────────────────────

namespace {

void append_common(QByteArray& out, const HeaderList& extra_headers, bool close_after, int keep_alive_secs) {
    out += "Cache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\n";
    for (const auto& h : extra_headers)
        out += h.first + ": " + h.second + "\r\n";
    if (close_after)
        out += "Connection: close\r\n";
    else
        out += "Connection: keep-alive\r\nKeep-Alive: timeout=" + QByteArray::number(keep_alive_secs) + "\r\n";
    out += "\r\n";
}

} // namespace

QByteArray build_response(int status, const QByteArray& content_type, const QByteArray& body,
                          const HeaderList& extra_headers, bool close_after, int keep_alive_secs, bool head_only) {
    QByteArray out;
    out.reserve(body.size() + 320);
    out += "HTTP/1.1 " + QByteArray::number(status) + ' ' + reason_phrase(status) + "\r\n";
    if (!content_type.isEmpty())
        out += "Content-Type: " + content_type + "\r\n";
    out += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
    append_common(out, extra_headers, close_after, keep_alive_secs);
    if (!head_only)
        out += body;
    return out;
}

QByteArray build_sse_head(const HeaderList& extra_headers, bool close_after, int keep_alive_secs) {
    QByteArray out = "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nTransfer-Encoding: chunked\r\n";
    append_common(out, extra_headers, close_after, keep_alive_secs);
    return out;
}

QByteArray http_chunk(const QByteArray& data) {
    return QByteArray::number(data.size(), 16) + "\r\n" + data + "\r\n";
}

QByteArray sse_message(const QByteArray& compact) {
    // Compact JSON never contains a raw newline (strings escape it), so one
    // data line carries the whole message.
    return QByteArrayLiteral("event: message\ndata: ") + compact + "\n\n";
}

QByteArray sse_comment(const QByteArray& text) {
    QByteArray clean = text;
    clean.replace('\n', ' ');
    clean.replace('\r', ' ');
    return ": " + clean + "\n\n";
}

// ── JSON ─────────────────────────────────────────────────────────────────────

QByteArray compact_json(const QJsonObject& object) {
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

QByteArray compact_json(const QJsonArray& array) {
    return QJsonDocument(array).toJson(QJsonDocument::Compact);
}

QByteArray compact_json_value(const QJsonValue& value) {
    if (value.isObject())
        return compact_json(value.toObject());
    if (value.isArray())
        return compact_json(value.toArray());
    const QByteArray wrapped = compact_json(QJsonArray{value});
    return wrapped.mid(1, wrapped.size() - 2);
}

// ── JSON-RPC ─────────────────────────────────────────────────────────────────

QJsonObject rpc_result(const QJsonValue& id, const QJsonValue& result) {
    return QJsonObject{{QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                       {QStringLiteral("id"), id},
                       {QStringLiteral("result"), result}};
}

QJsonObject rpc_error_body(int code, const QString& message) {
    return QJsonObject{{QStringLiteral("code"), code}, {QStringLiteral("message"), message}};
}

QJsonObject rpc_error(const QJsonValue& id, int code, const QString& message) {
    return QJsonObject{{QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                       {QStringLiteral("id"), id},
                       {QStringLiteral("error"), rpc_error_body(code, message)}};
}

bool valid_request_id(const QJsonValue& id) {
    if (id.isString())
        return id.toString().size() <= 1024;
    // Any finite number: QJsonValue keeps 64-bit integers exact, so the id is
    // echoed back exactly as the client wrote it.
    return id.isDouble() && std::isfinite(id.toDouble());
}

QString request_id_key(const QJsonValue& id) {
    if (id.isString())
        return QStringLiteral("s:") + id.toString();
    const double d = id.toDouble();
    if (std::floor(d) == d && std::fabs(d) < 9.2e18)
        return QStringLiteral("n:") + QString::number(id.toInteger(static_cast<qint64>(d)));
    return QStringLiteral("n:") + QString::number(d, 'g', 17);
}

QJsonObject text_block(const QString& text) {
    return QJsonObject{{QStringLiteral("type"), QStringLiteral("text")}, {QStringLiteral("text"), text}};
}

QJsonObject error_call_result(const QString& message) {
    return QJsonObject{{QStringLiteral("content"), QJsonArray{text_block(message)}}, {QStringLiteral("isError"), true}};
}

// ── Protocol versions ────────────────────────────────────────────────────────

const QStringList& supported_protocol_versions() {
    static const QStringList k = {QStringLiteral("2025-11-25"), QStringLiteral("2025-06-18"),
                                  QStringLiteral("2025-03-26"), QStringLiteral("2024-11-05")};
    return k;
}

QString negotiate_protocol_version(const QString& requested) {
    return supported_protocol_versions().contains(requested) ? requested : QString::fromLatin1(kLatestProtocol);
}

QString unsupported_protocol_requested(const QByteArray& header_value, const QJsonObject& params) {
    const QString header = QString::fromLatin1(header_value.trimmed()).left(40);
    if (!header.isEmpty() && !supported_protocol_versions().contains(header))
        return header;
    const QJsonValue meta_version = params.value(QStringLiteral("_meta"))
                                        .toObject()
                                        .value(QStringLiteral("io.modelcontextprotocol/protocolVersion"));
    if (meta_version.isString() && !supported_protocol_versions().contains(meta_version.toString()))
        return meta_version.toString().left(40);
    return {};
}

bool valid_session_id(const QByteArray& id) {
    if (id.isEmpty() || id.size() > 128)
        return false;
    return std::all_of(id.cbegin(), id.cend(), [](char ch) {
        const auto u = static_cast<unsigned char>(ch);
        return u >= 0x21 && u <= 0x7e;
    });
}

} // namespace fincept::mcp::wire
