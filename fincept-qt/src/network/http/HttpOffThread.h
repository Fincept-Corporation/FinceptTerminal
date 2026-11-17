#pragma once
// HttpOffThread — the worker half of HttpClient's large-body path (get_large(), post_large(),
// put_large(), patch_large(), del_large()): turn a
// response body into JSON away from the GUI thread, then hand the result back on the caller's thread.
//
// QtCore only and header-only: HttpClient.cpp uses it, tests/tst_http_off_thread.cpp tests it alone.

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QThread>
#include <QThreadPool>

#include <functional>
#include <utility>

namespace fincept::http_off_thread {

/// A response body turned into JSON, classified the way HttpClient reports it.
struct ParsedBody {
    QJsonDocument doc;   ///< null unless `ok`
    bool ok = false;     ///< valid JSON
    bool empty = false;  ///< there was no body at all
    QString error;       ///< QJsonParseError::errorString() when not `ok` — the text get() reports
    qsizetype bytes = 0; ///< size of the body
};

/// QJsonDocument::fromJson plus that classification. Pure; any thread.
inline ParsedBody parse_body(const QByteArray& body) {
    ParsedBody out;
    out.bytes = body.size();
    out.empty = body.isEmpty();
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(body, &err);
    out.ok = err.error == QJsonParseError::NoError;
    if (out.ok)
        out.doc = std::move(doc);
    else
        out.error = err.errorString();
    return out;
}

/// What the worker hands back: run on the context's thread.
using Delivery = std::function<void()>;
/// The job: runs on a worker thread, returns the Delivery (empty = nothing to deliver).
using Work = std::function<Delivery()>;

/// Runs `work` on `pool`, then the Delivery it returned on `context`'s thread — unless `context` was
/// destroyed meanwhile: then nothing runs. Returns at once; never blocks the calling thread.
///
/// The worker never touches `context`: it posts to a small relay object that lives on the context's
/// thread and outlives the job, and whether `context` still exists is read there, on its own thread,
/// so a context destroyed at any moment is never called. `context` null: the Delivery runs on the
/// calling thread. The relay deletes itself after the delivery (it leaks only if that thread's event
/// loop never runs again, e.g. at exit).
inline void run(QThreadPool* pool, QObject* context, Work work) {
    auto* relay = new QObject;
    if (context && context->thread() != relay->thread())
        relay->moveToThread(context->thread());
    const bool guarded = context != nullptr;
    QPointer<QObject> guard(context);
    pool->start([relay, guard, guarded, work = std::move(work)]() mutable {
        Delivery deliver = work();
        work = nullptr; // release what the job captured (the raw body) before the hand-over
        QMetaObject::invokeMethod(
            relay,
            [relay, guard, guarded, deliver = std::move(deliver)]() {
                relay->deleteLater();
                if (guarded && !guard)
                    return;
                if (deliver)
                    deliver();
            },
            Qt::QueuedConnection);
    });
}

} // namespace fincept::http_off_thread
