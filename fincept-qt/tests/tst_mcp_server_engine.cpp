// tst_mcp_server_engine.cpp — protocol conformance of the terminal's MCP server
// engine (src/mcp/server/McpServerEngine.h) against a REAL loopback socket, with
// a fake backend instead of the application: handshake and version negotiation,
// every error path, sessions, the MCP 2026-07-28 fallback, auth and origin
// checks, cancellation that really cancels, deadlines for a stuck tool,
// concurrency limits, SSE streaming with progress, slow / idle clients,
// oversized and malformed input, shutdown with calls in flight, concurrent
// clients, client-profile shaping (golden), the wire parser on its own — and the
// stdio bridge script end to end when a Python interpreter is available
// (FINCEPT_TEST_PYTHON, else `python` on PATH; skipped otherwise).
//
// QtCore + QtNetwork. Nothing here starts the app, a harness or a model.

#include "mcp/GeminiSchema.h"
#include "mcp/server/ApprovalPolicy.h"
#include "mcp/server/CallApproval.h"
#include "mcp/server/McpClientProfile.h"
#include "mcp/server/McpServerEngine.h"
#include "mcp/server/McpServerSelfCheck.h"
#include "mcp/server/McpWire.h"

#include <QElapsedTimer>
#include <QFileInfo>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include <QThreadPool>
#include <QtTest>

#include <algorithm>
#include <atomic>
#include <memory>
#include <thread>
#include <vector>

#ifndef FINCEPT_SOURCE_DIR
#define FINCEPT_SOURCE_DIR "" // the stdio-bridge test is skipped without it
#endif

using namespace fincept::mcp;
using namespace fincept::mcp::server;
namespace wire = fincept::mcp::wire;
namespace profiles = fincept::mcp::profiles;

namespace {

const QByteArray kToken = "test-token-0123456789";

QJsonObject text_result(const QString& text, const QJsonObject& structured = {}) {
    QJsonObject r{{QStringLiteral("content"), QJsonArray{QJsonObject{{QStringLiteral("type"), QStringLiteral("text")},
                                                                     {QStringLiteral("text"), text}}}},
                  {QStringLiteral("isError"), false}};
    if (!structured.isEmpty())
        r.insert(QStringLiteral("structuredContent"), structured);
    return r;
}

// ── Fake backend ─────────────────────────────────────────────────────────────

class FakeBackend final : public McpServerBackend {
  public:
    FakeBackend() {
        pool.setMaxThreadCount(32);
        auto cat = std::make_shared<Catalogue>();
        cat->generation = 7;
        auto add = [&](const QString& name, const QString& category, int timeout_ms, const QJsonObject& schema,
                       bool output_schema = false) {
            ToolMeta m;
            m.internal = name;
            m.advertised = name;
            m.category = category;
            m.timeout_ms = timeout_ms;
            m.input_schema = schema;
            m.has_output_schema = output_schema;
            cat->tools.insert(name, m);
            cat->internal_to_advertised.insert(name, name);
            cat->entries.append(QJsonObject{{QStringLiteral("name"), name},
                                            {QStringLiteral("description"), QStringLiteral("Test tool ") + name},
                                            {QStringLiteral("inputSchema"), schema}});
        };
        const QJsonObject empty{{QStringLiteral("type"), QStringLiteral("object")},
                                {QStringLiteral("properties"), QJsonObject{}}};
        const QJsonObject optional_schema{
            {QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"),
             QJsonObject{{QStringLiteral("ref"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}},
                         {QStringLiteral("mode"),
                          QJsonObject{{QStringLiteral("type"), QStringLiteral("string")},
                                      {QStringLiteral("enum"), QJsonArray{QStringLiteral("a"), QStringLiteral("b")}},
                                      {QStringLiteral("default"), QStringLiteral("a")}}},
                         {QStringLiteral("ms"), QJsonObject{{QStringLiteral("type"), QStringLiteral("integer")},
                                                            {QStringLiteral("minimum"), 0}}},
                         {QStringLiteral("value"), QJsonObject{{QStringLiteral("description"), QStringLiteral("any")}}},
                         {QStringLiteral("params"), QJsonObject{{QStringLiteral("type"), QStringLiteral("object")}}},
                         {QStringLiteral("steps"),
                          QJsonObject{{QStringLiteral("type"), QStringLiteral("array")},
                                      {QStringLiteral("items"),
                                       QJsonObject{{QStringLiteral("type"), QStringLiteral("object")},
                                                   {QStringLiteral("properties"),
                                                    QJsonObject{{QStringLiteral("action"),
                                                                 QJsonObject{{QStringLiteral("type"),
                                                                              QStringLiteral("string")}}},
                                                                {QStringLiteral("text"),
                                                                 QJsonObject{{QStringLiteral("type"),
                                                                              QStringLiteral("string")}}}}},
                                                   {QStringLiteral("required"),
                                                    QJsonArray{QStringLiteral("action")}}}}}}}},
            {QStringLiteral("required"), QJsonArray{QStringLiteral("ref")}}};
        add(QStringLiteral("echo"), QStringLiteral("ui-read"), 5000, optional_schema);
        add(QStringLiteral("slow"), QStringLiteral("ui-control"), 20000, empty);
        add(QStringLiteral("hang"), QStringLiteral("analytics"), 300, empty);
        add(QStringLiteral("progress"), QStringLiteral("analytics"), 10000, empty);
        add(QStringLiteral("big"), QStringLiteral("ui-read"), 5000, empty);
        add(QStringLiteral("nav"), QStringLiteral("navigation"), 5000, empty);
        add(QStringLiteral("structured"), QStringLiteral("ui-read"), 5000, empty, /*output_schema=*/true);
        add(QStringLiteral("ui_overview"), QStringLiteral("ui-read"), 5000, empty); // what the self-check calls
        add(QStringLiteral("whoami"), QStringLiteral("ui-read"), 5000, empty);      // the caller ids a handler sees
        catalogue_ = cat;
    }
    ~FakeBackend() override {
        stop.store(true);
        pool.waitForDone(5000);
    }

    AuthInfo authenticate(const QByteArray& bearer) const override {
        {
            QMutexLocker lock(&bearer_mutex);
            bearers << bearer; // every credential that reached the server
        }
        AuthInfo a;
        if (bearer == kToken) {
            a.ok = true;
            a.fingerprint = "main";
            return a;
        }
        if (const auto p = runs.check(QString::fromLatin1(bearer), QDateTime::currentMSecsSinceEpoch())) {
            a.ok = true;
            a.run_token = true;
            a.policy = *p;
            a.fingerprint = "run:" + bearer.right(6);
        }
        return a;
    }
    CataloguePtr catalogue() override { return catalogue_; }
    QJsonObject server_info() const override {
        QMutexLocker lock(&bearer_mutex);
        return QJsonObject{{QStringLiteral("name"), QStringLiteral("fincept-terminal")},
                           {QStringLiteral("version"), QStringLiteral("test")},
                           {QStringLiteral("edition"), edition}};
    }
    QString instructions() const override { return QStringLiteral("Test server."); }
    bool content_request(const QString& method, const QJsonObject&, QJsonObject* result, int*, QString*) override {
        if (method == QLatin1String("prompts/list")) {
            *result = QJsonObject{{QStringLiteral("prompts"), QJsonArray{}}};
            return true;
        }
        if (method == QLatin1String("explode"))
            throw std::runtime_error("backend blew up");
        return false;
    }
    void call_tool(const CallRequest& req, CallDone done) override {
        {
            QMutexLocker lock(&mutex);
            labels << req.client_label;
            kinds << req.kind;
            budgets << req.result_budget_bytes;
            args << req.args;
        }
        started.fetch_add(1);
        const QString tool = req.tool.internal;
        if (tool == QLatin1String("hang"))
            return; // never answers: the engine's deadline must
        if (tool == QLatin1String("whoami")) {
            // As the terminal backend does (McpHttpServer_Tools.cpp): the stable ids are
            // in scope for the synchronous part of the dispatch, where handlers read them.
            ScopedCallClient scope(req.client_id, req.run_id);
            const QString seen = ScopedCallClient::client_id() + QLatin1Char('|') + ScopedCallClient::run_id();
            pool.start([done, seen]() {
                CallOutcome o;
                o.success = true;
                o.payload = text_result(seen);
                done(o);
            });
            return;
        }
        pool.start([this, req, done, tool]() {
            if (tool == QLatin1String("echo")) {
                CallOutcome o;
                o.success = true;
                o.summary = QStringLiteral("ok");
                o.payload = text_result(QString::fromUtf8(QJsonDocument(req.args).toJson(QJsonDocument::Compact)),
                                        QJsonObject{{QStringLiteral("echo"), req.args}});
                done(o);
                return;
            }
            if (tool == QLatin1String("slow")) {
                const int ms = req.args.value(QStringLiteral("ms")).toInt(3000);
                QElapsedTimer t;
                t.start();
                while (t.elapsed() < ms && !stop.load()) {
                    if (req.cancel->load()) {
                        cancels.fetch_add(1);
                        CallOutcome o;
                        o.payload = wire::error_call_result(QStringLiteral("Cancelled — the client gave up on this call."));
                        o.summary = QStringLiteral("cancelled");
                        done(o);
                        return;
                    }
                    QThread::msleep(10);
                }
                CallOutcome o;
                o.success = true;
                o.payload = text_result(QStringLiteral("slow done"));
                done(o);
                return;
            }
            if (tool == QLatin1String("progress")) {
                for (int i = 1; i <= 5 && !stop.load(); ++i) {
                    if (req.progress)
                        req.progress(i / 5.0, 1.0, QStringLiteral("step %1").arg(i));
                    QThread::msleep(250);
                }
                CallOutcome o;
                o.success = true;
                o.payload = text_result(QStringLiteral("progress done"));
                done(o);
                return;
            }
            if (tool == QLatin1String("big")) {
                CallOutcome o;
                o.success = true;
                o.payload = text_result(QString(req.args.value(QStringLiteral("n")).toInt(1000), QLatin1Char('x')));
                done(o);
                return;
            }
            CallOutcome o;
            o.success = true;
            o.payload = text_result(tool, QJsonObject{{QStringLiteral("tool"), tool}});
            done(o);
        });
    }

    mutable McpRunTokenTable runs;
    mutable QMutex bearer_mutex;
    mutable QList<QByteArray> bearers;
    QString edition = QStringLiteral("community"); // guarded by bearer_mutex
    void set_edition(const QString& e) {
        QMutexLocker lock(&bearer_mutex);
        edition = e;
    }
    bool saw_bearer(const QByteArray& b) const {
        QMutexLocker lock(&bearer_mutex);
        return bearers.contains(b);
    }
    int bearer_count() const {
        QMutexLocker lock(&bearer_mutex);
        return static_cast<int>(bearers.size());
    }
    QThreadPool pool;
    std::atomic<bool> stop{false};
    std::atomic<int> started{0};
    std::atomic<int> cancels{0};
    QMutex mutex;
    QStringList labels;
    QList<profiles::ClientKind> kinds;
    QList<int> budgets;
    QList<QJsonObject> args;

  private:
    CataloguePtr catalogue_;
};

// ── Loopback HTTP client ─────────────────────────────────────────────────────

struct Reply {
    bool ok = false;
    int status = 0;
    QHash<QByteArray, QByteArray> headers;
    QByteArray body;    // de-chunked
    QByteArray raw_body;
    QJsonDocument json() const { return QJsonDocument::fromJson(body); }
    bool sse() const { return headers.value("content-type").startsWith("text/event-stream"); }
    /// JSON-RPC messages in an SSE body, in order.
    QList<QJsonObject> events() const {
        QList<QJsonObject> out;
        for (const QByteArray& line : body.split('\n')) {
            if (line.startsWith("data: "))
                out << QJsonDocument::fromJson(line.mid(6)).object();
        }
        return out;
    }
    /// The response: the JSON body, or the last SSE message with an id.
    QJsonObject message() const {
        if (!sse())
            return json().object();
        QJsonObject last;
        for (const QJsonObject& e : events()) {
            if (e.contains(QStringLiteral("id")))
                last = e;
        }
        return last;
    }
};

/// Try to take one complete response off the front of `buf`. Returns false
/// (and leaves `buf` alone) while it is incomplete; bytes of a pipelined next
/// response stay in `buf`.
bool take_reply(QByteArray& buf, Reply& r) {
    for (;;) {
        const qsizetype end = buf.indexOf("\r\n\r\n");
        if (end < 0)
            return false;
        if (!buf.startsWith("HTTP/1.1 100")) // interim 100-continue
            break;
        buf.remove(0, end + 4);
    }
    const qsizetype end = buf.indexOf("\r\n\r\n");
    Reply out;
    const QList<QByteArray> lines = buf.left(end).split('\n');
    out.status = lines.value(0).trimmed().split(' ').value(1).toInt();
    for (qsizetype i = 1; i < lines.size(); ++i) {
        const QByteArray line = lines.at(i).trimmed();
        const qsizetype colon = line.indexOf(':');
        if (colon > 0)
            out.headers.insert(line.left(colon).trimmed().toLower(), line.mid(colon + 1).trimmed());
    }
    const qsizetype body_start = end + 4;
    qsizetype consumed = 0;
    if (out.headers.value("transfer-encoding") == "chunked") {
        QByteArray decoded;
        qsizetype pos = body_start;
        for (;;) {
            const qsizetype eol = buf.indexOf("\r\n", pos);
            if (eol < 0)
                return false;
            bool ok = false;
            const qint64 n = buf.mid(pos, eol - pos).trimmed().toLongLong(&ok, 16);
            if (!ok)
                return false;
            if (n == 0) {
                if (buf.size() < eol + 4)
                    return false;
                consumed = eol + 4; // "0\r\n\r\n"
                break;
            }
            if (buf.size() < eol + 2 + n + 2)
                return false;
            decoded += buf.mid(eol + 2, n);
            pos = eol + 2 + n + 2;
        }
        out.body = decoded;
    } else {
        const qint64 n = out.headers.value("content-length", "0").toLongLong();
        if (buf.size() - body_start < n)
            return false;
        out.body = buf.mid(body_start, n);
        consumed = body_start + n;
    }
    out.raw_body = buf.mid(body_start, consumed - body_start);
    out.ok = true;
    buf.remove(0, consumed);
    r = out;
    return true;
}

/// Read one response from `sock` (Content-Length or chunked). Blocking.
/// `carry` keeps bytes of pipelined responses between calls.
Reply read_reply(QTcpSocket& sock, QByteArray& carry, int timeout_ms = 10000) {
    Reply r;
    QElapsedTimer t;
    t.start();
    while (!take_reply(carry, r)) {
        if (t.elapsed() > timeout_ms)
            return {};
        if (!sock.waitForReadyRead(50)) {
            if (sock.state() != QAbstractSocket::ConnectedState && sock.bytesAvailable() == 0)
                return {};
            continue;
        }
        carry += sock.readAll();
    }
    return r;
}

Reply read_reply(QTcpSocket& sock, int timeout_ms = 10000) {
    QByteArray carry;
    return read_reply(sock, carry, timeout_ms);
}

QByteArray request_bytes(quint16 port, const QByteArray& method, const QByteArray& path,
                         const QList<QPair<QByteArray, QByteArray>>& headers, const QByteArray& body,
                         bool keep_alive = false) {
    QByteArray raw = method + ' ' + path + " HTTP/1.1\r\n";
    bool has_host = false;
    for (const auto& h : headers) {
        has_host = has_host || h.first.toLower() == "host";
        raw += h.first + ": " + h.second + "\r\n";
    }
    if (!has_host)
        raw += "Host: 127.0.0.1:" + QByteArray::number(port) + "\r\n";
    if (!body.isEmpty() || method == "POST")
        raw += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
    raw += keep_alive ? "Connection: keep-alive\r\n" : "Connection: close\r\n";
    raw += "\r\n" + body;
    return raw;
}

QList<QPair<QByteArray, QByteArray>> mcp_headers(const QByteArray& token = kToken, const QByteArray& session = {},
                                                const QByteArray& accept = "application/json, text/event-stream") {
    QList<QPair<QByteArray, QByteArray>> h{{"Authorization", "Bearer " + token},
                                           {"Content-Type", "application/json"},
                                           {"Accept", accept}};
    if (!session.isEmpty())
        h.append({"Mcp-Session-Id", session});
    return h;
}

Reply http(quint16 port, const QByteArray& method, const QByteArray& path,
           const QList<QPair<QByteArray, QByteArray>>& headers, const QByteArray& body = {}, int timeout_ms = 10000) {
    QTcpSocket sock;
    sock.connectToHost(QHostAddress::LocalHost, port);
    if (!sock.waitForConnected(3000))
        return {};
    sock.write(request_bytes(port, method, path, headers, body));
    sock.flush();
    return read_reply(sock, timeout_ms);
}

QByteArray rpc(int id, const QString& method, const QJsonObject& params = {}) {
    QJsonObject m{{QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                  {QStringLiteral("id"), id},
                  {QStringLiteral("method"), method}};
    if (!params.isEmpty())
        m.insert(QStringLiteral("params"), params);
    return QJsonDocument(m).toJson(QJsonDocument::Compact);
}

QByteArray note(const QString& method, const QJsonObject& params = {}) {
    QJsonObject m{{QStringLiteral("jsonrpc"), QStringLiteral("2.0")}, {QStringLiteral("method"), method}};
    if (!params.isEmpty())
        m.insert(QStringLiteral("params"), params);
    return QJsonDocument(m).toJson(QJsonDocument::Compact);
}

QJsonObject init_params(const QString& client, const QString& version = QStringLiteral("2025-06-18")) {
    return QJsonObject{{QStringLiteral("protocolVersion"), version},
                       {QStringLiteral("capabilities"), QJsonObject{}},
                       {QStringLiteral("clientInfo"),
                        QJsonObject{{QStringLiteral("name"), client}, {QStringLiteral("version"), QStringLiteral("1")}}}};
}

QJsonObject call_params(const QString& tool, const QJsonObject& arguments = {}, const QJsonValue& progress = {}) {
    QJsonObject p{{QStringLiteral("name"), tool}, {QStringLiteral("arguments"), arguments}};
    if (!progress.isUndefined())
        p.insert(QStringLiteral("_meta"), QJsonObject{{QStringLiteral("progressToken"), progress}});
    return p;
}

int error_code(const QJsonObject& m) {
    return m.value(QStringLiteral("error")).toObject().value(QStringLiteral("code")).toInt();
}

QString first_text(const QJsonObject& m) {
    return m.value(QStringLiteral("result"))
        .toObject()
        .value(QStringLiteral("content"))
        .toArray()
        .at(0)
        .toObject()
        .value(QStringLiteral("text"))
        .toString();
}

bool is_error_result(const QJsonObject& m) {
    return m.value(QStringLiteral("result")).toObject().value(QStringLiteral("isError")).toBool();
}

// ── Engine fixture (the engine on its own thread, as in the app) ─────────────

struct Server {
    std::shared_ptr<FakeBackend> backend = std::make_shared<FakeBackend>();
    QThread thread;
    McpServerEngine* engine = nullptr;
    quint16 port = 0;
    QMutex records_mutex;
    QList<ExternalCallRecord> records;

    explicit Server(EngineLimits limits = test_limits(), quint16 want_port = 0, int range = 0) {
        engine = new McpServerEngine(backend, limits);
        engine->moveToThread(&thread);
        QObject::connect(engine, &McpServerEngine::call_finished, engine,
                         [this](const ExternalCallRecord& r) {
                             QMutexLocker lock(&records_mutex);
                             records << r;
                         },
                         Qt::DirectConnection);
        thread.start();
        bool ok = false;
        QString err;
        McpServerEngine* e = engine;
        QMetaObject::invokeMethod(
            e, [e, want_port, range, &ok, &err]() { ok = e->listen(want_port, range, &err); },
            Qt::BlockingQueuedConnection);
        port = ok ? e->port() : 0;
    }
    ~Server() { stop(); }
    void shutdown_engine(const QString& reason) {
        if (!engine)
            return;
        McpServerEngine* e = engine;
        QMetaObject::invokeMethod(e, [e, reason]() { e->shutdown(reason); }, Qt::BlockingQueuedConnection);
    }
    void stop() {
        if (!engine)
            return;
        McpServerEngine* e = engine;
        engine = nullptr;
        QMetaObject::invokeMethod(
            e,
            [e]() {
                e->shutdown(QStringLiteral("test over"));
                e->deleteLater(); // deleted on its thread as the thread finishes (as McpHttpServer does)
            },
            Qt::BlockingQueuedConnection);
        thread.quit();
        thread.wait(5000);
        backend->stop.store(true);
    }
    static EngineLimits test_limits() {
        EngineLimits l;
        l.idle_timeout_ms = 1500;
        l.request_timeout_ms = 1200;
        l.session_idle_ms = 60000;
        l.max_inflight = 6;
        l.max_inflight_per_client = 3;
        l.deadline_grace_ms = 400;
        l.deadline_floor_ms = 200;
        l.sse_upgrade_ms = 400;
        l.sse_heartbeat_ms = 200;
        l.tick_ms = 50;
        l.sweep_interval_ms = 200;
        l.tools_page_size = 4;
        return l;
    }
    /// initialize as `client`; returns the session id.
    QByteArray initialize(const QString& client, const QByteArray& token = kToken) {
        const Reply r = http(port, "POST", "/mcp", mcp_headers(token), rpc(1, QStringLiteral("initialize"), init_params(client)));
        return r.headers.value("mcp-session-id");
    }
};

} // namespace

class TestMcpServerEngine : public QObject {
    Q_OBJECT
  private slots:
    void initTestCase();

    // wire
    void wire_parser_pipelined_and_chunked();
    void wire_parser_rejects_bad_framing();
    void wire_accept_ids_versions();

    // protocol
    void handshake_negotiates_versions();
    void notifications_get_202_and_errors_have_codes();
    void ids_are_echoed_exactly();
    void batches();
    void modern_protocol_probe_falls_back();
    void auth_origin_and_host();
    void http_methods_and_sessions();
    void tools_list_pages_and_run_scope();
    void tools_call_shapes_per_profile();
    void cancellation_really_cancels();
    void caller_ids_reach_the_tool_scope();
    void approval_headings_and_grant_matrix();
    void timed_spend_grant_never_covers_an_order();
    void deadline_answers_a_stuck_tool();
    void concurrency_limits();
    void progress_streams_as_sse();
    void long_call_upgrades_to_sse_with_heartbeats();
    void json_only_client_gets_json();
    void disconnect_cancels_running_call();
    void idle_and_slow_clients_are_closed();
    void oversized_and_malformed_input();
    void pipelining_with_backpressure();
    void concurrent_clients();
    void session_expiry();
    void shutdown_answers_calls_in_flight();
    void latency_of_a_trivial_call();

    // profiles
    void profile_detection();
    void strict_dialect_golden();
    void gemini_dialect_golden();
    void profile_budget_caps();

    // stdio bridge
    void stdio_bridge_end_to_end();
    void stdio_bridge_refuses_another_edition();

    // edition co-existence / self-check
    void health_and_server_info_report_the_edition();
    void forbidden_ports_are_never_bound();
    void self_check_passes_every_profile();
    void self_check_refuses_another_edition();
};

void TestMcpServerEngine::initTestCase() {
    qRegisterMetaType<ExternalCallRecord>();
}

// ── wire ─────────────────────────────────────────────────────────────────────

void TestMcpServerEngine::wire_parser_pipelined_and_chunked() {
    wire::HttpRequestParser p;
    QByteArray buf = "\r\nPOST /mcp HTTP/1.1\r\nHost: x\r\nContent-Length: 3\r\n\r\nabcGET /health HTTP/1.1\r\nHost: x\r\n\r\n";
    QCOMPARE(p.feed(buf), wire::HttpRequestParser::State::HeadReady);
    QCOMPARE(p.feed(buf), wire::HttpRequestParser::State::Complete);
    QCOMPARE(p.request().body, QByteArray("abc"));
    QCOMPARE(p.request().method, QByteArray("POST"));
    p.reset();
    QCOMPARE(p.feed(buf), wire::HttpRequestParser::State::HeadReady);
    QCOMPARE(p.feed(buf), wire::HttpRequestParser::State::Complete);
    QCOMPARE(p.request().target, QByteArray("/health"));
    QVERIFY(buf.isEmpty());

    // Chunked, fed one byte at a time.
    p.reset();
    const QByteArray chunked =
        "POST /mcp HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\n\r\n4;ext=1\r\nWiki\r\n5\r\npedia\r\n0\r\nX-T: 1\r\n\r\n";
    QByteArray in;
    auto st = wire::HttpRequestParser::State::NeedMore;
    for (char ch : chunked) {
        in.append(ch);
        st = p.feed(in);
        if (st == wire::HttpRequestParser::State::HeadReady)
            st = p.feed(in);
        QVERIFY(st != wire::HttpRequestParser::State::Error);
    }
    QCOMPARE(st, wire::HttpRequestParser::State::Complete);
    QCOMPARE(p.request().body, QByteArray("Wikipedia"));
    QCOMPARE(wire::request_path("http://127.0.0.1:1/mcp/?x=1#f"), QByteArray("/mcp"));
}

void TestMcpServerEngine::wire_parser_rejects_bad_framing() {
    struct Case {
        QByteArray raw;
        int status;
    };
    const QList<Case> cases = {
        {"POST /mcp HTTP/1.1\r\nContent-Length: 5\r\nTransfer-Encoding: chunked\r\n\r\n", 400},
        {"POST /mcp HTTP/1.1\r\nContent-Length: 5\r\nContent-Length: 6\r\n\r\n", 400},
        {"POST /mcp HTTP/1.1\r\nContent-Length: -1\r\n\r\n", 400},
        {"POST /mcp HTTP/1.1\r\nContent-Length: 999999999999\r\n\r\n", 413},
        {"POST /mcp HTTP/2.0\r\n\r\n", 505},
        {"post /mcp HTTP/1.1\r\n\r\n", 400},
        {"POST /mcp HTTP/1.1\r\n folded\r\n\r\n", 400},
        {"POST /mcp HTTP/1.1\r\nTransfer-Encoding: gzip\r\n\r\n", 501},
        {"POST /mcp HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\nzz\r\n", 400},
        {"GET /x HTTP/1.1\r\nA: " + QByteArray(70 * 1024, 'a') + "\r\n\r\n", 431},
    };
    for (const Case& c : cases) {
        wire::HttpRequestParser p;
        QByteArray buf = c.raw;
        auto st = p.feed(buf);
        if (st == wire::HttpRequestParser::State::HeadReady)
            st = p.feed(buf);
        QVERIFY2(st == wire::HttpRequestParser::State::Error, c.raw.left(60).constData());
        QCOMPARE(p.error_status(), c.status);
    }
    // A header block that never ends is refused once it passes the limit.
    wire::HttpRequestParser p;
    QByteArray buf = "GET / HTTP/1.1\r\n" + QByteArray(wire::kMaxHeaderBytes + 10, 'a');
    QCOMPARE(p.feed(buf), wire::HttpRequestParser::State::Error);
    QCOMPARE(p.error_status(), 431);
}

void TestMcpServerEngine::wire_accept_ids_versions() {
    QVERIFY(wire::accepts("", "application/json"));
    QVERIFY(wire::accepts("application/json, text/event-stream", "text/event-stream"));
    QVERIFY(wire::accepts("*/*", "application/json"));
    QVERIFY(wire::accepts("application/*;q=0.5", "application/json"));
    QVERIFY(!wire::accepts("text/event-stream", "application/json"));
    QVERIFY(!wire::accepts("application/json;q=0", "application/json"));

    QVERIFY(wire::valid_request_id(QJsonValue(QStringLiteral("a"))));
    QVERIFY(wire::valid_request_id(QJsonValue(7)));
    QVERIFY(wire::valid_request_id(QJsonValue(1.5)));
    QVERIFY(!wire::valid_request_id(QJsonValue()));
    QVERIFY(!wire::valid_request_id(QJsonValue(true)));
    QVERIFY(!wire::valid_request_id(QJsonValue(QJsonObject{})));
    QCOMPARE(wire::request_id_key(QJsonValue(1)), wire::request_id_key(QJsonValue(1.0)));
    QVERIFY(wire::request_id_key(QJsonValue(1)) != wire::request_id_key(QJsonValue(QStringLiteral("1"))));

    QCOMPARE(wire::negotiate_protocol_version(QStringLiteral("2025-03-26")), QStringLiteral("2025-03-26"));
    QCOMPARE(wire::negotiate_protocol_version(QStringLiteral("2026-07-28")), QStringLiteral("2025-11-25"));
    QCOMPARE(wire::unsupported_protocol_requested("2026-07-28", {}), QStringLiteral("2026-07-28"));
    QVERIFY(wire::unsupported_protocol_requested("2025-06-18", {}).isEmpty());
    QVERIFY(wire::unsupported_protocol_requested("", {}).isEmpty());
    const QJsonObject modern{{QStringLiteral("_meta"),
                              QJsonObject{{QStringLiteral("io.modelcontextprotocol/protocolVersion"),
                                           QStringLiteral("2026-07-28")}}}};
    QCOMPARE(wire::unsupported_protocol_requested("", modern), QStringLiteral("2026-07-28"));

    QCOMPARE(wire::sse_message("{\"a\":1}"), QByteArray("event: message\ndata: {\"a\":1}\n\n"));
    QCOMPARE(wire::http_chunk("abc"), QByteArray("3\r\nabc\r\n"));
    QVERIFY(wire::valid_session_id("abc-DEF_123"));
    QVERIFY(!wire::valid_session_id("a b"));
    QVERIFY(!wire::valid_session_id(QByteArray(129, 'a')));
}

// ── protocol ─────────────────────────────────────────────────────────────────

void TestMcpServerEngine::handshake_negotiates_versions() {
    Server s;
    QVERIFY(s.port != 0);
    for (const QString& v : {QStringLiteral("2025-11-25"), QStringLiteral("2025-06-18"), QStringLiteral("2025-03-26"),
                             QStringLiteral("2024-11-05"), QStringLiteral("2099-01-01"), QString()}) {
        const Reply r = http(s.port, "POST", "/mcp", mcp_headers(),
                             rpc(1, QStringLiteral("initialize"), init_params(QStringLiteral("claude-code"), v)));
        QCOMPARE(r.status, 200);
        const QJsonObject res = r.json().object().value(QStringLiteral("result")).toObject();
        const QString expect = wire::supported_protocol_versions().contains(v) ? v : QStringLiteral("2025-11-25");
        QCOMPARE(res.value(QStringLiteral("protocolVersion")).toString(), expect);
        QVERIFY(res.value(QStringLiteral("capabilities")).toObject().contains(QStringLiteral("tools")));
        QCOMPARE(res.value(QStringLiteral("serverInfo")).toObject().value(QStringLiteral("name")).toString(),
                 QStringLiteral("fincept-terminal"));
        QCOMPARE(res.value(QStringLiteral("instructions")).toString(), QStringLiteral("Test server."));
        QVERIFY(wire::valid_session_id(r.headers.value("mcp-session-id")));
        QCOMPARE(r.headers.value("content-type"), QByteArray("application/json"));
    }
}

void TestMcpServerEngine::notifications_get_202_and_errors_have_codes() {
    Server s;
    const QByteArray sid = s.initialize(QStringLiteral("codex-mcp-client"));
    Reply r = http(s.port, "POST", "/mcp", mcp_headers(kToken, sid), note(QStringLiteral("notifications/initialized")));
    QCOMPARE(r.status, 202);
    QVERIFY(r.body.isEmpty());
    r = http(s.port, "POST", "/mcp", mcp_headers(kToken, sid), rpc(2, QStringLiteral("no/such/method")));
    QCOMPARE(error_code(r.json().object()), -32601);
    r = http(s.port, "POST", "/mcp", mcp_headers(kToken, sid), "{not json");
    QCOMPARE(r.status, 400);
    QCOMPARE(error_code(r.json().object()), -32700);
    QVERIFY(r.json().object().value(QStringLiteral("id")).isNull());
    r = http(s.port, "POST", "/mcp", mcp_headers(kToken, sid), "42");
    QCOMPARE(error_code(r.json().object()), -32700);
    r = http(s.port, "POST", "/mcp", mcp_headers(kToken, sid), "{\"jsonrpc\":\"1.0\",\"id\":3,\"method\":\"ping\"}");
    QCOMPARE(error_code(r.json().object()), -32600);
    r = http(s.port, "POST", "/mcp", mcp_headers(kToken, sid), "{\"jsonrpc\":\"2.0\",\"id\":null,\"method\":\"ping\"}");
    QCOMPARE(error_code(r.json().object()), -32600);
    r = http(s.port, "POST", "/mcp", mcp_headers(kToken, sid), "{\"jsonrpc\":\"2.0\",\"id\":4,\"method\":\"ping\",\"params\":[1]}");
    QCOMPARE(error_code(r.json().object()), -32602);
    r = http(s.port, "POST", "/mcp", mcp_headers(kToken, sid), rpc(5, QStringLiteral("tools/call"), {{QStringLiteral("name"), 5}}));
    QCOMPARE(error_code(r.json().object()), -32602);
    r = http(s.port, "POST", "/mcp", mcp_headers(kToken, sid),
             rpc(6, QStringLiteral("tools/call"), {{QStringLiteral("name"), QStringLiteral("echo")}, {QStringLiteral("arguments"), 3}}));
    QCOMPARE(error_code(r.json().object()), -32602);
    r = http(s.port, "POST", "/mcp", mcp_headers(kToken, sid), rpc(7, QStringLiteral("tools/call"), call_params(QStringLiteral("nope"))));
    QCOMPARE(error_code(r.json().object()), -32602);
    r = http(s.port, "POST", "/mcp", mcp_headers(kToken, sid), rpc(8, QStringLiteral("explode")));
    QCOMPARE(error_code(r.json().object()), -32603); // a throwing backend never takes the server down
    r = http(s.port, "POST", "/mcp", mcp_headers(kToken, sid), rpc(9, QStringLiteral("prompts/list")));
    QVERIFY(r.json().object().value(QStringLiteral("result")).toObject().contains(QStringLiteral("prompts")));
    // A client response (to a server request we never sent) is accepted silently.
    r = http(s.port, "POST", "/mcp", mcp_headers(kToken, sid), "{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":{}}");
    QCOMPARE(r.status, 202);
}

void TestMcpServerEngine::ids_are_echoed_exactly() {
    Server s;
    for (const QByteArray& id : {QByteArray("\"abc\""), QByteArray("42"), QByteArray("9007199254740993"), QByteArray("-7"),
                                 QByteArray("1.5"), QByteArray("\"\""), QByteArray("0")}) {
        const QByteArray body = "{\"jsonrpc\":\"2.0\",\"id\":" + id + ",\"method\":\"ping\"}";
        const Reply r = http(s.port, "POST", "/mcp", mcp_headers(), body);
        QCOMPARE(r.status, 200);
        QVERIFY2(r.body.contains("\"id\":" + id), r.body.constData());
    }
}

void TestMcpServerEngine::batches() {
    Server s;
    const QByteArray batch = "[" + rpc(1, QStringLiteral("ping")) + "," + note(QStringLiteral("notifications/initialized")) +
                             "," + rpc(2, QStringLiteral("tools/call"), call_params(QStringLiteral("echo"), {{QStringLiteral("ref"), QStringLiteral("x")}})) +
                             "," + rpc(3, QStringLiteral("tools/list")) + "]";
    Reply r = http(s.port, "POST", "/mcp", mcp_headers(), batch);
    QCOMPARE(r.status, 200);
    const QJsonArray arr = r.json().array();
    QCOMPARE(arr.size(), 3);
    QCOMPARE(arr.at(0).toObject().value(QStringLiteral("id")).toInt(), 1);
    QCOMPARE(arr.at(1).toObject().value(QStringLiteral("id")).toInt(), 2);
    QVERIFY(arr.at(1).toObject().contains(QStringLiteral("result")));
    QCOMPARE(arr.at(2).toObject().value(QStringLiteral("id")).toInt(), 3);
    r = http(s.port, "POST", "/mcp", mcp_headers(), "[]");
    QCOMPARE(r.status, 400);
    QCOMPARE(error_code(r.json().object()), -32600);
    r = http(s.port, "POST", "/mcp", mcp_headers(), "[" + note(QStringLiteral("notifications/initialized")) + "]");
    QCOMPARE(r.status, 202);
    r = http(s.port, "POST", "/mcp", mcp_headers(), "[1,\"x\"]");
    QCOMPARE(r.json().array().size(), 2);
    QCOMPARE(error_code(r.json().array().at(0).toObject()), -32600);
}

void TestMcpServerEngine::modern_protocol_probe_falls_back() {
    Server s;
    auto h = mcp_headers();
    h.append({"MCP-Protocol-Version", QByteArray("2026-07-28")});
    Reply r = http(s.port, "POST", "/mcp", h, rpc(1, QStringLiteral("server/discover")));
    QCOMPARE(r.status, 400);
    QVERIFY(!r.json().object().contains(QStringLiteral("jsonrpc"))); // no modern error body
    r = http(s.port, "POST", "/mcp", h, rpc(2, QStringLiteral("tools/list")));
    QCOMPARE(r.status, 400);
    r = http(s.port, "POST", "/mcp", mcp_headers(), rpc(3, QStringLiteral("server/discover")));
    QCOMPARE(r.status, 400);
    const QJsonObject modern_meta{{QStringLiteral("_meta"),
                                   QJsonObject{{QStringLiteral("io.modelcontextprotocol/protocolVersion"),
                                                QStringLiteral("2026-07-28")}}}};
    r = http(s.port, "POST", "/mcp", mcp_headers(), rpc(4, QStringLiteral("tools/list"), modern_meta));
    QCOMPARE(r.status, 400);
    // …and the fallback the client then makes works, whatever the header says.
    r = http(s.port, "POST", "/mcp", h, rpc(5, QStringLiteral("initialize"), init_params(QStringLiteral("claude-code"), QStringLiteral("2025-11-25"))));
    QCOMPARE(r.status, 200);
    auto legacy = mcp_headers(kToken, r.headers.value("mcp-session-id"));
    legacy.append({"MCP-Protocol-Version", QByteArray("2025-11-25")});
    r = http(s.port, "POST", "/mcp", legacy, rpc(6, QStringLiteral("tools/list")));
    QCOMPARE(r.status, 200);
}

void TestMcpServerEngine::auth_origin_and_host() {
    Server s;
    const QByteArray ping = rpc(1, QStringLiteral("ping"));
    Reply r = http(s.port, "POST", "/mcp", {{"Content-Type", "application/json"}}, ping);
    QCOMPARE(r.status, 401);
    QVERIFY(r.headers.value("www-authenticate").startsWith("Bearer"));
    QCOMPARE(r.headers.value("connection"), QByteArray("close"));
    r = http(s.port, "POST", "/mcp", mcp_headers("wrong-token"), ping);
    QCOMPARE(r.status, 401);
    QVERIFY(r.headers.value("www-authenticate").contains("invalid_token"));
    auto evil_host = mcp_headers();
    evil_host.append({"Host", "evil.example.com:" + QByteArray::number(s.port)});
    QCOMPARE(http(s.port, "POST", "/mcp", evil_host, ping).status, 403);
    auto evil_origin = mcp_headers();
    evil_origin.append({"Origin", QByteArray("http://evil.example.com")});
    QCOMPARE(http(s.port, "POST", "/mcp", evil_origin, ping).status, 403);
    auto own_origin = mcp_headers();
    own_origin.append({"Origin", "http://127.0.0.1:" + QByteArray::number(s.port)});
    QCOMPARE(http(s.port, "POST", "/mcp", own_origin, ping).status, 200);
    auto cross_site = mcp_headers();
    cross_site.append({"Sec-Fetch-Site", QByteArray("cross-site")});
    cross_site.append({"Sec-Fetch-Mode", QByteArray("cors")});
    QCOMPARE(http(s.port, "POST", "/mcp", cross_site, ping).status, 403);
    // /health: no token, nothing sensitive; GET and HEAD only.
    r = http(s.port, "GET", "/health", {});
    QCOMPARE(r.status, 200);
    QVERIFY(r.json().object().value(QStringLiteral("ok")).toBool());
    QCOMPARE(http(s.port, "POST", "/health", {}, "x").status, 405);
    QCOMPARE(http(s.port, "GET", "/nope", {}).status, 404);
    // A run token works until it ends; then 401.
    RunTokenPolicy pol;
    pol.harness_id = QStringLiteral("codex");
    const QString run = s.backend->runs.begin(pol, 60000, QDateTime::currentMSecsSinceEpoch());
    QCOMPARE(http(s.port, "POST", "/mcp", mcp_headers(run.toLatin1()), ping).status, 200);
    s.backend->runs.end(run);
    QCOMPARE(http(s.port, "POST", "/mcp", mcp_headers(run.toLatin1()), ping).status, 401);
}

void TestMcpServerEngine::http_methods_and_sessions() {
    Server s;
    Reply r = http(s.port, "GET", "/mcp", mcp_headers());
    QCOMPARE(r.status, 405);
    QVERIFY(r.headers.value("allow").contains("POST"));
    const QByteArray sid = s.initialize(QStringLiteral("gemini-cli-mcp-client"));
    QVERIFY(!sid.isEmpty());
    // An unknown session id (the terminal restarted) is served statelessly.
    r = http(s.port, "POST", "/mcp", mcp_headers(kToken, "not-a-known-session"), rpc(2, QStringLiteral("ping")));
    QCOMPARE(r.status, 200);
    // A session is bound to the credential that created it.
    RunTokenPolicy pol;
    const QString run = s.backend->runs.begin(pol, 60000, QDateTime::currentMSecsSinceEpoch());
    QCOMPARE(http(s.port, "DELETE", "/mcp", mcp_headers(run.toLatin1(), sid)).status, 404);
    QCOMPARE(http(s.port, "DELETE", "/mcp", mcp_headers(kToken, sid)).status, 200);
    QCOMPARE(http(s.port, "DELETE", "/mcp", mcp_headers(kToken, sid)).status, 404);
    QCOMPARE(http(s.port, "PUT", "/mcp", mcp_headers(), "{}").status, 405);
    // 406 when the client accepts neither JSON nor SSE.
    QCOMPARE(http(s.port, "POST", "/mcp", mcp_headers(kToken, {}, "text/html"), rpc(3, QStringLiteral("ping"))).status, 406);
}

void TestMcpServerEngine::tools_list_pages_and_run_scope() {
    Server s;
    QStringList names;
    QJsonValue cursor(QJsonValue::Undefined);
    int pages = 0;
    do {
        QJsonObject params;
        if (!cursor.isUndefined())
            params.insert(QStringLiteral("cursor"), cursor);
        const Reply r = http(s.port, "POST", "/mcp", mcp_headers(), rpc(10 + pages, QStringLiteral("tools/list"), params));
        const QJsonObject res = r.json().object().value(QStringLiteral("result")).toObject();
        for (const QJsonValue& t : res.value(QStringLiteral("tools")).toArray())
            names << t.toObject().value(QStringLiteral("name")).toString();
        cursor = res.contains(QStringLiteral("nextCursor")) ? res.value(QStringLiteral("nextCursor")) : QJsonValue(QJsonValue::Undefined);
        ++pages;
    } while (!cursor.isUndefined() && pages < 10);
    QCOMPARE(pages, 3); // page size 4, nine tools
    QCOMPARE(names.size(), 9);
    Reply bad = http(s.port, "POST", "/mcp", mcp_headers(),
                     rpc(30, QStringLiteral("tools/list"), {{QStringLiteral("cursor"), QStringLiteral("zz")}}));
    QCOMPARE(error_code(bad.json().object()), -32602);

    RunTokenPolicy pol;
    pol.scope = RunToolScope::NoNavigation;
    const QByteArray run = s.backend->runs.begin(pol, 60000, QDateTime::currentMSecsSinceEpoch()).toLatin1();
    QStringList scoped;
    cursor = QJsonValue(QJsonValue::Undefined);
    pages = 0;
    do {
        QJsonObject params;
        if (!cursor.isUndefined())
            params.insert(QStringLiteral("cursor"), cursor);
        const Reply r = http(s.port, "POST", "/mcp", mcp_headers(run), rpc(40 + pages, QStringLiteral("tools/list"), params));
        const QJsonObject res = r.json().object().value(QStringLiteral("result")).toObject();
        for (const QJsonValue& t : res.value(QStringLiteral("tools")).toArray())
            scoped << t.toObject().value(QStringLiteral("name")).toString();
        cursor = res.contains(QStringLiteral("nextCursor")) ? res.value(QStringLiteral("nextCursor")) : QJsonValue(QJsonValue::Undefined);
        ++pages;
    } while (!cursor.isUndefined() && pages < 10);
    QVERIFY(!scoped.contains(QStringLiteral("nav")));
    QCOMPARE(scoped.size(), 8);
    const Reply nav = http(s.port, "POST", "/mcp", mcp_headers(run), rpc(50, QStringLiteral("tools/call"), call_params(QStringLiteral("nav"))));
    QVERIFY(is_error_result(nav.json().object()));
    QVERIFY(first_text(nav.json().object()).contains(QStringLiteral("not available")));
}

void TestMcpServerEngine::tools_call_shapes_per_profile() {
    Server s;
    // Codex: structuredContent is never sent (Codex would drop the text).
    const QByteArray codex = s.initialize(QStringLiteral("codex-mcp-client"));
    Reply r = http(s.port, "POST", "/mcp", mcp_headers(kToken, codex),
                   rpc(2, QStringLiteral("tools/call"), call_params(QStringLiteral("echo"), {{QStringLiteral("ref"), QStringLiteral("e1")}})));
    QJsonObject res = r.json().object().value(QStringLiteral("result")).toObject();
    QVERIFY(!res.contains(QStringLiteral("structuredContent")));
    QCOMPARE(res.value(QStringLiteral("isError")).toBool(), false);
    QVERIFY(first_text(r.json().object()).contains(QStringLiteral("e1")));
    // A tool that declares an outputSchema keeps it (MCP requires it then).
    r = http(s.port, "POST", "/mcp", mcp_headers(kToken, codex),
             rpc(3, QStringLiteral("tools/call"), call_params(QStringLiteral("structured"))));
    QVERIFY(r.json().object().value(QStringLiteral("result")).toObject().contains(QStringLiteral("structuredContent")));
    // The client identity reaches the backend: profile + label; the run token's harness wins.
    {
        QMutexLocker lock(&s.backend->mutex);
        QCOMPARE(s.backend->kinds.last(), profiles::ClientKind::Codex);
        QCOMPARE(s.backend->labels.last(), QStringLiteral("codex-mcp-client"));
    }
    RunTokenPolicy pol;
    pol.harness_id = QStringLiteral("qwen");
    const QByteArray run = s.backend->runs.begin(pol, 60000, QDateTime::currentMSecsSinceEpoch()).toLatin1();
    http(s.port, "POST", "/mcp", mcp_headers(run), rpc(4, QStringLiteral("tools/call"), call_params(QStringLiteral("echo"))));
    {
        QMutexLocker lock(&s.backend->mutex);
        QCOMPARE(s.backend->kinds.last(), profiles::ClientKind::QwenCode);
        QVERIFY(s.backend->labels.last().endsWith(QStringLiteral(" · terminal AI")));
    }
    // Activity records for every finished call.
    QTRY_VERIFY_WITH_TIMEOUT(
        [&]() {
            QMutexLocker lock(&s.records_mutex);
            return s.records.size() >= 3;
        }(),
        3000);
}

void TestMcpServerEngine::caller_ids_reach_the_tool_scope() {
    // The stable client id a tool handler sees (ScopedCallClient — what the UI tools
    // stamp on an agent's orders as "agent:<id>") is the client profile id, never
    // the free-text label; a run-scoped token adds its run id.
    Server s;
    auto whoami = [&](const QList<QPair<QByteArray, QByteArray>>& headers) {
        const Reply r = http(s.port, "POST", "/mcp", headers,
                             rpc(9, QStringLiteral("tools/call"), call_params(QStringLiteral("whoami"))));
        return first_text(r.json().object());
    };
    QVERIFY(!ScopedCallClient::client_id().size()); // nothing leaks onto the test thread
    // Identified by initialize.clientInfo.
    QCOMPARE(whoami(mcp_headers(kToken, s.initialize(QStringLiteral("claude-code")))), QStringLiteral("claude_code|"));
    QCOMPARE(whoami(mcp_headers(kToken, s.initialize(QStringLiteral("codex-mcp-client")))), QStringLiteral("codex|"));
    QCOMPARE(whoami(mcp_headers(kToken, s.initialize(QStringLiteral("gemini-cli-mcp-client")))), QStringLiteral("gemini|"));
    QCOMPARE(whoami(mcp_headers(kToken, s.initialize(QStringLiteral("some-new-tool")))), QStringLiteral("external|"));
    // A run-scoped token: the run's harness decides the profile (whatever the
    // client calls itself) and the run id travels with it.
    RunTokenPolicy pol;
    pol.harness_id = QStringLiteral("claude");
    pol.run_id = QStringLiteral("run-42");
    const QByteArray run = s.backend->runs.begin(pol, 60000, QDateTime::currentMSecsSinceEpoch()).toLatin1();
    QCOMPARE(whoami(mcp_headers(run)), QStringLiteral("claude_code|run-42")); // stateless, no initialize
    QCOMPARE(whoami(mcp_headers(run, s.initialize(QStringLiteral("some-new-tool"), run))),
             QStringLiteral("claude_code|run-42"));
    pol.harness_id = QStringLiteral("copilot");
    pol.run_id = QStringLiteral("run-43");
    const QByteArray run2 = s.backend->runs.begin(pol, 60000, QDateTime::currentMSecsSinceEpoch()).toLatin1();
    QCOMPARE(whoami(mcp_headers(run2)), QStringLiteral("copilot|run-43"));
    // Every profile has a stable id: lower-case, no spaces; unknown = "external".
    for (const profiles::ClientKind k : profiles::all_kinds()) {
        const QString id = QString::fromLatin1(profiles::profile(k).actor_id);
        QVERIFY2(QRegularExpression(QStringLiteral("^[a-z][a-z_]*$")).match(id).hasMatch(), qPrintable(id));
    }
    QCOMPARE(QString::fromLatin1(profiles::profile(profiles::ClientKind::Generic).actor_id), QStringLiteral("external"));
}

void TestMcpServerEngine::approval_headings_and_grant_matrix() {
    namespace ap = fincept::mcp::approval;
    struct Row {
        const char* kind;
        const char* heading;
        bool grant;
        bool danger; // an order whose mode the screen does not show counts as real money
    };
    const Row rows[] = {
        {"order", "Place / change an order", false, true},
        {"trade", "Place / change an order", false, true},
        {"money", "Move money or change balances", false, true},
        {"spend", "Use credits or model quota", true, false},
        {"delete", "Delete data", true, false},
        {"destructive", "Delete data", true, false},
        {"account", "Change account or security settings", false, false},
        {"code", "Run code on this computer", false, false},
        {"file", "Write a file", true, false},
        {"generic", "Agent action needs your approval", true, false},
        {"something-new", "Agent action needs your approval", true, false},
    };
    for (const Row& row : rows) {
        ToolApprovalRequest q;
        q.kind = QString::fromLatin1(row.kind);
        q.client = QStringLiteral("Claude Code · terminal AI");
        q.client_id = QStringLiteral("claude_code");
        q.run_id = QStringLiteral("run-1");
        q.tool = QStringLiteral("ui_act: click X");
        const ap::Presentation p = ap::present(q);
        QCOMPARE(p.heading, QString::fromLatin1(row.heading));
        QCOMPARE(p.grant_allowed, row.grant);
        QCOMPARE(ap::timed_grant_allowed(q.kind), row.grant);
        QCOMPARE(p.danger, row.danger);
        QCOMPARE(ap::grant_key(q).isEmpty(), !row.grant);
        QVERIFY(!p.sentence.isEmpty() && !p.status.isEmpty() && !p.grant_label.isEmpty() && !p.grant_note.isEmpty());
        QCOMPARE(p.client, QStringLiteral("Claude Code · terminal AI — claude_code · run run-1"));
    }
    // An order states LIVE / PAPER as the screen shows it, or says it does not.
    ToolApprovalRequest o;
    o.kind = QStringLiteral("order");
    o.trading_mode = ApprovalTradingMode::Live;
    ap::Presentation p = ap::present(o);
    QVERIFY(p.sentence.contains(QStringLiteral("LIVE order")) && p.danger);
    QCOMPARE(p.status, QStringLiteral("LIVE · REAL MONEY"));
    o.trading_mode = ApprovalTradingMode::Paper;
    p = ap::present(o);
    QVERIFY(p.sentence.contains(QStringLiteral("PAPER order")) && !p.danger);
    QVERIFY(!p.grant_allowed); // a paper order is still approved one at a time
    o.trading_mode = ApprovalTradingMode::Unknown;
    p = ap::present(o);
    QVERIFY(p.sentence.contains(QStringLiteral("does not show")) && p.danger);
    // Spend: the cost when known.
    ToolApprovalRequest sp;
    sp.kind = QStringLiteral("spend");
    QVERIFY(ap::present(sp).sentence.contains(QStringLiteral("not shown")));
    sp.cost = QStringLiteral("about 40 credits");
    QVERIFY(ap::present(sp).sentence.contains(QStringLiteral("Cost: about 40 credits.")));
    // No kind: the legacy flags decide.
    ToolApprovalRequest legacy;
    legacy.live_trading = true;
    QCOMPARE(ap::kind_of(legacy), QStringLiteral("order"));
    legacy.live_trading = false;
    QCOMPARE(ap::kind_of(legacy), QStringLiteral("generic"));
    // Unknown client.
    QCOMPARE(ap::present(ToolApprovalRequest{}).client, QStringLiteral("unknown client"));
    // LIVE / PAPER from what a screen shows.
    QVERIFY(ap::trading_mode_from_text(QStringLiteral("LIVE BUY LIMIT 0.5 BTC on KRAKEN")) == ApprovalTradingMode::Live);
    QVERIFY(ap::trading_mode_from_text(QStringLiteral("confirm live order — live buy 1 aapl")) == ApprovalTradingMode::Live);
    QVERIFY(ap::trading_mode_from_text(QStringLiteral("PAPER")) == ApprovalTradingMode::Paper);
    QVERIFY(ap::trading_mode_from_text(QStringLiteral("paper trading account")) == ApprovalTradingMode::Paper);
    QVERIFY(ap::trading_mode_from_text(QStringLiteral("LIVE BUY 1 AAPL\nPAPER")) == ApprovalTradingMode::Unknown);
    QVERIFY(ap::trading_mode_from_text(QStringLiteral("Place order")) == ApprovalTradingMode::Unknown);
    QVERIFY(ap::trading_mode_from_text(QStringLiteral("live prices, newspaper")) == ApprovalTradingMode::Unknown);
}

void TestMcpServerEngine::timed_spend_grant_never_covers_an_order() {
    namespace ap = fincept::mcp::approval;
    ap::GrantBook book;
    const qint64 t0 = 1000000;
    ToolApprovalRequest spend;
    spend.kind = QStringLiteral("spend");
    spend.client = QStringLiteral("Claude Code · terminal AI");
    spend.client_id = QStringLiteral("claude_code");
    spend.run_id = QStringLiteral("run-1");
    spend.screen = QStringLiteral("p3 \"Crypto Trading\"");
    spend.tool = QStringLiteral("ui_act: click Yes");
    book.add(spend, t0);
    QCOMPARE(book.size(), 1);
    QVERIFY(book.covers(spend, t0 + 1000));
    // Same client, run, screen and words — but an order / money / account / code request.
    for (const char* other : {"order", "trade", "money", "account", "code"}) {
        ToolApprovalRequest r = spend;
        r.kind = QString::fromLatin1(other);
        QVERIFY2(!book.covers(r, t0 + 1000), other);
        book.add(r, t0); // never recorded
        QCOMPARE(book.size(), 1);
    }
    ToolApprovalRequest legacy_order = spend; // no kind, the old live-trading flag
    legacy_order.kind.clear();
    legacy_order.live_trading = true;
    QVERIFY(!book.covers(legacy_order, t0 + 1000));
    // Never across kinds, clients, runs or screens.
    ToolApprovalRequest r = spend;
    r.kind = QStringLiteral("delete");
    QVERIFY(!book.covers(r, t0 + 1000));
    r = spend;
    r.client_id = QStringLiteral("codex");
    QVERIFY(!book.covers(r, t0 + 1000));
    r = spend;
    r.run_id = QStringLiteral("run-2");
    QVERIFY(!book.covers(r, t0 + 1000));
    r = spend;
    r.screen = QStringLiteral("p4 \"AI Chat\"");
    QVERIFY(!book.covers(r, t0 + 1000));
    // A grant ends after its time, or with its run.
    QVERIFY(!book.covers(spend, t0 + 15 * 60 * 1000));
    book.add(spend, t0);
    ToolApprovalRequest other_run = spend;
    other_run.run_id = QStringLiteral("run-7");
    book.add(other_run, t0);
    book.clear_run(QStringLiteral("run-1"));
    QVERIFY(!book.covers(spend, t0 + 1000));
    QVERIFY(book.covers(other_run, t0 + 1000));
    book.clear();
    QCOMPARE(book.size(), 0);
}

void TestMcpServerEngine::cancellation_really_cancels() {
    Server s;
    const QByteArray sid = s.initialize(QStringLiteral("claude-code"));
    QTcpSocket a;
    a.connectToHost(QHostAddress::LocalHost, s.port);
    QVERIFY(a.waitForConnected(3000));
    QElapsedTimer t;
    t.start();
    a.write(request_bytes(s.port, "POST", "/mcp", mcp_headers(kToken, sid, "application/json"),
                          rpc(31, QStringLiteral("tools/call"), call_params(QStringLiteral("slow"), {{QStringLiteral("ms"), 8000}}))));
    a.flush();
    QTRY_VERIFY_WITH_TIMEOUT(s.backend->started.load() >= 1, 3000);
    const Reply c = http(s.port, "POST", "/mcp", mcp_headers(kToken, sid),
                         note(QStringLiteral("notifications/cancelled"),
                              {{QStringLiteral("requestId"), 31}, {QStringLiteral("reason"), QStringLiteral("test")}}));
    QCOMPARE(c.status, 202);
    const Reply r = read_reply(a, 6000);
    QVERIFY(r.ok);
    QVERIFY(t.elapsed() < 4000);
    QVERIFY(first_text(r.json().object()).contains(QStringLiteral("Cancelled")));
    QTRY_COMPARE_WITH_TIMEOUT(s.backend->cancels.load(), 1, 3000); // the handler itself saw the flag
    // A cancel for a request that is not running is ignored.
    QCOMPARE(http(s.port, "POST", "/mcp", mcp_headers(kToken, sid),
                  note(QStringLiteral("notifications/cancelled"), {{QStringLiteral("requestId"), 999}}))
                 .status,
             202);
}

void TestMcpServerEngine::deadline_answers_a_stuck_tool() {
    Server s; // hang: budget 300 ms + grace 400 ms
    QElapsedTimer t;
    t.start();
    const Reply r = http(s.port, "POST", "/mcp", mcp_headers(kToken, {}, "application/json"),
                         rpc(1, QStringLiteral("tools/call"), call_params(QStringLiteral("hang"))), 8000);
    QVERIFY(r.ok);
    QVERIFY2(t.elapsed() < 3000, qPrintable(QString::number(t.elapsed())));
    QVERIFY(is_error_result(r.json().object()));
    QVERIFY(first_text(r.json().object()).contains(QStringLiteral("Timed out")));
}

void TestMcpServerEngine::concurrency_limits() {
    Server s; // 3 per client, 6 overall
    const QByteArray sid = s.initialize(QStringLiteral("cursor-vscode"));
    std::vector<std::unique_ptr<QTcpSocket>> socks;
    for (int i = 0; i < 3; ++i) {
        auto sock = std::make_unique<QTcpSocket>();
        sock->connectToHost(QHostAddress::LocalHost, s.port);
        QVERIFY(sock->waitForConnected(3000));
        sock->write(request_bytes(s.port, "POST", "/mcp", mcp_headers(kToken, sid, "application/json"),
                                  rpc(100 + i, QStringLiteral("tools/call"), call_params(QStringLiteral("slow"), {{QStringLiteral("ms"), 1500}}))));
        sock->flush();
        socks.push_back(std::move(sock));
    }
    QTRY_VERIFY_WITH_TIMEOUT(s.backend->started.load() >= 3, 3000);
    const Reply busy = http(s.port, "POST", "/mcp", mcp_headers(kToken, sid),
                            rpc(200, QStringLiteral("tools/call"), call_params(QStringLiteral("echo"))));
    QVERIFY(is_error_result(busy.json().object()));
    QVERIFY(first_text(busy.json().object()).contains(QStringLiteral("busy")));
    // Another client is not starved by the first one's burst.
    const QByteArray other = s.initialize(QStringLiteral("grok-cli"));
    const Reply fine = http(s.port, "POST", "/mcp", mcp_headers(kToken, other),
                            rpc(201, QStringLiteral("tools/call"), call_params(QStringLiteral("echo"))));
    QVERIFY(!is_error_result(fine.json().object()));
    for (auto& sock : socks)
        QVERIFY(read_reply(*sock, 5000).ok);
    // Slots are released: the first client can call again.
    const Reply again = http(s.port, "POST", "/mcp", mcp_headers(kToken, sid),
                             rpc(202, QStringLiteral("tools/call"), call_params(QStringLiteral("echo"))));
    QVERIFY(!is_error_result(again.json().object()));
}

void TestMcpServerEngine::progress_streams_as_sse() {
    Server s;
    const QByteArray sid = s.initialize(QStringLiteral("claude-code"));
    const Reply r = http(s.port, "POST", "/mcp", mcp_headers(kToken, sid),
                         rpc(7, QStringLiteral("tools/call"), call_params(QStringLiteral("progress"), {}, QStringLiteral("tok-1"))), 10000);
    QVERIFY(r.ok);
    QVERIFY(r.sse());
    QCOMPARE(r.headers.value("transfer-encoding"), QByteArray("chunked"));
    double last = -1;
    int progress = 0;
    for (const QJsonObject& e : r.events()) {
        if (e.value(QStringLiteral("method")).toString() != QLatin1String("notifications/progress"))
            continue;
        const QJsonObject p = e.value(QStringLiteral("params")).toObject();
        QCOMPARE(p.value(QStringLiteral("progressToken")).toString(), QStringLiteral("tok-1"));
        QVERIFY(p.value(QStringLiteral("progress")).toDouble() > last);
        last = p.value(QStringLiteral("progress")).toDouble();
        ++progress;
    }
    QVERIFY(progress >= 3);
    QCOMPARE(first_text(r.message()), QStringLiteral("progress done"));
    QCOMPARE(r.message().value(QStringLiteral("id")).toInt(), 7);
    // Without a progress token the same call gets no progress events.
    const Reply quiet = http(s.port, "POST", "/mcp", mcp_headers(kToken, sid, "application/json"),
                             rpc(8, QStringLiteral("tools/call"), call_params(QStringLiteral("progress"))), 10000);
    QVERIFY(!quiet.sse());
    QCOMPARE(first_text(quiet.json().object()), QStringLiteral("progress done"));
}

void TestMcpServerEngine::long_call_upgrades_to_sse_with_heartbeats() {
    Server s; // upgrade after 400 ms, heartbeat every 200 ms
    const Reply r = http(s.port, "POST", "/mcp", mcp_headers(),
                         rpc(9, QStringLiteral("tools/call"), call_params(QStringLiteral("slow"), {{QStringLiteral("ms"), 1300}})), 8000);
    QVERIFY(r.ok);
    QVERIFY(r.sse());
    QVERIFY(r.body.count(": keep-alive") >= 2);
    QCOMPARE(first_text(r.message()), QStringLiteral("slow done"));
    // A fast call stays plain JSON.
    const Reply fast = http(s.port, "POST", "/mcp", mcp_headers(), rpc(10, QStringLiteral("tools/call"), call_params(QStringLiteral("echo"))));
    QVERIFY(!fast.sse());
}

void TestMcpServerEngine::json_only_client_gets_json() {
    Server s;
    const Reply r = http(s.port, "POST", "/mcp", mcp_headers(kToken, {}, "application/json"),
                         rpc(11, QStringLiteral("tools/call"), call_params(QStringLiteral("slow"), {{QStringLiteral("ms"), 900}})), 8000);
    QVERIFY(!r.sse());
    QCOMPARE(first_text(r.json().object()), QStringLiteral("slow done"));
}

void TestMcpServerEngine::disconnect_cancels_running_call() {
    Server s;
    {
        QTcpSocket a;
        a.connectToHost(QHostAddress::LocalHost, s.port);
        QVERIFY(a.waitForConnected(3000));
        a.write(request_bytes(s.port, "POST", "/mcp", mcp_headers(),
                              rpc(12, QStringLiteral("tools/call"), call_params(QStringLiteral("slow"), {{QStringLiteral("ms"), 8000}}))));
        a.flush();
        QTRY_VERIFY_WITH_TIMEOUT(s.backend->started.load() >= 1, 3000);
        a.abort();
    }
    QTRY_COMPARE_WITH_TIMEOUT(s.backend->cancels.load(), 1, 4000);
    QTRY_COMPARE_WITH_TIMEOUT(s.engine->inflight_count(), 0, 4000);
}

void TestMcpServerEngine::idle_and_slow_clients_are_closed() {
    Server s; // idle 1.5 s, request 1.2 s
    QTcpSocket idle;
    idle.connectToHost(QHostAddress::LocalHost, s.port);
    QVERIFY(idle.waitForConnected(3000));
    QTcpSocket slowloris;
    slowloris.connectToHost(QHostAddress::LocalHost, s.port);
    QVERIFY(slowloris.waitForConnected(3000));
    slowloris.write("POST /mcp HTTP/1.1\r\nHost: 127.0.0.1:" + QByteArray::number(s.port) + "\r\n");
    slowloris.flush();
    QElapsedTimer t;
    t.start();
    // Keep trickling one header byte at a time: the request deadline still ends it.
    bool slow_closed = false;
    while (t.elapsed() < 4000 && !slow_closed) {
        slowloris.write("X");
        slowloris.flush();
        slowloris.waitForReadyRead(100);
        if (slowloris.readAll().contains("408") || slowloris.state() != QAbstractSocket::ConnectedState)
            slow_closed = true;
    }
    QVERIFY(slow_closed);
    QTRY_VERIFY_WITH_TIMEOUT((idle.waitForReadyRead(50), idle.state() != QAbstractSocket::ConnectedState), 5000);
    QTRY_COMPARE_WITH_TIMEOUT(s.engine->connection_count(), 0, 3000);
}

void TestMcpServerEngine::oversized_and_malformed_input() {
    Server s;
    // Declared too big: refused before a byte of body is read.
    QTcpSocket a;
    a.connectToHost(QHostAddress::LocalHost, s.port);
    QVERIFY(a.waitForConnected(3000));
    a.write("POST /mcp HTTP/1.1\r\nHost: 127.0.0.1:" + QByteArray::number(s.port) + "\r\nAuthorization: Bearer " +
            kToken + "\r\nContent-Length: 9000000\r\n\r\n");
    a.flush();
    QCOMPARE(read_reply(a).status, 413);
    // No length on a POST: 411.
    QTcpSocket b;
    b.connectToHost(QHostAddress::LocalHost, s.port);
    QVERIFY(b.waitForConnected(3000));
    b.write("POST /mcp HTTP/1.1\r\nHost: 127.0.0.1:" + QByteArray::number(s.port) + "\r\nAuthorization: Bearer " + kToken +
            "\r\n\r\n");
    b.flush();
    QCOMPARE(read_reply(b).status, 411);
    // Deeply nested JSON: a parse error, never a crash.
    const QByteArray deep = QByteArray(5000, '[') + QByteArray(5000, ']');
    const Reply r = http(s.port, "POST", "/mcp", mcp_headers(), deep);
    QCOMPARE(r.status, 400);
    // A large but legal body (2 MB argument) round-trips.
    const QString big(2 * 1024 * 1024, QLatin1Char('y'));
    const Reply ok = http(s.port, "POST", "/mcp", mcp_headers(),
                          rpc(13, QStringLiteral("tools/call"), call_params(QStringLiteral("echo"), {{QStringLiteral("ref"), big}})), 15000);
    QCOMPARE(ok.status, 200);
    QVERIFY(first_text(ok.json().object()).size() > 2 * 1024 * 1024);
    // Garbage request line.
    QTcpSocket c;
    c.connectToHost(QHostAddress::LocalHost, s.port);
    QVERIFY(c.waitForConnected(3000));
    c.write("\x01\x02\x03 garbage\r\n\r\n");
    c.flush();
    QCOMPARE(read_reply(c).status, 400);
    // The server is still fine.
    QCOMPARE(http(s.port, "POST", "/mcp", mcp_headers(), rpc(14, QStringLiteral("ping"))).status, 200);
}

void TestMcpServerEngine::pipelining_with_backpressure() {
    Server s;
    QTcpSocket sock;
    sock.connectToHost(QHostAddress::LocalHost, s.port);
    QVERIFY(sock.waitForConnected(3000));
    sock.setReadBufferSize(64 * 1024);
    // 60 pipelined calls whose answers total ~6 MB, without reading anything.
    const int n = 60;
    QByteArray all;
    for (int i = 0; i < n; ++i)
        all += request_bytes(s.port, "POST", "/mcp", mcp_headers(kToken, {}, "application/json"),
                             rpc(1000 + i, QStringLiteral("tools/call"), call_params(QStringLiteral("big"), {{QStringLiteral("n"), 100000}})),
                             /*keep_alive=*/true);
    sock.write(all);
    sock.flush();
    QThread::msleep(500);
    int answered = 0;
    QElapsedTimer t;
    t.start();
    QByteArray carry;
    while (answered < n && t.elapsed() < 30000) {
        const Reply r = read_reply(sock, carry, 10000);
        if (!r.ok)
            break;
        QCOMPARE(r.json().object().value(QStringLiteral("id")).toInt(), 1000 + answered); // in order
        ++answered;
    }
    QCOMPARE(answered, n);
}

void TestMcpServerEngine::concurrent_clients() {
    EngineLimits roomy = Server::test_limits(); // this checks correctness under load, not the caps
    roomy.max_inflight = 64;
    roomy.max_inflight_per_client = 16;
    Server s(roomy);
    std::atomic<int> ok{0};
    std::vector<std::thread> threads;
    for (int c = 0; c < 8; ++c) {
        threads.emplace_back([&s, &ok, c]() {
            const Reply init = http(s.port, "POST", "/mcp", mcp_headers(),
                                    rpc(1, QStringLiteral("initialize"), init_params(QStringLiteral("client-%1").arg(c))));
            const QByteArray sid = init.headers.value("mcp-session-id");
            int good = 0;
            for (int i = 0; i < 10; ++i) {
                const Reply r = http(s.port, "POST", "/mcp", mcp_headers(kToken, sid),
                                     rpc(10 + i, QStringLiteral("tools/call"),
                                         call_params(QStringLiteral("echo"), {{QStringLiteral("ref"), QStringLiteral("c%1-%2").arg(c).arg(i)}})));
                if (first_text(r.json().object()).contains(QStringLiteral("c%1-%2").arg(c).arg(i)))
                    ++good;
            }
            if (good == 10)
                ok.fetch_add(1);
        });
    }
    for (auto& t : threads)
        t.join();
    QCOMPARE(ok.load(), 8);
    QCOMPARE(s.engine->inflight_count(), 0);
}

void TestMcpServerEngine::session_expiry() {
    EngineLimits quick = Server::test_limits();
    quick.session_idle_ms = 800; // sessions idle 0.8 s are forgotten (sweep every 0.2 s)
    Server s(quick);
    for (int i = 0; i < 5; ++i)
        s.initialize(QStringLiteral("claude-code"));
    QCOMPARE(s.engine->session_count(), 5);
    QTRY_COMPARE_WITH_TIMEOUT(s.engine->session_count(), 0, 4000);
    // And the cap holds: more initializes than max_sessions never grow the table.
    EngineLimits l = Server::test_limits();
    l.max_sessions = 3;
    l.session_idle_ms = 60000;
    Server capped(l);
    for (int i = 0; i < 10; ++i)
        capped.initialize(QStringLiteral("codex-mcp-client"));
    QCOMPARE(capped.engine->session_count(), 3);
}

void TestMcpServerEngine::shutdown_answers_calls_in_flight() {
    Server s;
    QTcpSocket a;
    a.connectToHost(QHostAddress::LocalHost, s.port);
    QVERIFY(a.waitForConnected(3000));
    a.write(request_bytes(s.port, "POST", "/mcp", mcp_headers(kToken, {}, "application/json"),
                          rpc(77, QStringLiteral("tools/call"), call_params(QStringLiteral("slow"), {{QStringLiteral("ms"), 8000}}))));
    a.flush();
    QTRY_VERIFY_WITH_TIMEOUT(s.backend->started.load() >= 1, 3000);
    s.shutdown_engine(QStringLiteral("test shutdown"));
    const Reply r = read_reply(a, 3000);
    QVERIFY(r.ok); // answered, not reset
    QCOMPARE(r.json().object().value(QStringLiteral("id")).toInt(), 77);
    QVERIFY(is_error_result(r.json().object()));
    QVERIFY(first_text(r.json().object()).contains(QStringLiteral("stopped")));
    QCOMPARE(s.engine->port(), quint16(0));
    QCOMPARE(s.engine->inflight_count(), 0);
    // A late completion from the backend after shutdown is harmless.
    QTRY_COMPARE_WITH_TIMEOUT(s.backend->cancels.load(), 1, 3000);
}

void TestMcpServerEngine::latency_of_a_trivial_call() {
    Server s;
    const QByteArray sid = s.initialize(QStringLiteral("claude-code"));
    QTcpSocket sock;
    sock.connectToHost(QHostAddress::LocalHost, s.port);
    QVERIFY(sock.waitForConnected(3000));
    QList<double> keep_alive_ms;
    QByteArray carry;
    for (int i = 0; i < 300; ++i) {
        QElapsedTimer t;
        t.start();
        sock.write(request_bytes(s.port, "POST", "/mcp", mcp_headers(kToken, sid),
                                 rpc(i, QStringLiteral("tools/call"), call_params(QStringLiteral("echo"), {{QStringLiteral("ref"), QStringLiteral("x")}})),
                                 /*keep_alive=*/true));
        sock.flush();
        const Reply r = read_reply(sock, carry, 5000);
        QVERIFY(r.ok);
        keep_alive_ms << t.nsecsElapsed() / 1e6;
    }
    QList<double> fresh_ms;
    for (int i = 0; i < 100; ++i) {
        QElapsedTimer t;
        t.start();
        const Reply r = http(s.port, "POST", "/mcp", mcp_headers(kToken, sid),
                             rpc(i, QStringLiteral("tools/call"), call_params(QStringLiteral("echo"), {{QStringLiteral("ref"), QStringLiteral("x")}})));
        QVERIFY(r.ok);
        fresh_ms << t.nsecsElapsed() / 1e6;
    }
    auto stats = [](QList<double> v) {
        std::sort(v.begin(), v.end());
        double sum = 0;
        for (double x : v)
            sum += x;
        return QStringLiteral("mean %1 ms, p50 %2 ms, p95 %3 ms")
            .arg(sum / v.size(), 0, 'f', 2)
            .arg(v.at(v.size() / 2), 0, 'f', 2)
            .arg(v.at(static_cast<int>(v.size() * 0.95)), 0, 'f', 2);
    };
    qInfo().noquote() << "LATENCY trivial tools/call, keep-alive:" << stats(keep_alive_ms);
    qInfo().noquote() << "LATENCY trivial tools/call, new connection each:" << stats(fresh_ms);
    std::sort(keep_alive_ms.begin(), keep_alive_ms.end());
    QVERIFY(keep_alive_ms.at(keep_alive_ms.size() / 2) < 50.0);
}

// ── profiles ─────────────────────────────────────────────────────────────────

void TestMcpServerEngine::profile_detection() {
    using profiles::ClientKind;
    QCOMPARE(profiles::detect_client(QStringLiteral("claude-code")), ClientKind::ClaudeCode);
    QCOMPARE(profiles::detect_client(QStringLiteral("claude-ai")), ClientKind::ClaudeDesktop);
    QCOMPARE(profiles::detect_client(QStringLiteral("codex-mcp-client")), ClientKind::Codex);
    QCOMPARE(profiles::detect_client(QStringLiteral("gemini-cli-mcp-client")), ClientKind::GeminiCli);
    QCOMPARE(profiles::detect_client(QStringLiteral("qwen-code-mcp-client")), ClientKind::QwenCode);
    QCOMPARE(profiles::detect_client(QStringLiteral("cursor-vscode")), ClientKind::Cursor);
    QCOMPARE(profiles::detect_client(QStringLiteral("github-copilot-cli")), ClientKind::Copilot);
    QCOMPARE(profiles::detect_client(QStringLiteral("grok-cli")), ClientKind::Grok);
    QCOMPARE(profiles::detect_client(QStringLiteral("Visual Studio Code")), ClientKind::VsCode);
    QCOMPARE(profiles::detect_client(QStringLiteral("something")), ClientKind::Generic);
    QCOMPARE(profiles::detect_client(QString(), QStringLiteral("fincept-mcp-stdio/1.1 (claude-ai)")), ClientKind::ClaudeDesktop);
    for (const char* h : {"claude", "codex", "gemini", "qwen", "cursor", "copilot", "grok"})
        QVERIFY2(profiles::kind_for_harness(QString::fromLatin1(h)) != ClientKind::Generic, h);
    QCOMPARE(profiles::kind_for_harness(QStringLiteral("nope")), ClientKind::Generic);
    for (ClientKind k : profiles::all_kinds()) {
        const profiles::ClientProfile& p = profiles::profile(k);
        QCOMPARE(p.kind, k);
        QVERIFY(!p.structured_content); // see McpClientProfile.cpp
        QVERIFY(p.tool_name_budget >= 32);
    }
}

void TestMcpServerEngine::strict_dialect_golden() {
    FakeBackend b;
    const QJsonObject original = b.catalogue()->tools.value(QStringLiteral("echo")).input_schema;
    const QJsonObject strict = profiles::shape_input_schema(original, profiles::SchemaDialect::OpenAiStrict);
    QString why;
    QVERIFY2(profiles::is_strict_schema(strict, &why), qPrintable(why));
    QVERIFY2(!profiles::is_strict_schema(original, &why), "the original is not strict");
    const QByteArray golden =
        "{\"additionalProperties\":false,\"properties\":{\"mode\":{\"enum\":[\"a\",\"b\",null],\"type\":[\"string\",\"null\"]},"
        "\"ms\":{\"type\":[\"integer\",\"null\"]},\"params\":{\"additionalProperties\":false,\"properties\":{},\"required\":[],"
        "\"type\":[\"object\",\"null\"]},\"ref\":{\"type\":\"string\"},\"steps\":{\"items\":{\"additionalProperties\":false,"
        "\"properties\":{\"action\":{\"type\":\"string\"},\"text\":{\"type\":[\"string\",\"null\"]}},\"required\":[\"action\","
        "\"text\"],\"type\":\"object\"},\"type\":[\"array\",\"null\"]},\"value\":{\"description\":\"any\",\"type\":[\"string\","
        "\"number\",\"boolean\",\"null\"]}},\"required\":[\"mode\",\"ms\",\"params\",\"ref\",\"steps\",\"value\"],\"type\":\"object\"}";
    QCOMPARE(QJsonDocument(strict).toJson(QJsonDocument::Compact), golden);
    // The nulls a strict client must send for optional properties are dropped again.
    const QJsonObject sent{{QStringLiteral("ref"), QStringLiteral("e1")},
                           {QStringLiteral("mode"), QJsonValue()},
                           {QStringLiteral("ms"), QJsonValue()},
                           {QStringLiteral("params"), QJsonValue()},
                           {QStringLiteral("value"), 5},
                           {QStringLiteral("steps"),
                            QJsonArray{QJsonObject{{QStringLiteral("action"), QStringLiteral("click")},
                                                   {QStringLiteral("text"), QJsonValue()}}}}};
    const QJsonObject restored = profiles::restore_call_args(original, sent, profiles::SchemaDialect::OpenAiStrict);
    QCOMPARE(QJsonDocument(restored).toJson(QJsonDocument::Compact),
             QByteArray("{\"ref\":\"e1\",\"steps\":[{\"action\":\"click\"}],\"value\":5}"));
}

void TestMcpServerEngine::gemini_dialect_golden() {
    FakeBackend b;
    const QJsonObject original = b.catalogue()->tools.value(QStringLiteral("echo")).input_schema;
    const QJsonObject g = profiles::shape_input_schema(original, profiles::SchemaDialect::Gemini);
    const QByteArray text = QJsonDocument(g).toJson(QJsonDocument::Compact);
    for (const char* banned : {"additionalProperties", "$schema", "oneOf", "allOf", "const"})
        QVERIFY2(!text.contains(banned), banned);
    // A free-form object travels as a JSON string and is parsed back.
    QCOMPARE(g.value(QStringLiteral("properties")).toObject().value(QStringLiteral("params")).toObject().value(QStringLiteral("type")).toString(),
             QStringLiteral("string"));
    const QJsonObject restored = profiles::restore_call_args(
        original, {{QStringLiteral("ref"), QStringLiteral("e1")}, {QStringLiteral("params"), QStringLiteral("{\"k\":1}")}},
        profiles::SchemaDialect::Gemini);
    QCOMPARE(restored.value(QStringLiteral("params")).toObject().value(QStringLiteral("k")).toInt(), 1);
    // A parameterless tool: an object schema without an empty properties map.
    const QJsonObject none = profiles::shape_input_schema(QJsonObject{}, profiles::SchemaDialect::Gemini);
    QCOMPARE(QJsonDocument(none).toJson(QJsonDocument::Compact), QByteArray("{\"type\":\"object\"}"));
    // Standard leaves a schema as declared (only normalised to an object).
    QCOMPARE(profiles::shape_input_schema(original, profiles::SchemaDialect::Standard), original);
}

void TestMcpServerEngine::profile_budget_caps() {
    Server s;
    const QByteArray claude = s.initialize(QStringLiteral("claude-code"));
    const QByteArray copilot = s.initialize(QStringLiteral("github-copilot-cli"));
    const QByteArray codex = s.initialize(QStringLiteral("codex-mcp-client"));
    QMetaObject::invokeMethod(s.engine, [e = s.engine]() { e->set_result_budget_bytes(256 * 1024); },
                              Qt::BlockingQueuedConnection);
    for (const QByteArray& sid : {claude, copilot, codex})
        http(s.port, "POST", "/mcp", mcp_headers(kToken, sid), rpc(1, QStringLiteral("tools/call"), call_params(QStringLiteral("echo"))));
    QMutexLocker lock(&s.backend->mutex);
    const QList<int> b = s.backend->budgets.mid(s.backend->budgets.size() - 3);
    QCOMPARE(b, (QList<int>{48000, 20000, 256 * 1024}));
}

// ── stdio bridge ─────────────────────────────────────────────────────────────

void TestMcpServerEngine::stdio_bridge_end_to_end() {
    QString python = qEnvironmentVariable("FINCEPT_TEST_PYTHON");
    if (python.isEmpty())
        python = QStandardPaths::findExecutable(QStringLiteral("python"));
    const QString script =
        QFileInfo(QString::fromUtf8(FINCEPT_SOURCE_DIR) + QStringLiteral("/scripts/fincept_mcp/fincept_mcp_stdio.py"))
            .absoluteFilePath();
    if (python.isEmpty() || QString::fromUtf8(FINCEPT_SOURCE_DIR).isEmpty() || !QFileInfo::exists(script))
        QSKIP("no Python interpreter or bridge script");

    EngineLimits roomy = Server::test_limits();
    roomy.max_inflight = 64;
    roomy.max_inflight_per_client = 32;
    roomy.deadline_grace_ms = 30000;
    Server s(roomy);
    QProcess proc;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("FINCEPT_COMMUNITY_MCP_TOKEN"), QString::fromLatin1(kToken));
    env.insert(QStringLiteral("FINCEPT_MCP_TOKEN"), QStringLiteral("private-build-token")); // never read
    proc.setProcessEnvironment(env);
    proc.start(python, {QStringLiteral("-I"), script, QStringLiteral("--url"),
                        QStringLiteral("http://127.0.0.1:%1/mcp").arg(s.port)});
    QVERIFY(proc.waitForStarted(10000));
    QByteArray out_buf;
    auto read_message = [&](int id, int timeout_ms = 15000) {
        QElapsedTimer t;
        t.start();
        while (t.elapsed() < timeout_ms) {
            for (;;) {
                const qsizetype nl = out_buf.indexOf('\n');
                if (nl < 0)
                    break;
                const QJsonObject o = QJsonDocument::fromJson(out_buf.left(nl)).object();
                out_buf.remove(0, nl + 1);
                if (o.value(QStringLiteral("id")).toInt(-1) == id)
                    return o;
            }
            proc.waitForReadyRead(50);
            out_buf += proc.readAllStandardOutput();
        }
        return QJsonObject{};
    };
    // A message split across two writes (partial frame).
    const QByteArray init = rpc(1, QStringLiteral("initialize"), init_params(QStringLiteral("claude-ai")));
    proc.write(init.left(10));
    proc.waitForBytesWritten(1000);
    QThread::msleep(100);
    proc.write(init.mid(10) + "\n");
    QJsonObject m = read_message(1);
    QCOMPARE(m.value(QStringLiteral("result")).toObject().value(QStringLiteral("serverInfo")).toObject().value(QStringLiteral("name")).toString(),
             QStringLiteral("fincept-terminal"));
    proc.write(note(QStringLiteral("notifications/initialized")) + "\n");
    // A large frame (1 MB argument).
    proc.write(rpc(2, QStringLiteral("tools/call"),
                   call_params(QStringLiteral("echo"), {{QStringLiteral("ref"), QString(1024 * 1024, QLatin1Char('z'))}})) +
               "\n");
    m = read_message(2);
    QVERIFY(first_text(m).size() > 1024 * 1024);
    // Progress through the bridge, streamed.
    proc.write(rpc(3, QStringLiteral("tools/call"), call_params(QStringLiteral("progress"), {}, 99)) + "\n");
    m = read_message(3);
    QCOMPARE(first_text(m), QStringLiteral("progress done"));
    // Every bridge slot held by a slow call, plus one more request queued behind
    // them: a cancel still gets through at once (notifications never wait).
    QElapsedTimer held;
    held.start();
    for (int i = 0; i < 16; ++i)
        proc.write(rpc(100 + i, QStringLiteral("tools/call"), call_params(QStringLiteral("slow"), {{QStringLiteral("ms"), 9000}})) + "\n");
    proc.write(rpc(200, QStringLiteral("ping")) + "\n"); // queued behind the full slots
    QTRY_VERIFY_WITH_TIMEOUT(s.backend->started.load() >= 18, 8000); // echo + progress + 16 slow
    proc.write(note(QStringLiteral("notifications/cancelled"), {{QStringLiteral("requestId"), 105}}) + "\n");
    m = read_message(105, 6000);
    QVERIFY(first_text(m).contains(QStringLiteral("Cancelled")));
    QVERIFY2(held.elapsed() < 8000, "the cancel waited behind the slow calls");
    for (int i = 0; i < 16; ++i)
        proc.write(note(QStringLiteral("notifications/cancelled"), {{QStringLiteral("requestId"), 100 + i}}) + "\n");
    m = read_message(200, 10000); // the queued ping runs once a slot frees
    QVERIFY(m.contains(QStringLiteral("result")));
    // The terminal goes away: requests fail fast with -32000, no hang.
    s.stop();
    QElapsedTimer t;
    t.start();
    proc.write(rpc(4, QStringLiteral("ping")) + "\n");
    m = read_message(4, 12000);
    QCOMPARE(error_code(m), -32000);
    QVERIFY(t.elapsed() < 8000);
    // ...and comes back on the same port: the bridge just works again (stateless fallback).
    EngineLimits l = Server::test_limits();
    Server again(l, s.port);
    if (again.port != 0) {
        proc.write(rpc(5, QStringLiteral("ping")) + "\n");
        m = read_message(5);
        QVERIFY(m.contains(QStringLiteral("result")));
    }
    // stdin closes → the bridge exits.
    proc.closeWriteChannel();
    QVERIFY(proc.waitForFinished(10000));
    QCOMPARE(proc.exitCode(), 0);
    // Nothing but protocol lines ever reached stdout.
    out_buf += proc.readAllStandardOutput();
    for (const QByteArray& line : out_buf.split('\n')) {
        if (!line.trimmed().isEmpty())
            QVERIFY2(QJsonDocument::fromJson(line).isObject() || QJsonDocument::fromJson(line).isArray(), line.left(80).constData());
    }
}

// ── edition co-existence ─────────────────────────────────────────────────────

void TestMcpServerEngine::health_and_server_info_report_the_edition() {
    Server s;
    const Reply h = http(s.port, "GET", "/health", {});
    QCOMPARE(h.status, 200);
    QCOMPARE(h.json().object().value(QStringLiteral("edition")).toString(), QStringLiteral("community"));
    QCOMPARE(s.backend->bearer_count(), 0); // asked without a token
    const Reply r = http(s.port, "POST", "/mcp", mcp_headers(),
                         rpc(1, QStringLiteral("initialize"), init_params(QStringLiteral("claude-code"))));
    QCOMPARE(r.json()
                 .object()
                 .value(QStringLiteral("result"))
                 .toObject()
                 .value(QStringLiteral("serverInfo"))
                 .toObject()
                 .value(QStringLiteral("edition"))
                 .toString(),
             QStringLiteral("community"));
}

void TestMcpServerEngine::forbidden_ports_are_never_bound() {
    // A free port to play the part of the other edition's 47821.
    quint16 p = 0;
    {
        QTcpServer probe;
        QVERIFY(probe.listen(QHostAddress::LocalHost, 0));
        p = static_cast<quint16>(probe.serverPort());
    }
    EngineLimits l = Server::test_limits();
    l.forbidden_ports = {p};
    {
        Server only(l, p, 0); // that port only: refused, never bound
        QCOMPARE(only.port, quint16(0));
    }
    {
        Server fallback(l, p, 3); // the scan skips it
        QVERIFY(fallback.port != 0);
        QVERIFY(fallback.port != p);
        QVERIFY(fallback.port > p && fallback.port <= p + 3);
    }
    {
        EngineLimits any = Server::test_limits();
        any.forbidden_ports = {p};
        Server os(any, 0, 0); // OS-assigned: never lands on a reserved port
        QVERIFY(os.port != 0 && os.port != p);
    }
}

void TestMcpServerEngine::self_check_passes_every_profile() {
    Server s;
    SelfCheckOptions o;
    o.url = QStringLiteral("http://127.0.0.1:%1/mcp").arg(s.port);
    o.token = QString::fromLatin1(kToken);
    o.timeout_ms = 8000;
    const SelfCheckReport report = run_mcp_self_check(o);
    QVERIFY2(report.ok, qPrintable(report.lines().join(QLatin1Char('\n'))));
    QStringList profiles_seen;
    for (const SelfCheckStep& st : report.steps) {
        if (!profiles_seen.contains(st.profile))
            profiles_seen << st.profile;
    }
    // edition gate, the 2026-07-28 fallback, then every client profile.
    QCOMPARE(profiles_seen.value(0), QStringLiteral("server"));
    QVERIFY(profiles_seen.contains(QStringLiteral("protocol")));
    for (const profiles::ClientKind k : profiles::all_kinds())
        QVERIFY2(profiles_seen.contains(QLatin1String(profiles::profile(k).id)), profiles::profile(k).id);
}

void TestMcpServerEngine::self_check_refuses_another_edition() {
    Server s;
    s.backend->set_edition(QStringLiteral("private"));
    SelfCheckOptions o;
    o.url = QStringLiteral("http://127.0.0.1:%1/mcp").arg(s.port);
    o.token = QString::fromLatin1(kToken);
    o.timeout_ms = 5000;
    const SelfCheckReport report = run_mcp_self_check(o);
    QVERIFY(!report.ok);
    QCOMPARE(report.steps.size(), 1); // stopped at the edition gate
    QVERIFY(report.steps.at(0).detail.contains(QStringLiteral("private edition")));
    QVERIFY(report.steps.at(0).detail.contains(QStringLiteral("no token was sent")));
    QCOMPARE(s.backend->bearer_count(), 0); // and indeed none reached the server
}

void TestMcpServerEngine::stdio_bridge_refuses_another_edition() {
    QString python = qEnvironmentVariable("FINCEPT_TEST_PYTHON");
    if (python.isEmpty())
        python = QStandardPaths::findExecutable(QStringLiteral("python"));
    const QString script =
        QFileInfo(QString::fromUtf8(FINCEPT_SOURCE_DIR) + QStringLiteral("/scripts/fincept_mcp/fincept_mcp_stdio.py"))
            .absoluteFilePath();
    if (python.isEmpty() || QString::fromUtf8(FINCEPT_SOURCE_DIR).isEmpty() || !QFileInfo::exists(script))
        QSKIP("no Python interpreter or bridge script");
    auto run_once = [&](Server& s, const QProcessEnvironment& env) {
        QProcess proc;
        proc.setProcessEnvironment(env);
        proc.start(python, {QStringLiteral("-I"), script, QStringLiteral("--url"),
                            QStringLiteral("http://127.0.0.1:%1/mcp").arg(s.port)});
        if (!proc.waitForStarted(10000))
            return QJsonObject{};
        proc.write(rpc(1, QStringLiteral("initialize"), init_params(QStringLiteral("claude-ai"))) + "\n");
        QByteArray out;
        QElapsedTimer t;
        t.start();
        QJsonObject answer;
        while (t.elapsed() < 15000 && answer.isEmpty()) {
            proc.waitForReadyRead(100);
            out += proc.readAllStandardOutput();
            const qsizetype nl = out.indexOf('\n');
            if (nl >= 0)
                answer = QJsonDocument::fromJson(out.left(nl)).object();
        }
        proc.closeWriteChannel();
        if (!proc.waitForFinished(8000))
            proc.kill();
        return answer;
    };
    QProcessEnvironment base = QProcessEnvironment::systemEnvironment();
    base.remove(QStringLiteral("FINCEPT_COMMUNITY_MCP_TOKEN"));
    base.remove(QStringLiteral("FINCEPT_MCP_TOKEN"));

    // 1. The other edition answers on the port: no token is sent, a plain error comes back.
    {
        Server s;
        s.backend->set_edition(QStringLiteral("private"));
        QProcessEnvironment env = base;
        env.insert(QStringLiteral("FINCEPT_COMMUNITY_MCP_TOKEN"), QString::fromLatin1(kToken));
        const QJsonObject m = run_once(s, env);
        QCOMPARE(error_code(m), -32000);
        QVERIFY2(m.value(QStringLiteral("error")).toObject().value(QStringLiteral("message")).toString().contains(
                     QStringLiteral("private edition")),
                 qPrintable(QString::fromUtf8(QJsonDocument(m).toJson(QJsonDocument::Compact))));
        QCOMPARE(s.backend->bearer_count(), 0);
    }
    // 2. Only the shared / private variable is set: the bridge never reads it.
    {
        Server s;
        QProcessEnvironment env = base;
        env.insert(QStringLiteral("FINCEPT_MCP_TOKEN"), QStringLiteral("private-build-token"));
        const QJsonObject m = run_once(s, env);
        QCOMPARE(error_code(m), -32000); // 401: no token at all
        QVERIFY(!s.backend->saw_bearer("private-build-token"));
    }
}

QTEST_GUILESS_MAIN(TestMcpServerEngine)
#include "tst_mcp_server_engine.moc"
