#include "services/akshare/AkShareService.h"

#include "core/logging/Logger.h"
#include "python/PythonRunner.h"
#include "services/akshare/AkShareDecode.h"

#include <QFuture>
#include <QPointer>
#include <QThreadPool>
#include <QtConcurrent/QtConcurrentRun>

#include <utility>

namespace {

fincept::python::PythonRunner::RunOptions akshare_svc_options(const fincept::services::akshare::CancelToken& cancel) {
    fincept::python::PythonRunner::RunOptions opts;
    opts.cancel_token = cancel;
    return opts;
}

bool akshare_svc_cancelled(const fincept::services::akshare::CancelToken& cancel) {
    return cancel && cancel->load();
}

/// The decode thread: the script's JSON (a long history is megabytes) is parsed and shaped here,
/// never on the GUI thread. One thread, so answers come back in the order the scripts finished. A
/// child of the process-lifetime service: its destructor waits for a decode still in flight.
QThreadPool* akshare_svc_decode_pool() {
    static QThreadPool* const pool = [] {
        auto* p = new QThreadPool(&fincept::services::akshare::AkShareService::instance());
        p->setObjectName(QStringLiteral("AkShareDecode"));
        p->setMaxThreadCount(1);
        return p;
    }();
    return pool;
}

} // namespace

namespace fincept::services::akshare {

AkShareService& AkShareService::instance() {
    static AkShareService s;
    return s;
}

void AkShareService::fetch_endpoints(const QString& script, EndpointsCallback cb, CancelToken cancel) {
    fincept::python::PythonRunner::instance().run_with_options(
        script, {QStringLiteral("get_all_endpoints")}, akshare_svc_options(cancel),
        [this, cb, script, cancel](fincept::python::PythonResult py) {
            if (akshare_svc_cancelled(cancel)) {
                EndpointsResult out;
                out.cancelled = true;
                out.error = QStringLiteral("Cancelled");
                if (cb)
                    cb(out);
                return;
            }
            const bool run_failed = !py.success;
            // GUI thread: only the hand-over. The decode thread parses; the answer comes back here.
            (void)QtConcurrent::run(akshare_svc_decode_pool(),
                                    [py = std::move(py), cancel]() {
                                        if (akshare_svc_cancelled(cancel))
                                            return EndpointsResult{}; // replaced by "Cancelled" below
                                        return decode_endpoints(py);
                                    })
                .then(this, [self = QPointer<AkShareService>(this), cb, script, cancel,
                             run_failed](EndpointsResult r) {
                    if (!self)
                        return;
                    if (akshare_svc_cancelled(cancel)) {
                        r = EndpointsResult{};
                        r.cancelled = true;
                        r.error = QStringLiteral("Cancelled");
                    } else if (run_failed && !r.success) {
                        LOG_ERROR("AkShareService",
                                  QStringLiteral("%1 get_all_endpoints failed: %2").arg(script, r.error.left(300)));
                    }
                    if (cb)
                        cb(r);
                });
        });
}

void AkShareService::query(const QString& script, const QString& endpoint, const QStringList& extra_args,
                           QueryCallback cb, CancelToken cancel) {
    QStringList args;
    args << endpoint << extra_args;

    fincept::python::PythonRunner::instance().run_with_options(
        script, args, akshare_svc_options(cancel),
        [this, cb, script, endpoint, cancel](fincept::python::PythonResult py) {
            if (akshare_svc_cancelled(cancel)) {
                QueryResult out;
                out.cancelled = true;
                out.error = QStringLiteral("Cancelled");
                if (cb)
                    cb(out);
                return;
            }
            const bool run_failed = !py.success;
            // GUI thread: only the hand-over. The decode thread parses the JSON, shapes rows / columns
            // and estimates their size; the answer comes back here.
            (void)QtConcurrent::run(akshare_svc_decode_pool(),
                                    [py = std::move(py), endpoint, cancel]() {
                                        if (akshare_svc_cancelled(cancel))
                                            return QueryResult{}; // replaced by "Cancelled" below
                                        return decode_query(py, endpoint);
                                    })
                .then(this, [self = QPointer<AkShareService>(this), cb, script, endpoint, cancel,
                             run_failed](QueryResult r) {
                    if (!self)
                        return;
                    if (akshare_svc_cancelled(cancel)) {
                        // Stale by now: the decoded rows are dropped, the caller sees a cancelled run.
                        r = QueryResult{};
                        r.cancelled = true;
                        r.error = QStringLiteral("Cancelled");
                    } else if (run_failed && !r.success) {
                        LOG_ERROR("AkShareService", logf("AkShare query failed", {{"script", script},
                                                                                  {"endpoint", endpoint},
                                                                                  {"type", r.error_type},
                                                                                  {"error", r.error.left(300)}}));
                    }
                    if (cb)
                        cb(r);
                });
        });
}

} // namespace fincept::services::akshare
