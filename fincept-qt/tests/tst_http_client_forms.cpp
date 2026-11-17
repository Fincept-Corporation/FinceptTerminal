// tst_http_client_forms — src/network/http/HttpClient.{h,cpp}: the opt-in forms of the
// write verbs, end to end against a loopback HTTP server (no outside network).
//   put_large / patch_large / del_large (both del forms): the request is the plain verb's
//     (same method, same body bytes), the answer equals the plain verb's (documents, error
//     strings), it arrives on the context's thread, and never once the context is gone.
//   post_raw / put_raw / patch_raw / del_raw: a 2xx body as it arrived ("ok", 204 empty)
//     with its status in response_status() and its headers in response_header(); an
//     HTTP error is the plain verb's err("HTTP_<status>: …") with the status and headers
//     still readable; a transport failure is err("Network error: …") with status 0.
//   The JsonCallback forms still report a non-JSON 2xx as a parse error (unchanged).
//   Fincept API bookkeeping for the new forms: the 401 session re-send and the
//     session_token_rejected / api_key_rejected / credit_balance_reported /
//     ip_block_changed signals.
// tst_http_off_thread covers the worker hand-over itself (HttpOffThread.h, QtCore only);
// this suite needs QtNetwork and HttpClient.cpp + Logger.cpp. Its two telemetry calls are
// stubbed below (ClientInfo / UsageReporter would pull in the app).
#include "network/http/HttpClient.h"

#include "core/telemetry/ClientInfo.h"
#include "core/telemetry/UsageReporter.h"

#include <QHash>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QPointer>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include <QTimer>
#include <QtTest/QtTest>

#include <functional>
#include <memory>

// ── Stubs for the app's telemetry (only reached for Fincept API URLs / failures) ──
namespace fincept::telemetry {
void ClientInfo::apply_headers(QNetworkRequest&) {}
void UsageReporter::note_http_error(const QUrl&, int, int, const QString&) {}
} // namespace fincept::telemetry

using fincept::HttpClient;
using fincept::Result;

namespace {

/// One request as the loopback server saw it.
struct SeenRequest {
    QByteArray method;
    QByteArray path;
    QByteArray body;
    QHash<QByteArray, QByteArray> headers; ///< lower-case names
};

/// A minimal HTTP/1.1 server: one request per connection, answered by path, then closed.
class LoopbackServer : public QObject {
  public:
    bool start() {
        connect(&server_, &QTcpServer::newConnection, this, [this]() { accept_all(); });
        return server_.listen(QHostAddress::LocalHost, 0);
    }
    QString url(const char* path) const {
        return QStringLiteral("http://127.0.0.1:%1%2").arg(server_.serverPort()).arg(QLatin1String(path));
    }
    QString base() const { return QStringLiteral("http://127.0.0.1:%1").arg(server_.serverPort()); }
    int count(const QByteArray& path) const {
        int n = 0;
        for (const auto& r : seen)
            n += r.path == path ? 1 : 0;
        return n;
    }
    QList<SeenRequest> seen;

  private:
    void accept_all() {
        while (QTcpSocket* s = server_.nextPendingConnection()) {
            auto buf = std::make_shared<QByteArray>();
            connect(s, &QTcpSocket::disconnected, s, &QObject::deleteLater);
            connect(s, &QTcpSocket::readyRead, this, [this, s, buf]() {
                *buf += s->readAll();
                const qsizetype end = buf->indexOf("\r\n\r\n");
                if (end < 0)
                    return;
                SeenRequest r;
                const QList<QByteArray> lines = buf->left(end).split('\n');
                const QList<QByteArray> start = lines.value(0).trimmed().split(' ');
                r.method = start.value(0);
                r.path = start.value(1);
                if (const qsizetype q = r.path.indexOf('?'); q >= 0)
                    r.path.truncate(q);
                qsizetype length = 0;
                for (qsizetype i = 1; i < lines.size(); ++i) {
                    const qsizetype colon = lines[i].indexOf(':');
                    if (colon <= 0)
                        continue;
                    const QByteArray name = lines[i].left(colon).trimmed().toLower();
                    r.headers.insert(name, lines[i].mid(colon + 1).trimmed());
                    if (name == "content-length")
                        length = lines[i].mid(colon + 1).trimmed().toLongLong();
                }
                if (buf->size() < end + 4 + length)
                    return; // the body is still coming
                r.body = buf->mid(end + 4, length);
                buf->clear();
                seen.append(r);
                answer(s, r);
            });
        }
    }

    static QByteArray response(int status, const QByteArray& body, const QByteArray& type = "application/json",
                               const QList<QPair<QByteArray, QByteArray>>& extra = {}) {
        QByteArray out = "HTTP/1.1 " + QByteArray::number(status) + " X\r\n";
        if (status != 204)
            out += "Content-Type: " + type + "\r\nContent-Length: " + QByteArray::number(body.size()) + "\r\n";
        for (const auto& h : extra)
            out += h.first + ": " + h.second + "\r\n";
        out += "Connection: close\r\n\r\n";
        if (status != 204)
            out += body;
        return out;
    }

    static QByteArray echo(const SeenRequest& r) {
        QJsonObject o;
        o["method"] = QString::fromLatin1(r.method);
        o["path"] = QString::fromLatin1(r.path);
        o["body"] = QString::fromUtf8(r.body);
        o["token"] = r.headers.contains("x-session-token");
        o["key"] = QString::fromUtf8(r.headers.value("x-api-key"));
        o["type"] = QString::fromUtf8(r.headers.value("content-type"));
        return QJsonDocument(o).toJson(QJsonDocument::Compact);
    }

    static void send(QTcpSocket* s, const QByteArray& bytes) {
        s->write(bytes);
        s->disconnectFromHost();
    }

    void answer(QTcpSocket* s, const SeenRequest& r) {
        if (r.path == "/echo") {
            send(s, response(200, echo(r)));
        } else if (r.path == "/text") {
            send(s, response(200, "ok", "text/plain"));
        } else if (r.path == "/empty") {
            send(s, response(204, {}));
        } else if (r.path == "/fail") {
            send(s, response(500, R"({"message":"boom"})"));
        } else if (r.path == "/slow") {
            const QByteArray bytes = response(200, echo(r));
            QPointer<QTcpSocket> guard(s);
            QTimer::singleShot(400, this, [guard, bytes]() {
                if (guard)
                    send(guard, bytes);
            });
        } else if (r.path == "/session") {
            if (r.headers.contains("x-session-token"))
                send(s, response(401, R"({"message":{"error":"session_invalidated","message":"Signed in elsewhere"}})",
                                 "application/json", {{"X-Session-Expired", "true"}}));
            else
                send(s, response(200, echo(r)));
        } else if (r.path == "/badkey") {
            send(s, response(401, R"({"message":"Invalid API key"})"));
        } else if (r.path == "/billed") {
            send(s, response(200, echo(r), "application/json", {{"X-Credits-Cost", "2"}, {"X-Credits-Balance", "41.5"}}));
        } else if (r.path == "/blocked") {
            send(s, response(403, R"({"error_code":"IP_BLOCKED","message":"Too many failed attempts."})"));
        } else {
            send(s, response(404, R"({"message":"no route"})"));
        }
    }

    QTcpServer server_;
};

/// What one JsonCallback received, plus what the client published while it ran.
struct JsonOutcome {
    bool done = false;
    bool ok = false;
    QJsonDocument doc;
    std::string error;
    int status = 0;
    QThread* thread = nullptr;
};

/// The same for a BytesCallback.
struct RawOutcome {
    bool done = false;
    bool ok = false;
    QByteArray body;
    std::string error;
    int status = 0;
    QByteArray content_type;
    QThread* thread = nullptr;
};

HttpClient::JsonCallback record(std::shared_ptr<JsonOutcome> out) {
    return [out](Result<QJsonDocument> r) {
        out->ok = r.is_ok();
        if (r.is_ok())
            out->doc = r.value();
        else
            out->error = r.error();
        out->status = HttpClient::response_status();
        out->thread = QThread::currentThread();
        out->done = true;
    };
}

HttpClient::BytesCallback record_raw(std::shared_ptr<RawOutcome> out) {
    return [out](Result<QByteArray> r) {
        out->ok = r.is_ok();
        if (r.is_ok())
            out->body = r.value();
        else
            out->error = r.error();
        out->status = HttpClient::response_status();
        out->content_type = HttpClient::response_header("Content-Type");
        out->thread = QThread::currentThread();
        out->done = true;
    };
}

QJsonObject sample_body() {
    return QJsonObject{{"symbol", "RELIANCE"}, {"qty", 10}, {"note", "ünïcödé"}};
}

} // namespace

class TestHttpClientForms : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase();
    void cleanup();

    void large_forms_send_what_the_plain_verbs_send();
    void large_forms_answer_like_the_plain_verbs();
    void destroyed_context_gets_no_callback();
    void raw_forms_hand_over_the_body();
    void transport_failure_is_an_error_without_status();
    void json_forms_still_report_a_non_json_2xx_as_parse_error();
    void bad_url_is_refused_asynchronously();
    void session_resend_and_signals_for_the_new_forms();

  private:
    LoopbackServer server_;
};

void TestHttpClientForms::initTestCase() {
    QVERIFY(server_.start());
}

void TestHttpClientForms::cleanup() {
    auto& http = HttpClient::instance();
    http.set_base_url(QString());
    http.set_auth_header(QString());
    http.clear_session_token();
}

void TestHttpClientForms::large_forms_send_what_the_plain_verbs_send() {
    auto& http = HttpClient::instance();
    QObject context;
    const QJsonObject body = sample_body();
    struct Pair {
        const char* name;
        std::function<void(HttpClient::JsonCallback)> plain;
        std::function<void(HttpClient::JsonCallback)> large;
        const char* method;
    };
    const QString url = server_.url("/echo");
    const QList<Pair> pairs = {
        {"put", [&](auto cb) { http.put(url, body, cb, &context); },
         [&](auto cb) { http.put_large(url, body, cb, &context); }, "PUT"},
        {"patch", [&](auto cb) { http.patch(url, body, cb, &context); },
         [&](auto cb) { http.patch_large(url, body, cb, &context); }, "PATCH"},
        {"del", [&](auto cb) { http.del(url, cb, &context); }, [&](auto cb) { http.del_large(url, cb, &context); },
         "DELETE"},
        {"del+body", [&](auto cb) { http.del(url, body, cb, &context); },
         [&](auto cb) { http.del_large(url, body, cb, &context); }, "DELETE"},
    };
    for (const auto& p : pairs) {
        auto plain = std::make_shared<JsonOutcome>();
        auto large = std::make_shared<JsonOutcome>();
        p.plain(record(plain));
        p.large(record(large));
        QTRY_VERIFY_WITH_TIMEOUT(plain->done && large->done, 10000);
        QVERIFY2(plain->ok && large->ok, p.name);
        const QJsonObject a = plain->doc.object();
        const QJsonObject b = large->doc.object();
        QCOMPARE(b.value("method").toString(), QString::fromLatin1(p.method));
        // Same request bytes as the plain verb: the echoed body is identical.
        QCOMPARE(b.value("body").toString(), a.value("body").toString());
        QCOMPARE(large->doc, plain->doc);
        QCOMPARE(large->status, 200);
        QCOMPARE(large->thread, QThread::currentThread());
    }
}

void TestHttpClientForms::large_forms_answer_like_the_plain_verbs() {
    auto& http = HttpClient::instance();
    QObject context;
    const QJsonObject body = sample_body();
    for (const char* path : {"/fail", "/text", "/empty", "/nowhere"}) {
        const QString url = server_.url(path);
        auto plain = std::make_shared<JsonOutcome>();
        auto large = std::make_shared<JsonOutcome>();
        http.put(url, body, record(plain), &context);
        http.put_large(url, body, record(large), &context);
        QTRY_VERIFY_WITH_TIMEOUT(plain->done && large->done, 10000);
        QVERIFY2(!plain->ok && !large->ok, path);
        QCOMPARE(large->error, plain->error); // "HTTP_500: boom", the parse text, "HTTP_404: no route"
        QCOMPARE(large->status, plain->status);
    }
    // The exact strings the callers switch on.
    auto fail = std::make_shared<JsonOutcome>();
    http.patch_large(server_.url("/fail"), body, record(fail), &context);
    QTRY_VERIFY_WITH_TIMEOUT(fail->done, 10000);
    QCOMPARE(QString::fromStdString(fail->error), QStringLiteral("HTTP_500: boom"));
    QCOMPARE(HttpClient::status_from_error(fail->error), 500);
    QCOMPARE(fail->status, 500);
}

void TestHttpClientForms::destroyed_context_gets_no_callback() {
    auto& http = HttpClient::instance();
    const int before = server_.count("/slow");
    bool called = false;
    {
        auto* context = new QObject;
        http.put_large(server_.url("/slow"), sample_body(), [&called](Result<QJsonDocument>) { called = true; },
                       context);
        http.patch_large(server_.url("/slow"), sample_body(), [&called](Result<QJsonDocument>) { called = true; },
                         context);
        http.del_large(server_.url("/slow"), [&called](Result<QJsonDocument>) { called = true; }, context);
        http.post_raw(server_.url("/slow"), sample_body(), [&called](Result<QByteArray>) { called = true; },
                      context);
        http.del_raw(server_.url("/slow"), [&called](Result<QByteArray>) { called = true; }, context);
        QTRY_COMPARE_WITH_TIMEOUT(server_.count("/slow"), before + 5, 10000); // all five are in flight
        delete context; // the run / panel went away while the answers were on their way
    }
    QTest::qWait(1200); // the answers arrive (400 ms) and are dropped
    QVERIFY(!called);
}

void TestHttpClientForms::raw_forms_hand_over_the_body() {
    auto& http = HttpClient::instance();
    QObject context;
    const QJsonObject body = sample_body();

    // Slack / Mattermost: 200 "ok".
    auto text = std::make_shared<RawOutcome>();
    http.post_raw(server_.url("/text"), body, record_raw(text), &context);
    // Discord: 204, no body.
    auto empty = std::make_shared<RawOutcome>();
    http.del_raw(server_.url("/empty"), record_raw(empty), &context);
    // JSON stays bytes: the caller decodes where it likes (HttpClient::parse_body).
    auto put = std::make_shared<RawOutcome>();
    http.put_raw(server_.url("/echo"), body, record_raw(put), &context);
    auto patch = std::make_shared<RawOutcome>();
    http.patch_raw(server_.url("/echo"), body, record_raw(patch), &context);
    auto del_body = std::make_shared<RawOutcome>();
    http.del_raw(server_.url("/echo"), body, record_raw(del_body), &context);
    // An HTTP error: the plain verb's error string, the status and headers still readable.
    auto fail = std::make_shared<RawOutcome>();
    http.post_raw(server_.url("/fail"), body, record_raw(fail), &context);
    auto missing = std::make_shared<RawOutcome>();
    http.put_raw(server_.url("/nowhere"), body, record_raw(missing), &context);
    auto plain_fail = std::make_shared<JsonOutcome>();
    http.post(server_.url("/fail"), body, record(plain_fail), &context);

    QTRY_VERIFY_WITH_TIMEOUT(text->done && empty->done && put->done && patch->done && del_body->done && fail->done &&
                                 missing->done && plain_fail->done,
                             10000);
    QVERIFY(text->ok);
    QCOMPARE(text->body, QByteArray("ok"));
    QCOMPARE(text->status, 200);
    QCOMPARE(text->content_type, QByteArray("text/plain"));
    QCOMPARE(text->thread, QThread::currentThread());

    QVERIFY(empty->ok);
    QVERIFY(empty->body.isEmpty());
    QCOMPARE(empty->status, 204);

    for (const auto& [o, method] : {std::pair{put, "PUT"}, std::pair{patch, "PATCH"}, std::pair{del_body, "DELETE"}}) {
        QVERIFY(o->ok);
        QCOMPARE(o->status, 200);
        const auto parsed = HttpClient::parse_body(o->body);
        QVERIFY(parsed.is_ok());
        QCOMPARE(parsed.value().object().value("method").toString(), QString::fromLatin1(method));
        QCOMPARE(QJsonDocument::fromJson(parsed.value().object().value("body").toString().toUtf8()).object(), body);
    }

    QVERIFY(!fail->ok);
    QCOMPARE(fail->error, plain_fail->error);
    QCOMPARE(QString::fromStdString(fail->error), QStringLiteral("HTTP_500: boom"));
    QCOMPARE(fail->status, 500);
    QCOMPARE(fail->content_type, QByteArray("application/json"));
    QVERIFY(!missing->ok);
    QCOMPARE(QString::fromStdString(missing->error), QStringLiteral("HTTP_404: no route"));
    QCOMPARE(missing->status, 404);
}

void TestHttpClientForms::transport_failure_is_an_error_without_status() {
    // A port nobody listens on: connection refused, no response at all.
    QTcpServer probe;
    QVERIFY(probe.listen(QHostAddress::LocalHost, 0));
    const quint16 dead_port = probe.serverPort();
    probe.close();
    const QString url = QStringLiteral("http://127.0.0.1:%1/hook").arg(dead_port);

    auto& http = HttpClient::instance();
    QObject context;
    auto raw = std::make_shared<RawOutcome>();
    auto plain = std::make_shared<JsonOutcome>();
    http.post_raw(url, sample_body(), record_raw(raw), &context);
    http.post(url, sample_body(), record(plain), &context);
    QTRY_VERIFY_WITH_TIMEOUT(raw->done && plain->done, 15000);
    QVERIFY(!raw->ok);
    QVERIFY(QString::fromStdString(raw->error).startsWith(QLatin1String("Network error: ")));
    QCOMPARE(raw->error, plain->error);
    QCOMPARE(raw->status, 0);
    QCOMPARE(HttpClient::status_from_error(raw->error), 0);
}

void TestHttpClientForms::json_forms_still_report_a_non_json_2xx_as_parse_error() {
    // Unchanged on purpose: what a JsonCallback caller sees for "ok" / an empty 204. The
    // answer did succeed (response_status() is the 2xx, status_from_error() is 0) but the
    // result is the JSON parse text — the raw forms above are the way to take such bodies.
    auto& http = HttpClient::instance();
    QObject context;
    auto text = std::make_shared<JsonOutcome>();
    http.post(server_.url("/text"), sample_body(), record(text), &context);
    auto empty = std::make_shared<JsonOutcome>();
    http.del(server_.url("/empty"), record(empty), &context);
    QTRY_VERIFY_WITH_TIMEOUT(text->done && empty->done, 10000);
    QVERIFY(!text->ok);
    QCOMPARE(text->status, 200);
    QCOMPARE(HttpClient::status_from_error(text->error), 0);
    QJsonParseError err;
    (void)QJsonDocument::fromJson("ok", &err);
    QCOMPARE(QString::fromStdString(text->error), err.errorString());
    QVERIFY(!empty->ok);
    QCOMPARE(empty->status, 204);
    QCOMPARE(HttpClient::status_from_error(empty->error), 0);
}

void TestHttpClientForms::bad_url_is_refused_asynchronously() {
    auto& http = HttpClient::instance();
    QObject context;
    auto large = std::make_shared<JsonOutcome>();
    auto raw = std::make_shared<RawOutcome>();
    // Scheme-less third-party targets must never be resolved against the Fincept API.
    http.put_large(QStringLiteral("hooks.example.com/services/x"), sample_body(), record(large), &context);
    http.post_raw(QStringLiteral("hooks.example.com/services/x"), sample_body(), record_raw(raw), &context);
    QVERIFY(!large->done && !raw->done); // never re-entrant
    QTRY_VERIFY_WITH_TIMEOUT(large->done && raw->done, 5000);
    QVERIFY(!large->ok && !raw->ok);
    QVERIFY(QString::fromStdString(large->error).startsWith(QLatin1String("Invalid URL")));
    QCOMPARE(raw->error, large->error);
}

void TestHttpClientForms::session_resend_and_signals_for_the_new_forms() {
    auto& http = HttpClient::instance();
    http.set_base_url(server_.base()); // 127.0.0.1 is "the Fincept API" for this test
    http.set_auth_header(QStringLiteral("key-1"));
    http.set_session_token(QStringLiteral("tok-1"));
    QSignalSpy session_spy(&http, &HttpClient::session_token_rejected);
    QSignalSpy key_spy(&http, &HttpClient::api_key_rejected);
    QSignalSpy credit_spy(&http, &HttpClient::credit_balance_reported);
    QSignalSpy block_spy(&http, &HttpClient::ip_block_changed);
    QObject context;

    // 401 session_invalidated: the token is dropped, the signal fires once, the request is
    // re-sent once without it and the caller only sees that second answer.
    const int sessions_before = server_.count("/session");
    auto large = std::make_shared<JsonOutcome>();
    http.put_large(QStringLiteral("/session"), sample_body(), record(large), &context);
    QTRY_VERIFY_WITH_TIMEOUT(large->done, 10000);
    QVERIFY2(large->ok, large->error.c_str());
    QCOMPARE(large->doc.object().value("token").toBool(), false);
    QCOMPARE(large->doc.object().value("key").toString(), QStringLiteral("key-1"));
    QCOMPARE(large->doc.object().value("method").toString(), QStringLiteral("PUT"));
    QCOMPARE(server_.count("/session"), sessions_before + 2);
    QCOMPARE(session_spy.count(), 1);
    QCOMPARE(session_spy.at(0).at(0).toString(), QStringLiteral("tok-1"));

    // The same for a raw form.
    http.set_session_token(QStringLiteral("tok-2"));
    auto raw = std::make_shared<RawOutcome>();
    http.del_raw(QStringLiteral("/session"), record_raw(raw), &context);
    QTRY_VERIFY_WITH_TIMEOUT(raw->done, 10000);
    QVERIFY2(raw->ok, raw->error.c_str());
    QCOMPARE(raw->status, 200);
    QCOMPARE(server_.count("/session"), sessions_before + 4);
    QCOMPARE(session_spy.count(), 2);
    QCOMPARE(session_spy.at(1).at(0).toString(), QStringLiteral("tok-2"));

    // Any other 401 for the current key: api_key_rejected, and the caller gets the error.
    auto badkey = std::make_shared<JsonOutcome>();
    http.patch_large(QStringLiteral("/badkey"), sample_body(), record(badkey), &context);
    QTRY_VERIFY_WITH_TIMEOUT(badkey->done, 10000);
    QCOMPARE(QString::fromStdString(badkey->error), QStringLiteral("HTTP_401: Invalid API key"));
    QCOMPARE(key_spy.count(), 1);
    auto badkey_raw = std::make_shared<RawOutcome>();
    http.post_raw(QStringLiteral("/badkey"), sample_body(), record_raw(badkey_raw), &context);
    QTRY_VERIFY_WITH_TIMEOUT(badkey_raw->done, 10000);
    QCOMPARE(QString::fromStdString(badkey_raw->error), QStringLiteral("HTTP_401: Invalid API key"));
    QCOMPARE(badkey_raw->status, 401);
    QCOMPARE(key_spy.count(), 2);

    // A billed answer carries the balance after the charge.
    auto billed = std::make_shared<JsonOutcome>();
    http.del_large(QStringLiteral("/billed"), record(billed), &context);
    QTRY_VERIFY_WITH_TIMEOUT(billed->done, 10000);
    QVERIFY(billed->ok);
    QCOMPARE(credit_spy.count(), 1);
    QCOMPARE(credit_spy.at(0).at(0).toDouble(), 41.5);
    auto billed_raw = std::make_shared<RawOutcome>();
    http.post_raw(QStringLiteral("/billed"), sample_body(), record_raw(billed_raw), &context);
    QTRY_VERIFY_WITH_TIMEOUT(billed_raw->done, 10000);
    QCOMPARE(credit_spy.count(), 2);

    // 403 IP_BLOCKED: never an account problem — ip_block_changed(true, …).
    auto blocked = std::make_shared<RawOutcome>();
    http.put_raw(QStringLiteral("/blocked"), sample_body(), record_raw(blocked), &context);
    QTRY_VERIFY_WITH_TIMEOUT(blocked->done, 10000);
    QVERIFY(!blocked->ok);
    QCOMPARE(HttpClient::status_from_error(blocked->error), 403);
    QCOMPARE(blocked->status, 403);
    QCOMPARE(block_spy.count(), 1);
    QCOMPARE(block_spy.at(0).at(0).toBool(), true);
    QVERIFY(http.is_ip_blocked());
}

QTEST_GUILESS_MAIN(TestHttpClientForms)
#include "tst_http_client_forms.moc"
