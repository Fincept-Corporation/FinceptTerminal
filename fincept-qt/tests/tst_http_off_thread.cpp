// tst_http_off_thread — src/network/http/HttpOffThread.h, the worker half of HttpClient's
// large-body path (get_large / post_large): the body is parsed with get()'s classification, the
// parse runs off the calling thread, the result comes back on the context's thread, and never
// once the context has been destroyed.
#include "network/http/HttpOffThread.h"

#include <QAtomicInt>
#include <QElapsedTimer>
#include <QJsonObject>
#include <QSemaphore>
#include <QThread>
#include <QThreadPool>
#include <QtTest/QtTest>

#include <memory>

using namespace fincept::http_off_thread;

class TestHttpOffThread : public QObject {
    Q_OBJECT

  private slots:
    void parse_valid_object();
    void parse_invalid_uses_qt_error_text();
    void parse_empty_body();
    void delivers_on_context_thread_without_blocking();
    void destroyed_context_gets_no_delivery();
    void delivers_on_a_worker_threads_context();
    void null_context_delivers_on_calling_thread();
    void many_jobs_all_delivered();
    void empty_delivery_is_ignored();
};

void TestHttpOffThread::parse_valid_object() {
    const QByteArray body = R"({"datasets":{"num_found":106,"docs":[{"code":"WEO"}]}})";
    const ParsedBody p = parse_body(body);
    QVERIFY(p.ok);
    QVERIFY(!p.empty);
    QVERIFY(p.error.isEmpty());
    QVERIFY(p.doc.isObject());
    QCOMPARE(p.doc.object().value("datasets").toObject().value("num_found").toInt(), 106);
    QCOMPARE(p.bytes, body.size());
}

void TestHttpOffThread::parse_invalid_uses_qt_error_text() {
    const QByteArray body = "<html>502 Bad Gateway</html>";
    const ParsedBody p = parse_body(body);
    QVERIFY(!p.ok);
    QVERIFY(!p.empty);
    QVERIFY(p.doc.isNull());
    // HttpClient::get() reports exactly QJsonParseError::errorString() — the off-thread path must too.
    QJsonParseError err;
    (void)QJsonDocument::fromJson(body, &err);
    QCOMPARE(p.error, err.errorString());
    QCOMPARE(p.bytes, body.size());
}

void TestHttpOffThread::parse_empty_body() {
    const ParsedBody p = parse_body(QByteArray());
    QVERIFY(!p.ok);
    QVERIFY(p.empty);
    QCOMPARE(p.bytes, qsizetype(0));
    QVERIFY(!p.error.isEmpty());
}

void TestHttpOffThread::delivers_on_context_thread_without_blocking() {
    QThreadPool pool;
    QObject context;
    QSemaphore gate;
    QThread* worker_thread = nullptr;
    QThread* delivery_thread = nullptr;
    bool delivered = false;

    QElapsedTimer t;
    t.start();
    run(&pool, &context, [&]() -> Delivery {
        gate.acquire(); // the "parse" takes as long as the test wants
        worker_thread = QThread::currentThread();
        const ParsedBody p = parse_body(R"({"ok":true})");
        return [&, p]() {
            delivery_thread = QThread::currentThread();
            delivered = p.ok;
        };
    });
    // run() returned while the job is still blocked: the calling thread never waits for a parse.
    QVERIFY(t.elapsed() < 1000);
    QVERIFY(!delivered);
    gate.release();
    QTRY_VERIFY(delivered);
    QVERIFY(worker_thread != QThread::currentThread());
    QCOMPARE(delivery_thread, QThread::currentThread());
}

void TestHttpOffThread::destroyed_context_gets_no_delivery() {
    QThreadPool pool;
    auto* context = new QObject;
    QSemaphore gate;
    QAtomicInt work_ran = 0;
    bool delivered = false;

    run(&pool, context, [&]() -> Delivery {
        gate.acquire();
        work_ran.storeRelease(1);
        return [&]() { delivered = true; };
    });
    delete context; // the panel closed while its answer was being parsed
    gate.release();
    QTRY_VERIFY(work_ran.loadAcquire() == 1);
    pool.waitForDone();
    // Let the queued delivery (if any) be processed: it must find the context gone.
    QTest::qWait(100);
    QVERIFY(!delivered);
}

void TestHttpOffThread::delivers_on_a_worker_threads_context() {
    QThreadPool pool;
    QThread other;
    other.start();
    auto* context = new QObject;
    context->moveToThread(&other);
    QAtomicPointer<QThread> delivery_thread = nullptr;
    QAtomicInt delivered = 0;

    run(&pool, context, [&]() -> Delivery {
        return [&]() {
            delivery_thread.storeRelease(QThread::currentThread());
            delivered.storeRelease(1);
        };
    });
    QTRY_VERIFY(delivered.loadAcquire() == 1);
    QCOMPARE(delivery_thread.loadAcquire(), &other);

    connect(&other, &QThread::finished, context, &QObject::deleteLater);
    other.quit();
    QVERIFY(other.wait(5000));
}

void TestHttpOffThread::null_context_delivers_on_calling_thread() {
    QThreadPool pool;
    QThread* delivery_thread = nullptr;
    run(&pool, nullptr, [&]() -> Delivery { return [&]() { delivery_thread = QThread::currentThread(); }; });
    QTRY_VERIFY(delivery_thread != nullptr);
    QCOMPARE(delivery_thread, QThread::currentThread());
}

void TestHttpOffThread::many_jobs_all_delivered() {
    QThreadPool pool;
    pool.setMaxThreadCount(2);
    QObject context;
    int delivered = 0;
    const int jobs = 50;
    for (int i = 0; i < jobs; ++i) {
        const QByteArray body = QByteArray("{\"i\":") + QByteArray::number(i) + "}";
        run(&pool, &context, [&, body]() -> Delivery {
            const ParsedBody p = parse_body(body);
            return [&, p]() {
                if (p.ok)
                    ++delivered; // the context's thread only: no lock needed
            };
        });
    }
    QTRY_COMPARE(delivered, jobs);
}

void TestHttpOffThread::empty_delivery_is_ignored() {
    QThreadPool pool;
    QObject context;
    QAtomicInt ran = 0;
    run(&pool, &context, [&]() -> Delivery {
        ran.storeRelease(1);
        return {};
    });
    QTRY_VERIFY(ran.loadAcquire() == 1);
    pool.waitForDone();
    QTest::qWait(50); // the relay processes an empty delivery without calling anything
}

QTEST_GUILESS_MAIN(TestHttpOffThread)
#include "tst_http_off_thread.moc"
