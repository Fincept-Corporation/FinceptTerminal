// McpServerEngine.cpp — see McpServerEngine.h.

#include "mcp/server/McpServerEngine.h"

#include "auth/LoopbackGuard.h"
#include "core/logging/Logger.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QMetaObject>
#include <QMutexLocker>
#include <QRandomGenerator>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

#include <algorithm>
#include <array>
#include <cmath>
#include <exception>

namespace fincept::mcp::server {

namespace {

constexpr const char* kTag = "McpServer";
constexpr int kMaxBatch = 100;
constexpr int kMaxTrackedAgents = 256;
constexpr int kClientNameMaxChars = 64;
constexpr qint64 kCancelGraceMs = 3000;   // a cancelled call is answered by then even if the tool ignores it
constexpr qint64 kProgressMinGapMs = 200; // progress notifications per call at most this often
constexpr qint64 kShutdownFlushMs = 400; // the GUI thread waits for shutdown: keep it short

qint64 now_ms() {
    return QDateTime::currentMSecsSinceEpoch();
}

QString sanitize_client_text(const QString& raw, int max_chars) {
    QString out;
    out.reserve(std::min<qsizetype>(raw.size(), max_chars * 2));
    for (const QChar c : raw) {
        if (c.isPrint())
            out.append(c);
        if (out.size() > max_chars * 2)
            break;
    }
    return out.simplified().left(max_chars);
}

QString clip(const QString& text, int max_chars) {
    const QString flat = text.simplified();
    return flat.size() <= max_chars ? flat : flat.left(std::max(0, max_chars - 1)) + QChar(0x2026);
}

QByteArray random_session_id() {
    std::array<quint32, 6> words{};
    QRandomGenerator::system()->fillRange(words.data(), static_cast<qsizetype>(words.size()));
    const QByteArray raw(reinterpret_cast<const char*>(words.data()), static_cast<qsizetype>(sizeof(words)));
    return raw.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
}

QByteArray bearer_of(const wire::HttpRequest& req) {
    const QByteArray h = req.header("authorization").trimmed();
    if (h.size() <= 7 || qstrnicmp(h.constData(), "bearer ", 7) != 0)
        return {};
    return h.mid(7).trimmed();
}

QJsonObject json_error_body(const QString& message) {
    return QJsonObject{{QStringLiteral("error"), message}};
}

} // namespace

// ── Private state ────────────────────────────────────────────────────────────

struct McpServerEngine::Conn {
    quint64 id = 0;
    QTcpSocket* sock = nullptr;
    QByteArray buf;
    wire::HttpRequestParser parser;
    AuthInfo auth;               // of the request being served (checked at its head)
    bool busy = false;           // a request is being served (tools/call may be running)
    bool closing = false;        // close initiated / peer gone — ignore further input
    bool paused = false;         // write backlog above the limit: stop serving pipelined requests
    qint64 last_activity_ms = 0;
    qint64 request_started_ms = 0; // first byte of the request in progress; 0 = between requests
    QString client_name;         // clientInfo.name of an `initialize` on this connection
};

struct McpServerEngine::ClientCtx {
    AuthInfo auth;
    QByteArray bearer;
    profiles::ClientKind kind = profiles::ClientKind::Generic;
    QString label;      // who is asking, for activity and approvals
    QString scope;      // cancellation / per-client concurrency scope
    QString session_id; // empty = stateless
    QByteArray user_agent;
};

struct McpServerEngine::Session {
    QString id;
    QByteArray auth_fp;
    profiles::ClientKind kind = profiles::ClientKind::Generic;
    QString client_name;
    QString client_version;
    QString protocol;
    QByteArray user_agent;
    qint64 created_ms = 0;
    qint64 last_seen_ms = 0;
    bool initialized = false;
};

struct McpServerEngine::LiveCall {
    quint64 call_id = 0;
    quint64 http_id = 0; // 0 = nobody waits for the answer any more (client gone)
    int slot = -1;
    QJsonValue rpc_id;
    QString tool;
    QString client_label;
    QString client_scope;
    QString running_key;
    profiles::ClientKind kind = profiles::ClientKind::Generic;
    bool keep_structured = false; // the tool declares an outputSchema
    CancelFlag cancel;
    qint64 started_ms = 0;
    qint64 deadline_ms = 0;
    QJsonValue progress_token;
    double last_progress = -1.0;
    qint64 last_progress_sent_ms = 0;
};

struct McpServerEngine::Pending {
    quint64 http_id = 0;
    QPointer<QTcpSocket> sock;
    quint64 conn_id = 0;
    bool batch = false;
    QVector<QJsonValue> responses;
    int outstanding = 0;
    QVector<quint64> call_ids;
    bool sse_allowed = false; // the client accepts text/event-stream for this POST
    bool sse_only = false;    // …and NOT application/json
    bool sse_open = false;
    bool close_after = false;
    qint64 created_ms = 0;
    qint64 last_write_ms = 0;
    wire::HeaderList extra_headers; // Mcp-Session-Id on an initialize response
};

struct McpServerEngine::Link {
    QMutex mutex;
    McpServerEngine* engine = nullptr;
};

namespace {

/// Post `fn(engine)` to the engine's thread if the engine still exists.
/// Callable from any thread: the mutex keeps the engine alive for the duration
/// of the post (its destructor clears the pointer under the same mutex first),
/// and Qt drops the event if the engine is destroyed before it runs.
template <typename LinkT, typename Fn>
void post_to_engine(const std::shared_ptr<LinkT>& link, Fn fn) {
    QMutexLocker lock(&link->mutex);
    auto* engine = link->engine;
    if (!engine)
        return;
    QMetaObject::invokeMethod(
        engine, [engine, fn = std::move(fn)]() { fn(engine); }, Qt::QueuedConnection);
}

} // namespace

const ToolMeta* Catalogue::resolve(const QString& called) const {
    if (called.isEmpty())
        return nullptr;
    auto it = tools.constFind(called);
    if (it != tools.cend())
        return &it.value();
    const auto adv = internal_to_advertised.constFind(called);
    if (adv != internal_to_advertised.cend()) {
        it = tools.constFind(adv.value());
        if (it != tools.cend())
            return &it.value();
    }
    return nullptr;
}

// ── Lifecycle ────────────────────────────────────────────────────────────────

McpServerEngine::McpServerEngine(std::shared_ptr<McpServerBackend> backend, EngineLimits limits, QObject* parent)
    : QObject(parent), backend_(std::move(backend)), limits_(limits), link_(std::make_shared<Link>()) {
    link_->engine = this;
    ticker_ = new QTimer(this);
    ticker_->setInterval(limits_.tick_ms);
    connect(ticker_, &QTimer::timeout, this, [this]() { tick(); });
}

McpServerEngine::~McpServerEngine() {
    {
        QMutexLocker lock(&link_->mutex);
        link_->engine = nullptr; // late completions from backend threads go nowhere
    }
    shutdown(QStringLiteral("server stopped"));
}

bool McpServerEngine::listen(quint16 first_port, int fallback_range, QString* error) {
    if (server_)
        return true;
    shut_down_ = false;
    auto* srv = new QTcpServer(this);
    srv->setMaxPendingConnections(limits_.max_connections);
    quint16 bound = 0;
    QString err;
    const int last = first_port == 0 ? 0 : std::min(65535, static_cast<int>(first_port) + std::max(0, fallback_range));
    for (int p = first_port; p <= last; ++p) {
        if (p != 0 && limits_.forbidden_ports.contains(static_cast<quint16>(p))) {
            err = QStringLiteral("port %1 is reserved for another program").arg(p);
            continue; // never even tried: another program's port
        }
        if (p == 0) {
            // OS-assigned: draw again if it lands on a reserved port.
            for (int attempt = 0; attempt < 8 && bound == 0; ++attempt) {
                if (!srv->listen(QHostAddress::LocalHost, 0)) {
                    err = srv->errorString();
                    break;
                }
                const auto got = static_cast<quint16>(srv->serverPort());
                if (!limits_.forbidden_ports.contains(got))
                    bound = got;
                else
                    srv->close();
            }
            break;
        }
        if (srv->listen(QHostAddress::LocalHost, static_cast<quint16>(p))) {
            bound = static_cast<quint16>(srv->serverPort());
            break;
        }
        err = srv->errorString();
    }
    if (bound == 0) {
        if (error)
            *error = first_port == last || first_port == 0
                         ? QStringLiteral("Could not listen on 127.0.0.1:%1 — %2").arg(first_port).arg(err)
                         : QStringLiteral("Could not listen on 127.0.0.1 ports %1-%2 — %3")
                               .arg(first_port)
                               .arg(last)
                               .arg(err);
        delete srv;
        return false;
    }
    server_ = srv;
    port_.store(bound);
    connect(srv, &QTcpServer::newConnection, this, [this]() { on_new_connection(); });
    connect(srv, &QTcpServer::acceptError, this, [this](QAbstractSocket::SocketError) {
        if (server_)
            LOG_WARN_RL(kTag, QStringLiteral("accept"), 10000,
                        logf("MCP server accept failed", {{"error", server_->errorString()}}));
    });
    ticker_->start();
    return true;
}

void McpServerEngine::shutdown(const QString& reason) {
    if (shut_down_)
        return;
    shut_down_ = true;
    ticker_->stop();
    if (server_)
        server_->close(); // no new connections

    // Every call still running is answered — a client must never wait on a
    // request the server has forgotten — and its tool is told to stop.
    const QString text =
        QStringLiteral("Fincept Terminal's MCP server stopped (%1) before this tool call finished. The action may or "
                       "may not have completed; check the terminal before retrying.")
            .arg(reason);
    const QList<quint64> live = calls_.keys();
    for (quint64 id : live) {
        LiveCall* lc = calls_.value(id);
        if (!lc)
            continue;
        lc->cancel->store(true);
        complete_slot(id, wire::rpc_result(lc->rpc_id, wire::error_call_result(text)), false, false,
                      QStringLiteral("server stopped"));
    }

    // Flush what was just written (bounded), then close every connection.
    const qint64 flush_until = now_ms() + kShutdownFlushMs;
    for (auto it = conns_.begin(); it != conns_.end(); ++it) {
        QTcpSocket* sock = it.key();
        QObject::disconnect(sock, nullptr, this, nullptr);
        // A socket that just got its final answer may already be closing (its
        // request said Connection: close) with the answer still unsent.
        const auto state = sock->state();
        if (state == QAbstractSocket::ConnectedState || state == QAbstractSocket::ClosingState) {
            sock->flush();
            qint64 left = flush_until - now_ms();
            if (sock->bytesToWrite() > 0 && left > 0)
                sock->waitForBytesWritten(static_cast<int>(std::min<qint64>(left, 200)));
            // Graceful close (FIN): an abort (RST) can make the peer discard an
            // answer it received but has not read yet.
            if (sock->state() == QAbstractSocket::ConnectedState)
                sock->disconnectFromHost();
            left = flush_until - now_ms();
            if (sock->state() != QAbstractSocket::UnconnectedState && left > 0)
                sock->waitForDisconnected(static_cast<int>(std::min<qint64>(left, 100)));
        }
        if (sock->state() != QAbstractSocket::UnconnectedState)
            sock->abort(); // could not flush in time: do not hold the shutdown
        sock->deleteLater();
        delete it.value();
    }
    conns_.clear();
    qDeleteAll(pending_);
    pending_.clear();
    qDeleteAll(sessions_);
    sessions_.clear();
    running_by_request_.clear();
    inflight_by_client_.clear();
    if (server_) {
        QObject::disconnect(server_, nullptr, this, nullptr);
        server_->deleteLater();
        server_ = nullptr;
    }
    port_.store(0);
    connections_.store(0);
    sessions_count_.store(0);
    inflight_.store(0);
}

// ── Connections ──────────────────────────────────────────────────────────────

void McpServerEngine::on_new_connection() {
    while (server_ && server_->hasPendingConnections()) {
        QTcpSocket* sock = server_->nextPendingConnection();
        if (!sock)
            break;
        int live = 0;
        for (const Conn* c : std::as_const(conns_))
            live += c->closing ? 0 : 1;
        if (live >= limits_.max_connections) {
            LOG_WARN_RL(kTag, QStringLiteral("conn_cap"), 10000,
                        logf("MCP server refused a connection", {{"open", live}, {"limit", limits_.max_connections}}));
            sock->abort();
            sock->deleteLater();
            continue;
        }
        auto* c = new Conn;
        c->id = next_conn_id_++;
        c->sock = sock;
        c->last_activity_ms = now_ms();
        conns_.insert(sock, c);
        connections_.store(static_cast<int>(conns_.size()));
        const quint64 id = c->id;
        connect(sock, &QTcpSocket::readyRead, this, [this, sock]() { on_ready_read(sock); });
        connect(sock, &QTcpSocket::disconnected, this, [this, sock, id]() { on_disconnected(sock, id); });
        connect(sock, &QTcpSocket::bytesWritten, this, [this, sock](qint64) { on_bytes_written(sock); });
    }
}

void McpServerEngine::on_ready_read(QTcpSocket* sock) {
    const QByteArray data = sock->readAll();
    Conn* c = conns_.value(sock);
    if (!c || c->closing)
        return;
    c->buf.append(data);
    c->last_activity_ms = now_ms();
    if (c->request_started_ms == 0 && !c->buf.isEmpty())
        c->request_started_ms = c->last_activity_ms;
    // Also bounds what a client may pipeline behind a request still running.
    if (c->buf.size() > wire::kMaxBodyBytes + wire::kMaxHeaderBytes) {
        fail_and_close(sock, 413, QStringLiteral("Request too large"));
        return;
    }
    process(sock);
}

void McpServerEngine::on_disconnected(QTcpSocket* sock, quint64 conn_id) {
    // Never erase here: this can fire synchronously from inside a write while a
    // caller holds the Conn. Mark it, stop its calls, and clean up next turn.
    Conn* c = conns_.value(sock);
    if (!c || c->id != conn_id)
        return;
    c->closing = true;
    // The client hung up while a tool was still working for it (client-side
    // timeout, Esc in the agent): nobody will read the answer, so stop the tool.
    drop_pending_for(sock);
    QPointer<QTcpSocket> guard(sock);
    QMetaObject::invokeMethod(
        this,
        [this, sock, conn_id, guard]() {
            Conn* found = conns_.value(sock);
            if (found && found->id == conn_id) {
                conns_.remove(sock);
                delete found;
                connections_.store(static_cast<int>(conns_.size()));
            }
            if (guard)
                guard->deleteLater();
        },
        Qt::QueuedConnection);
}

void McpServerEngine::on_bytes_written(QTcpSocket* sock) {
    Conn* c = conns_.value(sock);
    if (!c || !c->paused)
        return;
    if (sock->bytesToWrite() <= limits_.write_pause_bytes / 4) {
        c->paused = false;
        process(sock);
    }
}

void McpServerEngine::drop_pending_for(QTcpSocket* sock) {
    QList<quint64> dead;
    for (auto it = pending_.cbegin(); it != pending_.cend(); ++it) {
        if (it.value()->sock.data() == sock || it.value()->sock.isNull())
            dead.append(it.key());
    }
    for (quint64 http_id : dead) {
        Pending* p = pending_.take(http_id);
        for (quint64 call_id : std::as_const(p->call_ids)) {
            if (LiveCall* lc = calls_.value(call_id)) {
                lc->cancel->store(true);
                lc->http_id = 0;
                lc->deadline_ms = std::min(lc->deadline_ms, now_ms() + kCancelGraceMs);
            }
        }
        delete p;
    }
}

void McpServerEngine::process(QTcpSocket* sock) {
    for (;;) {
        Conn* c = conns_.value(sock);
        if (!c || c->busy || c->closing || c->paused)
            return;
        if (c->buf.isEmpty() && !c->parser.head_done())
            return;
        const auto state = c->parser.feed(c->buf);
        switch (state) {
            case wire::HttpRequestParser::State::NeedMore:
                return;
            case wire::HttpRequestParser::State::Error:
                fail_and_close(sock, c->parser.error_status(), c->parser.error_text());
                return;
            case wire::HttpRequestParser::State::HeadReady:
                if (!check_head(sock, *c))
                    return; // answered with Connection: close; the body is never read
                if (c->parser.request().expect_continue && sock->state() == QAbstractSocket::ConnectedState)
                    sock->write("HTTP/1.1 100 Continue\r\n\r\n");
                continue;
            case wire::HttpRequestParser::State::Complete:
                c->busy = true;
                c->request_started_ms = 0;
                handle_request(sock, *c);
                // A synchronous answer cleared `busy` (keep-alive): loop for a
                // pipelined request. A tools/call answer re-enters process() later.
                continue;
        }
    }
}

bool McpServerEngine::check_head(QTcpSocket* sock, Conn& c) {
    const wire::HttpRequest& req = c.parser.request();
    // 1. DNS rebinding / browser cross-site requests (Host, Origin, Sec-Fetch-*).
    const auth::LoopbackCheck origin = auth::check_loopback_request(req.head, port_.load(),
                                                                    /*allow_cross_site_navigation=*/false);
    if (!origin.allowed) {
        LOG_WARN_RL(kTag, QStringLiteral("origin"), 10000,
                    logf("MCP request refused by origin policy", {{"reason", origin.reason}}));
        fail_and_close(sock, 403, QStringLiteral("Request rejected by origin policy"));
        return false;
    }
    if (wire::request_path(req.target) != "/mcp")
        return true; // /health needs no token; anything else is a 404 after the body
    // 2. Bearer token, before a single body byte is buffered.
    const QByteArray bearer = bearer_of(req);
    c.auth = bearer.isEmpty() ? AuthInfo{} : backend_->authenticate(bearer);
    if (!c.auth.ok) {
        // Never the header value. Rate-limited: a misconfigured client retries in a loop.
        LOG_WARN_RL(kTag, QStringLiteral("mcp_auth_reject"), 60000,
                    logf("MCP request refused", {{"reason", bearer.isEmpty() ? "no bearer token" : "invalid token"}}));
        const QByteArray challenge = bearer.isEmpty()
                                         ? QByteArrayLiteral("Bearer realm=\"fincept-terminal\"")
                                         : QByteArrayLiteral("Bearer realm=\"fincept-terminal\", error=\"invalid_token\"");
        respond_json(sock, 401,
                     QJsonObject{{QStringLiteral("error"), QStringLiteral("unauthorized")},
                                 {QStringLiteral("message"),
                                  QStringLiteral("A valid 'Authorization: Bearer <token>' header is required. Copy the "
                                                 "current token from Fincept Terminal → Settings → AI Agents (MCP).")}},
                     {{QByteArrayLiteral("WWW-Authenticate"), challenge}}, /*force_close=*/true);
        return false;
    }
    // 3. Framing a POST must declare.
    if (req.method == "POST" && !req.chunked && !req.headers.contains("content-length")) {
        fail_and_close(sock, 411, QStringLiteral("Content-Length or chunked Transfer-Encoding is required"));
        return false;
    }
    return true;
}

void McpServerEngine::handle_request(QTcpSocket* sock, Conn& c) {
    const wire::HttpRequest& req = c.parser.request();
    const QByteArray path = wire::request_path(req.target);
    if (path == "/health") {
        if (req.method != "GET" && req.method != "HEAD") {
            respond_json(sock, 405, json_error_body(QStringLiteral("method not allowed")),
                         {{QByteArrayLiteral("Allow"), QByteArrayLiteral("GET, HEAD")}});
            return;
        }
        const QJsonObject info = backend_->server_info();
        respond_json(sock, 200,
                     QJsonObject{{QStringLiteral("ok"), true},
                                 {QStringLiteral("name"), info.value(QStringLiteral("name"))},
                                 {QStringLiteral("version"), info.value(QStringLiteral("version"))},
                                 // Which edition answered: a client verifies it BEFORE it
                                 // sends a token (the other Fincept build may be listening).
                                 {QStringLiteral("edition"), info.value(QStringLiteral("edition"))},
                                 {QStringLiteral("protocolVersions"),
                                  QJsonArray::fromStringList(wire::supported_protocol_versions())}});
        return;
    }
    if (path != "/mcp") {
        respond_json(sock, 404, json_error_body(QStringLiteral("not found")));
        return;
    }
    if (req.method == "POST") {
        handle_post(sock, c);
        return;
    }
    if (req.method == "DELETE") {
        handle_delete(sock, c);
        return;
    }
    // GET (the optional server→client stream) and anything else: not offered.
    // Clients treat 405 on GET as "no stream" (MCP Streamable HTTP).
    respond_json(sock, 405,
                 QJsonObject{{QStringLiteral("error"), QStringLiteral("method not allowed")},
                             {QStringLiteral("message"),
                              QStringLiteral("POST JSON-RPC messages to /mcp. There is no server-initiated event "
                                             "stream; DELETE ends a session.")}},
                 {{QByteArrayLiteral("Allow"), QByteArrayLiteral("POST, DELETE")}});
}

void McpServerEngine::handle_delete(QTcpSocket* sock, Conn& c) {
    const QString sid = QString::fromLatin1(c.parser.request().header("mcp-session-id").trimmed());
    Session* s = sessions_.value(sid);
    if (!s || s->auth_fp != c.auth.fingerprint) {
        respond_json(sock, 404, json_error_body(QStringLiteral("unknown session")));
        return;
    }
    // Calls still running for the session stop: the client is going away.
    const QString scope = QStringLiteral("s:") + sid;
    for (LiveCall* lc : std::as_const(calls_)) {
        if (lc->client_scope == scope) {
            lc->cancel->store(true);
            lc->deadline_ms = std::min(lc->deadline_ms, now_ms() + kCancelGraceMs);
        }
    }
    sessions_.remove(sid);
    delete s;
    sessions_count_.store(static_cast<int>(sessions_.size()));
    respond(sock, 200, QByteArray(), QByteArray());
}

void McpServerEngine::respond(QTcpSocket* sock, int status, const QByteArray& content_type, const QByteArray& body,
                              const wire::HeaderList& extra, bool force_close) {
    Conn* c = conns_.value(sock);
    if (!c)
        return;
    const wire::HttpRequest& req = c->parser.request();
    const bool close_after = force_close || !req.keep_alive || c->closing;
    const bool head_only = req.method == "HEAD";
    write_raw(sock, wire::build_response(status, content_type, body, extra, close_after, limits_.keep_alive_hint_secs,
                                         head_only));
    finish_connection_request(sock, close_after);
}

void McpServerEngine::respond_json(QTcpSocket* sock, int status, const QJsonObject& body, const wire::HeaderList& extra,
                                   bool force_close) {
    respond(sock, status, QByteArrayLiteral("application/json"), wire::compact_json(body), extra, force_close);
}

void McpServerEngine::fail_and_close(QTcpSocket* sock, int status, const QString& message) {
    respond_json(sock, status, json_error_body(message), {}, /*force_close=*/true);
}

void McpServerEngine::write_raw(QTcpSocket* sock, const QByteArray& bytes) {
    if (sock && sock->state() == QAbstractSocket::ConnectedState)
        sock->write(bytes);
}

void McpServerEngine::finish_connection_request(QTcpSocket* sock, bool close_after) {
    Conn* c = conns_.value(sock);
    if (!c)
        return;
    c->parser.reset();
    c->busy = false;
    c->last_activity_ms = now_ms();
    c->request_started_ms = c->buf.isEmpty() ? 0 : c->last_activity_ms;
    if (close_after) {
        c->closing = true;
        // May emit disconnected synchronously — on_disconnected defers the erase.
        sock->disconnectFromHost();
        return;
    }
    if (sock->bytesToWrite() > limits_.write_pause_bytes)
        c->paused = true; // a client that does not read: stop serving what it pipelined
}

// ── POST /mcp ────────────────────────────────────────────────────────────────

McpServerEngine::ClientCtx McpServerEngine::client_context(const Conn& conn, const wire::HttpRequest& req,
                                                           const AuthInfo& auth) {
    ClientCtx ctx;
    ctx.auth = auth;
    ctx.bearer = bearer_of(req);
    ctx.user_agent = req.header("user-agent").left(256);
    const QByteArray sid = req.header("mcp-session-id").trimmed();
    Session* s = wire::valid_session_id(sid) ? sessions_.value(QString::fromLatin1(sid)) : nullptr;
    if (s && s->auth_fp == auth.fingerprint) {
        s->last_seen_ms = now_ms();
        ctx.session_id = s->id;
        ctx.kind = s->kind;
        ctx.label = s->client_name;
    } else {
        // Stateless (no session, or one this process does not know — e.g. the
        // terminal restarted): who is this, from what was said on this
        // connection or under this User-Agent before.
        QString name = conn.client_name;
        if (name.isEmpty() && !ctx.user_agent.isEmpty())
            name = client_by_agent_.value(ctx.user_agent);
        ctx.kind = profiles::detect_client(name, QString::fromLatin1(ctx.user_agent));
        ctx.label = name.isEmpty() ? QStringLiteral("unknown") : name;
    }
    if (auth.run_token) {
        if (!auth.policy.harness_id.isEmpty()) {
            const profiles::ClientKind k = profiles::kind_for_harness(auth.policy.harness_id);
            if (k != profiles::ClientKind::Generic)
                ctx.kind = k;
        }
        ctx.label += QStringLiteral(" · terminal AI");
    }
    ctx.scope = ctx.session_id.isEmpty()
                    ? QStringLiteral("a:") + QString::fromLatin1(auth.fingerprint) + QLatin1Char('|') +
                          QString::fromLatin1(ctx.user_agent)
                    : QStringLiteral("s:") + ctx.session_id;
    return ctx;
}

void McpServerEngine::handle_post(QTcpSocket* sock, Conn& c) {
    const wire::HttpRequest& req = c.parser.request();
    const QByteArray accept = req.header("accept");
    const bool json_ok = wire::accepts(accept, "application/json");
    // SSE only when the client NAMES it (an absent Accept or */* gets JSON).
    const bool sse_named = accept.toLower().contains("text/event-stream") && wire::accepts(accept, "text/event-stream");
    if (!json_ok && !sse_named) {
        respond_json(sock, 406,
                     wire::rpc_error(QJsonValue(), wire::kInvalidRequest,
                                     QStringLiteral("Not Acceptable: accept application/json or text/event-stream")));
        return;
    }

    QJsonParseError perr{};
    const QJsonDocument doc = QJsonDocument::fromJson(req.body, &perr);
    if (perr.error != QJsonParseError::NoError || (!doc.isObject() && !doc.isArray())) {
        const QString why = perr.error != QJsonParseError::NoError ? perr.errorString()
                                                                   : QStringLiteral("expected a JSON object or array");
        respond_json(sock, 400,
                     wire::rpc_error(QJsonValue(), wire::kParseError, QStringLiteral("Parse error: %1").arg(why)));
        return;
    }
    if (doc.isArray() && (doc.array().isEmpty() || doc.array().size() > kMaxBatch)) {
        respond_json(sock, 400,
                     wire::rpc_error(QJsonValue(), wire::kInvalidRequest,
                                     doc.array().isEmpty() ? QStringLiteral("Invalid Request: empty batch")
                                                           : QStringLiteral("Invalid Request: batch too large")));
        return;
    }

    // A request in a protocol era this server does not speak (MCP 2026-07-28
    // dropped `initialize`): 400 without a modern error body, so a dual-era
    // client falls back to `initialize` (2025-11-25 also requires 400 here).
    const QJsonArray messages = doc.isArray() ? doc.array() : QJsonArray{doc.object()};
    for (const QJsonValue& m : messages) {
        const QJsonObject o = m.toObject();
        const QString method = o.value(QStringLiteral("method")).toString();
        if (method == QLatin1String("initialize"))
            continue; // an initialize negotiates by its body
        QString unsupported = method == QLatin1String("server/discover")
                                  ? QStringLiteral("server/discover")
                                  : wire::unsupported_protocol_requested(req.header("mcp-protocol-version"),
                                                                         o.value(QStringLiteral("params")).toObject());
        if (!unsupported.isEmpty()) {
            LOG_DEBUG(kTag, logf("MCP request in an unsupported protocol era refused", {{"asked", unsupported}}));
            respond_json(sock, 400,
                         QJsonObject{{QStringLiteral("error"), QStringLiteral("unsupported protocol version")},
                                     {QStringLiteral("message"),
                                      QStringLiteral("This server speaks MCP %1 (and older): start with initialize.")
                                          .arg(wire::supported_protocol_versions().join(QStringLiteral(", ")))}});
            return;
        }
    }

    ClientCtx ctx = client_context(c, req, c.auth);
    auto* p = new Pending;
    p->http_id = next_http_id_++;
    p->sock = sock;
    p->conn_id = c.id;
    p->batch = doc.isArray();
    p->close_after = !req.keep_alive;
    p->created_ms = now_ms();
    // Streaming needs chunked framing (HTTP/1.1) and a single request per POST.
    p->sse_allowed = sse_named && !p->batch && req.version == "HTTP/1.1";
    p->sse_only = !json_ok && p->sse_allowed;
    for (const QJsonValue& m : messages)
        handle_message(m, *p, c, ctx);

    if (p->responses.isEmpty()) {
        delete p;
        respond(sock, 202, QByteArray(), QByteArray()); // notifications / responses only
        return;
    }
    if (p->outstanding == 0) {
        write_pending_final(*p);
        const bool close_after = p->close_after;
        delete p;
        finish_connection_request(sock, close_after);
        return;
    }
    pending_.insert(p->http_id, p); // completed by on_call_done(); the connection stays busy
}

void McpServerEngine::handle_message(const QJsonValue& message, Pending& p, Conn& conn, ClientCtx& ctx) {
    if (!message.isObject()) {
        p.responses.append(wire::rpc_error(QJsonValue(), wire::kInvalidRequest,
                                           QStringLiteral("Invalid Request: expected an object")));
        return;
    }
    const QJsonObject m = message.toObject();
    const bool has_id = m.contains(QStringLiteral("id"));
    const QJsonValue id = has_id ? m.value(QStringLiteral("id")) : QJsonValue();
    const QJsonValue method_v = m.value(QStringLiteral("method"));

    if (m.value(QStringLiteral("jsonrpc")).toString() != QLatin1String("2.0")) {
        if (has_id || method_v.isUndefined())
            p.responses.append(wire::rpc_error(wire::valid_request_id(id) ? id : QJsonValue(), wire::kInvalidRequest,
                                               QStringLiteral("Invalid Request: \"jsonrpc\" must be \"2.0\"")));
        return;
    }
    if (method_v.isUndefined()) {
        // A response to a server→client request. This server sends none: ignore.
        if (m.contains(QStringLiteral("result")) || m.contains(QStringLiteral("error")))
            return;
        p.responses.append(wire::rpc_error(wire::valid_request_id(id) ? id : QJsonValue(), wire::kInvalidRequest,
                                           QStringLiteral("Invalid Request: missing \"method\"")));
        return;
    }
    if (!method_v.isString() || method_v.toString().isEmpty()) {
        if (has_id)
            p.responses.append(wire::rpc_error(wire::valid_request_id(id) ? id : QJsonValue(), wire::kInvalidRequest,
                                               QStringLiteral("Invalid Request: \"method\" must be a string")));
        return;
    }
    const QString method = method_v.toString();
    const QJsonValue params_v = m.value(QStringLiteral("params"));

    if (!has_id) { // notification — never answered
        if (method == QLatin1String("notifications/initialized")) {
            if (Session* s = sessions_.value(ctx.session_id))
                s->initialized = true;
        } else if (method == QLatin1String("notifications/cancelled")) {
            const QJsonObject pp = params_v.toObject();
            cancel_request(ctx, pp.value(QStringLiteral("requestId")), pp.value(QStringLiteral("reason")).toString());
        }
        return;
    }
    if (!wire::valid_request_id(id)) {
        p.responses.append(wire::rpc_error(QJsonValue(), wire::kInvalidRequest,
                                           QStringLiteral("Invalid Request: \"id\" must be a string or a number")));
        return;
    }
    if (!params_v.isUndefined() && !params_v.isNull() && !params_v.isObject()) {
        p.responses.append(wire::rpc_error(id, wire::kInvalidParams,
                                           QStringLiteral("Invalid params: \"params\" must be an object")));
        return;
    }
    const QJsonObject params = params_v.toObject();

    // A run-scoped token that died after this request's bearer check.
    if (ctx.auth.run_token && !backend_->authenticate(ctx.bearer).ok) {
        p.responses.append(wire::rpc_error(id, wire::kRunTokenExpired,
                                           QStringLiteral("The run token has expired; this run is over.")));
        return;
    }

    if (method == QLatin1String("tools/call")) {
        start_tool_call(id, params, p, ctx);
        return;
    }
    if (method == QLatin1String("initialize")) {
        QByteArray sid;
        p.responses.append(handle_initialize(id, params, conn, ctx, &sid));
        if (!sid.isEmpty()) {
            p.extra_headers.erase(std::remove_if(p.extra_headers.begin(), p.extra_headers.end(),
                                                 [](const auto& h) { return h.first == "Mcp-Session-Id"; }),
                                  p.extra_headers.end());
            p.extra_headers.append({QByteArrayLiteral("Mcp-Session-Id"), sid});
        }
        return;
    }
    if (method == QLatin1String("ping") || method == QLatin1String("logging/setLevel")) {
        p.responses.append(wire::rpc_result(id, QJsonObject{}));
        return;
    }
    if (method == QLatin1String("tools/list")) {
        p.responses.append(handle_tools_list(id, params, ctx));
        return;
    }
    QJsonObject result;
    int code = wire::kInternalError;
    QString error;
    bool served = false;
    try {
        served = backend_->content_request(method, params, &result, &code, &error);
    } catch (const std::exception& e) {
        served = true;
        code = wire::kInternalError;
        error = QStringLiteral("Internal error: %1").arg(QString::fromUtf8(e.what()));
    } catch (...) {
        served = true;
        code = wire::kInternalError;
        error = QStringLiteral("Internal error");
    }
    if (!served) {
        p.responses.append(wire::rpc_error(id, wire::kMethodNotFound, QStringLiteral("Method not found: %1")
                                                                          .arg(clip(method, 80))));
        return;
    }
    p.responses.append(error.isEmpty() ? wire::rpc_result(id, result) : wire::rpc_error(id, code, error));
}

QJsonObject McpServerEngine::handle_initialize(const QJsonValue& id, const QJsonObject& params, Conn& conn,
                                               ClientCtx& ctx, QByteArray* new_session_id) {
    const QString requested = params.value(QStringLiteral("protocolVersion")).toString();
    const QString negotiated = wire::negotiate_protocol_version(requested);
    const QJsonObject info = params.value(QStringLiteral("clientInfo")).toObject();
    QString name = sanitize_client_text(info.value(QStringLiteral("name")).toString(), kClientNameMaxChars);
    if (name.isEmpty())
        name = QStringLiteral("unknown");
    const QString version = sanitize_client_text(info.value(QStringLiteral("version")).toString(), 32);

    conn.client_name = name;
    if (!ctx.user_agent.isEmpty()) {
        if (client_by_agent_.size() >= kMaxTrackedAgents && !client_by_agent_.contains(ctx.user_agent))
            client_by_agent_.clear();
        client_by_agent_.insert(ctx.user_agent, name);
    }
    profiles::ClientKind kind = profiles::detect_client(name, QString::fromLatin1(ctx.user_agent));
    if (ctx.auth.run_token && !ctx.auth.policy.harness_id.isEmpty()) {
        const profiles::ClientKind k = profiles::kind_for_harness(ctx.auth.policy.harness_id);
        if (k != profiles::ClientKind::Generic)
            kind = k;
    }

    // A session: who this client is (profile, cancellation scope) for every
    // later request that names it. Bounded: the least recently used idle one
    // makes room.
    if (sessions_.size() >= limits_.max_sessions) {
        Session* oldest = nullptr;
        for (Session* s : std::as_const(sessions_)) {
            if (inflight_by_client_.value(QStringLiteral("s:") + s->id) > 0)
                continue;
            if (!oldest || s->last_seen_ms < oldest->last_seen_ms)
                oldest = s;
        }
        if (oldest) {
            sessions_.remove(oldest->id);
            delete oldest;
        }
    }
    if (sessions_.size() < limits_.max_sessions) {
        auto* s = new Session;
        s->id = QString::fromLatin1(random_session_id());
        s->auth_fp = ctx.auth.fingerprint;
        s->kind = kind;
        s->client_name = name;
        s->client_version = version;
        s->protocol = negotiated;
        s->user_agent = ctx.user_agent;
        s->created_ms = s->last_seen_ms = now_ms();
        sessions_.insert(s->id, s);
        sessions_count_.store(static_cast<int>(sessions_.size()));
        *new_session_id = s->id.toLatin1();
        ctx.session_id = s->id;
        ctx.scope = QStringLiteral("s:") + s->id;
    }
    ctx.kind = kind;
    ctx.label = name + (ctx.auth.run_token ? QStringLiteral(" · terminal AI") : QString());

    const profiles::ClientProfile& prof = profiles::profile(kind);
    LOG_INFO(kTag, logf("MCP client connected", {{"client", name},
                                                 {"version", version},
                                                 {"protocol", negotiated},
                                                 {"profile", prof.id},
                                                 {"run_token", ctx.auth.run_token}}));
    emit client_initialized(name, version, sanitize_client_text(QString::fromLatin1(ctx.user_agent), 120),
                            QString::fromLatin1(prof.id), negotiated);

    QJsonObject server_info = backend_->server_info();
    const QJsonObject result{
        {QStringLiteral("protocolVersion"), negotiated},
        {QStringLiteral("capabilities"),
         QJsonObject{{QStringLiteral("tools"), QJsonObject{{QStringLiteral("listChanged"), false}}},
                     {QStringLiteral("prompts"), QJsonObject{{QStringLiteral("listChanged"), false}}},
                     {QStringLiteral("resources"),
                      QJsonObject{{QStringLiteral("listChanged"), false}, {QStringLiteral("subscribe"), false}}}}},
        {QStringLiteral("serverInfo"), server_info},
        {QStringLiteral("instructions"), backend_->instructions()},
    };
    return wire::rpc_result(id, result);
}

QJsonObject McpServerEngine::handle_tools_list(const QJsonValue& id, const QJsonObject& params, const ClientCtx& ctx) {
    const CataloguePtr cat = backend_->catalogue();
    if (!cat)
        return wire::rpc_result(id, QJsonObject{{QStringLiteral("tools"), QJsonArray{}}});
    const int kind_key = static_cast<int>(ctx.kind);
    ShapedList& shaped = shaped_lists_[kind_key];
    if (shaped.generation != cat->generation || (shaped.entries.isEmpty() && !cat->entries.isEmpty())) {
        QStringList warnings;
        shaped.entries = profiles::shape_catalogue(cat->entries, profiles::profile(ctx.kind), &warnings);
        shaped.generation = cat->generation;
        for (const QString& w : std::as_const(warnings))
            LOG_WARN(kTag, logf("Tool catalogue does not fit a client", {{"detail", w}}));
    }
    QJsonArray all = shaped.entries;
    // A run-scoped token never sees the tools its policy refuses.
    if (ctx.auth.run_token && ctx.auth.policy.scope != RunToolScope::All) {
        QJsonArray kept;
        for (const QJsonValue& t : std::as_const(all)) {
            const ToolMeta* meta = cat->resolve(t.toObject().value(QStringLiteral("name")).toString());
            if (!meta || !run_scope_hides_category(ctx.auth.policy.scope, meta->category))
                kept.append(t);
        }
        all = kept;
    }
    const int total = static_cast<int>(all.size());
    int offset = 0;
    const QJsonValue cursor = params.value(QStringLiteral("cursor"));
    if (!cursor.isUndefined() && !cursor.isNull()) {
        const QString s = cursor.toString();
        bool ok = false;
        if (s.startsWith(QLatin1Char('p')))
            offset = s.mid(1).toInt(&ok);
        if (!ok || offset < 0 || offset > total)
            return wire::rpc_error(id, wire::kInvalidParams, QStringLiteral("Invalid params: unknown cursor"));
    }
    const int end = std::min(offset + std::max(1, limits_.tools_page_size), total);
    QJsonArray page;
    for (int i = offset; i < end; ++i)
        page.append(all.at(i));
    QJsonObject result{{QStringLiteral("tools"), page}};
    if (end < total)
        result.insert(QStringLiteral("nextCursor"), QStringLiteral("p%1").arg(end));
    return wire::rpc_result(id, result);
}

void McpServerEngine::start_tool_call(const QJsonValue& id, const QJsonObject& params, Pending& p, ClientCtx& ctx) {
    const QJsonValue name_v = params.value(QStringLiteral("name"));
    const QJsonValue args_v = params.value(QStringLiteral("arguments"));
    const QString called = name_v.toString().trimmed();
    QString invalid;
    if (!name_v.isString() || called.isEmpty())
        invalid = QStringLiteral("Invalid params: \"name\" (the tool name) is required");
    else if (!args_v.isUndefined() && !args_v.isNull() && !args_v.isObject())
        invalid = QStringLiteral("Invalid params: \"arguments\" must be an object");
    if (!invalid.isEmpty()) {
        p.responses.append(wire::rpc_error(id, wire::kInvalidParams, invalid));
        record(ctx.label, clip(called, 64), false, false, 0, QStringLiteral("rejected — invalid params"));
        return;
    }
    const CataloguePtr cat = backend_->catalogue();
    const ToolMeta* meta = cat ? cat->resolve(called) : nullptr;
    if (!meta) {
        p.responses.append(wire::rpc_error(id, wire::kInvalidParams,
                                           QStringLiteral("Unknown tool: %1. tools/list shows the tools this server "
                                                          "has.")
                                               .arg(clip(called, 80))));
        record(ctx.label, clip(called, 64), false, false, 0, QStringLiteral("unknown tool"));
        return;
    }
    // The run token's tool policy, enforced here whatever the client was told.
    if (ctx.auth.run_token && run_scope_hides_category(ctx.auth.policy.scope, meta->category)) {
        p.responses.append(wire::rpc_result(
            id, wire::error_call_result(QStringLiteral("%1 is not available in this conversation: it would move the "
                                                       "user to another screen. Answer from what is already open, or "
                                                       "tell the user which screen to open.")
                                            .arg(called))));
        record(ctx.label, called, false, true, 0, QStringLiteral("denied — navigation not allowed for this run"));
        return;
    }
    // Concurrency: a burst from one client cannot starve the others, and the
    // whole server cannot be driven into unbounded work.
    const int mine = inflight_by_client_.value(ctx.scope);
    if (inflight_.load() >= limits_.max_inflight || mine >= limits_.max_inflight_per_client) {
        p.responses.append(wire::rpc_result(
            id, wire::error_call_result(QStringLiteral("Fincept Terminal is busy: %1 tool calls are already running%2. "
                                                       "Wait for one to finish, then retry.")
                                            .arg(mine >= limits_.max_inflight_per_client ? mine : inflight_.load())
                                            .arg(mine >= limits_.max_inflight_per_client ? QStringLiteral(" for you")
                                                                                         : QString()))));
        record(ctx.label, called, false, true, 0, QStringLiteral("refused — too many calls running"));
        return;
    }

    const profiles::ClientProfile& prof = profiles::profile(ctx.kind);
    auto* lc = new LiveCall;
    lc->call_id = next_call_id_++;
    lc->http_id = p.http_id;
    lc->slot = static_cast<int>(p.responses.size());
    lc->rpc_id = id;
    lc->tool = called;
    lc->client_label = ctx.label;
    lc->client_scope = ctx.scope;
    lc->kind = ctx.kind;
    lc->cancel = std::make_shared<std::atomic<bool>>(false);
    lc->started_ms = now_ms();
    lc->deadline_ms = lc->started_ms + std::clamp<qint64>(static_cast<qint64>(meta->timeout_ms) +
                                                              limits_.deadline_grace_ms,
                                                          limits_.deadline_floor_ms, limits_.deadline_ceiling_ms);
    const QJsonValue token =
        params.value(QStringLiteral("_meta")).toObject().value(QStringLiteral("progressToken"));
    if (token.isString() || token.isDouble())
        lc->progress_token = token;
    lc->running_key = ctx.scope + QLatin1Char('|') + wire::request_id_key(id);
    // A client that reuses an id while the first call still runs: the newer
    // call owns the key (a cancel names the request it just sent).
    running_by_request_.insert(lc->running_key, lc->call_id);
    calls_.insert(lc->call_id, lc);
    inflight_by_client_[ctx.scope] = mine + 1;
    inflight_.fetch_add(1);
    p.responses.append(QJsonValue()); // filled by complete_slot()
    ++p.outstanding;
    p.call_ids.append(lc->call_id);

    CallRequest req;
    req.call_id = lc->call_id;
    req.called_name = called;
    req.tool = *meta;
    req.args = profiles::restore_call_args(meta->input_schema, args_v.toObject(), prof.dialect);
    req.client_label = ctx.label;
    req.client_id = QString::fromLatin1(prof.actor_id);
    req.run_id = ctx.auth.run_token ? ctx.auth.policy.run_id : QString();
    req.kind = ctx.kind;
    req.paper_only = ctx.auth.run_token && ctx.auth.policy.paper_only;
    const int user_budget = result_budget_.load();
    req.result_budget_bytes = prof.result_budget_cap_bytes > 0 && (user_budget <= 0 || user_budget > prof.result_budget_cap_bytes)
                                  ? prof.result_budget_cap_bytes
                                  : user_budget;
    req.deadline_ms = lc->deadline_ms;
    req.cancel = lc->cancel;
    const std::shared_ptr<Link> link = link_;
    const quint64 call_id = lc->call_id;
    lc->keep_structured = meta->has_output_schema;
    if (!lc->progress_token.isUndefined()) {
        req.progress = [link, call_id](double progress, double total, const QString& message) {
            post_to_engine(link, [call_id, progress, total, message](McpServerEngine* engine) {
                engine->on_call_progress(call_id, progress, total, message);
            });
        };
    }
    auto fired = std::make_shared<std::atomic<bool>>(false);
    CallDone done = [link, call_id, fired](CallOutcome outcome) {
        if (fired->exchange(true))
            return; // only the first answer counts
        post_to_engine(link, [call_id, outcome](McpServerEngine* engine) { engine->on_call_done(call_id, outcome); });
    };
    try {
        backend_->call_tool(req, done);
    } catch (const std::exception& e) {
        CallOutcome o;
        o.payload = wire::error_call_result(
            QStringLiteral("Internal error while starting the tool: %1").arg(QString::fromUtf8(e.what())));
        o.summary = QStringLiteral("internal error");
        done(o);
    } catch (...) {
        CallOutcome o;
        o.payload = wire::error_call_result(QStringLiteral("Internal error while starting the tool."));
        o.summary = QStringLiteral("internal error");
        done(o);
    }
}

void McpServerEngine::cancel_request(const ClientCtx& ctx, const QJsonValue& request_id, const QString& reason) {
    if (!wire::valid_request_id(request_id))
        return;
    const quint64 call_id = running_by_request_.value(ctx.scope + QLatin1Char('|') + wire::request_id_key(request_id));
    LiveCall* lc = call_id ? calls_.value(call_id) : nullptr;
    if (!lc) {
        LOG_DEBUG(kTag, QStringLiteral("notifications/cancelled for a request that is not running"));
        return;
    }
    lc->cancel->store(true);
    // The tool is told to stop; the client gets its answer within the grace
    // even if the tool does not notice.
    lc->deadline_ms = std::min(lc->deadline_ms, now_ms() + kCancelGraceMs);
    LOG_INFO(kTag, logf("Client cancelled a tool call", {{"tool", lc->tool}, {"reason", clip(reason, 80)}}));
}

void McpServerEngine::record(const QString& client, const QString& tool, bool success, bool denied, qint64 ms,
                             const QString& summary) {
    ExternalCallRecord rec;
    rec.at = QDateTime::currentDateTime();
    rec.client = client;
    rec.tool = tool;
    rec.success = success;
    rec.denied = denied;
    rec.duration_ms = ms;
    rec.summary = summary;
    // Never log arguments or result text: tool errors can echo them.
    LOG_INFO(kTag, logf("MCP tool call finished", {{"tool", tool},
                                                   {"client", client},
                                                   {"outcome", success ? QStringLiteral("ok")
                                                               : denied ? QStringLiteral("denied")
                                                                        : QStringLiteral("failed")},
                                                   {"detail", clip(summary, 60)},
                                                   {"duration_ms", ms}}));
    emit call_finished(rec);
}

// ── Calls ────────────────────────────────────────────────────────────────────

void McpServerEngine::on_call_done(quint64 call_id, const CallOutcome& outcome) {
    LiveCall* lc = calls_.value(call_id);
    if (!lc)
        return; // answered already (deadline, cancel grace, shutdown)
    QJsonObject response;
    if (outcome.rpc_error) {
        response = QJsonObject{{QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                               {QStringLiteral("id"), lc->rpc_id},
                               {QStringLiteral("error"), outcome.payload}};
    } else {
        QJsonObject payload = outcome.payload;
        if (!payload.contains(QStringLiteral("content")))
            payload.insert(QStringLiteral("content"), QJsonArray{wire::text_block(QStringLiteral("OK"))});
        if (!profiles::profile(lc->kind).structured_content && !lc->keep_structured)
            payload.remove(QStringLiteral("structuredContent"));
        response = wire::rpc_result(lc->rpc_id, payload);
    }
    complete_slot(call_id, response, outcome.success, outcome.denied, outcome.summary);
}

void McpServerEngine::on_call_progress(quint64 call_id, double progress, double total, const QString& message) {
    LiveCall* lc = calls_.value(call_id);
    if (!lc || lc->progress_token.isUndefined() || lc->http_id == 0)
        return;
    Pending* p = pending_.value(lc->http_id);
    if (!p || !p->sse_allowed || p->sock.isNull())
        return;
    const qint64 now = now_ms();
    if (now - lc->last_progress_sent_ms < kProgressMinGapMs)
        return;
    // The spec requires progress to increase with every notification.
    double value = std::isfinite(progress) ? progress : 0.0;
    if (value <= lc->last_progress)
        value = lc->last_progress + 0.001;
    lc->last_progress = value;
    lc->last_progress_sent_ms = now;
    QJsonObject params{{QStringLiteral("progressToken"), lc->progress_token}, {QStringLiteral("progress"), value}};
    if (std::isfinite(total) && total > 0)
        params.insert(QStringLiteral("total"), std::max(total, value));
    if (!message.isEmpty())
        params.insert(QStringLiteral("message"), clip(message, 200));
    open_sse(*p);
    send_sse(*p, wire::sse_message(wire::compact_json(QJsonObject{
                     {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                     {QStringLiteral("method"), QStringLiteral("notifications/progress")},
                     {QStringLiteral("params"), params}})));
}

void McpServerEngine::complete_slot(quint64 call_id, const QJsonObject& response, bool success, bool denied,
                                    const QString& summary) {
    LiveCall* lc = calls_.take(call_id);
    if (!lc)
        return;
    const auto rk = running_by_request_.constFind(lc->running_key);
    if (rk != running_by_request_.cend() && rk.value() == call_id)
        running_by_request_.erase(rk);
    const int left = inflight_by_client_.value(lc->client_scope) - 1;
    if (left > 0)
        inflight_by_client_.insert(lc->client_scope, left);
    else
        inflight_by_client_.remove(lc->client_scope);
    inflight_.fetch_sub(1);
    record(lc->client_label, lc->tool, success, denied, now_ms() - lc->started_ms, summary);

    if (lc->http_id != 0) {
        if (Pending* p = pending_.value(lc->http_id)) {
            // A pending slot holds a null placeholder until its answer lands.
            if (lc->slot >= 0 && lc->slot < p->responses.size() && !p->responses.at(lc->slot).isObject()) {
                p->responses[lc->slot] = response;
                --p->outstanding;
            }
            if (p->outstanding <= 0)
                try_finish_pending(lc->http_id);
        }
    }
    delete lc;
}

void McpServerEngine::try_finish_pending(quint64 http_id) {
    Pending* p = pending_.take(http_id);
    if (!p)
        return;
    QTcpSocket* sock = p->sock.data();
    if (sock) {
        write_pending_final(*p);
        finish_connection_request(sock, p->close_after);
        // Serve whatever the client pipelined behind this request.
        QPointer<QTcpSocket> guard(sock);
        QMetaObject::invokeMethod(
            this,
            [this, guard]() {
                if (guard)
                    process(guard.data());
            },
            Qt::QueuedConnection);
    }
    delete p;
}

void McpServerEngine::write_pending_final(Pending& p) {
    QTcpSocket* sock = p.sock.data();
    if (!sock)
        return;
    QByteArray body;
    if (p.batch) {
        QJsonArray responses;
        for (const QJsonValue& r : std::as_const(p.responses))
            responses.append(r);
        body = wire::compact_json(responses);
    } else {
        body = wire::compact_json(p.responses.value(0).toObject());
    }
    if (p.sse_only && !p.sse_open)
        open_sse(p);
    if (p.sse_open) {
        send_sse(p, wire::sse_message(body));
        write_raw(sock, QByteArray(wire::kLastChunk));
        return;
    }
    Conn* c = conns_.value(sock);
    const bool head_only = c && c->parser.request().method == "HEAD";
    write_raw(sock, wire::build_response(200, QByteArrayLiteral("application/json"), body, p.extra_headers,
                                         p.close_after || (c && c->closing), limits_.keep_alive_hint_secs, head_only));
}

void McpServerEngine::open_sse(Pending& p) {
    if (p.sse_open || p.sock.isNull())
        return;
    p.sse_open = true;
    write_raw(p.sock.data(), wire::build_sse_head(p.extra_headers, p.close_after, limits_.keep_alive_hint_secs));
    send_sse(p, wire::sse_comment("fincept-terminal: working"));
}

void McpServerEngine::send_sse(Pending& p, const QByteArray& event) {
    if (p.sock.isNull())
        return;
    write_raw(p.sock.data(), wire::http_chunk(event));
    p.last_write_ms = now_ms();
}

void McpServerEngine::tick() {
    const qint64 now = now_ms();
    // 1. Deadlines: every call is answered by its deadline, whatever the tool
    //    does — a stuck handler cannot hold a client (or a slot) forever.
    QList<quint64> overdue;
    for (auto it = calls_.cbegin(); it != calls_.cend(); ++it) {
        if (now >= it.value()->deadline_ms)
            overdue.append(it.key());
    }
    for (quint64 id : overdue) {
        LiveCall* lc = calls_.value(id);
        if (!lc)
            continue;
        const bool cancelled = lc->cancel->load();
        lc->cancel->store(true);
        const qint64 secs = (now - lc->started_ms) / 1000;
        const QString text =
            cancelled ? QStringLiteral("Cancelled — the client gave up on this call.")
                      : QStringLiteral("Timed out: %1 did not finish within %2 s in Fincept Terminal, so the terminal "
                                       "stopped waiting for it. It may still complete on screen — check with "
                                       "ui_overview before retrying.")
                            .arg(lc->tool)
                            .arg(secs);
        complete_slot(id, wire::rpc_result(lc->rpc_id, wire::error_call_result(text)), false, false,
                      cancelled ? QStringLiteral("cancelled") : QStringLiteral("timed out"));
    }
    // 2. Streams: a call still running after the upgrade delay streams its
    //    answer; open streams get a keep-alive comment so no idle timer fires.
    const QList<quint64> open_posts = pending_.keys();
    for (quint64 http_id : open_posts) {
        Pending* p = pending_.value(http_id);
        if (!p || p->sock.isNull())
            continue;
        if (!p->sse_open) {
            if (p->sse_allowed && now - p->created_ms >= limits_.sse_upgrade_ms)
                open_sse(*p);
        } else if (now - p->last_write_ms >= limits_.sse_heartbeat_ms) {
            send_sse(*p, wire::sse_comment("keep-alive"));
        }
    }
    if (++ticks_ * limits_.tick_ms >= limits_.sweep_interval_ms) {
        ticks_ = 0;
        sweep();
    }
}

void McpServerEngine::sweep() {
    const qint64 now = now_ms();
    QList<QTcpSocket*> stale;
    QList<QTcpSocket*> slow;
    for (auto it = conns_.cbegin(); it != conns_.cend(); ++it) {
        const Conn* c = it.value();
        if (c->closing || c->busy)
            continue;
        if (c->request_started_ms > 0 && now - c->request_started_ms > limits_.request_timeout_ms)
            slow.append(it.key()); // trickling a request in (slowloris)
        else if (now - c->last_activity_ms > limits_.idle_timeout_ms)
            stale.append(it.key());
    }
    for (QTcpSocket* sock : std::as_const(slow))
        fail_and_close(sock, 408, QStringLiteral("Request timed out"));
    for (QTcpSocket* sock : std::as_const(stale)) {
        Conn* c = conns_.value(sock);
        if (!c || c->closing)
            continue;
        if (c->parser.head_done() || !c->buf.isEmpty()) {
            fail_and_close(sock, 408, QStringLiteral("Request timed out"));
        } else {
            c->closing = true;
            sock->disconnectFromHost();
        }
    }
    // Sessions nobody used for a long time (and with nothing running) are forgotten.
    QList<QString> expired;
    for (const Session* s : std::as_const(sessions_)) {
        if (now - s->last_seen_ms > limits_.session_idle_ms &&
            inflight_by_client_.value(QStringLiteral("s:") + s->id) == 0)
            expired.append(s->id);
    }
    for (const QString& id : std::as_const(expired))
        delete sessions_.take(id);
    sessions_count_.store(static_cast<int>(sessions_.size()));
}

} // namespace fincept::mcp::server
