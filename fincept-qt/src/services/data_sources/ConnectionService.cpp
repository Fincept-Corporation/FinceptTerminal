// ConnectionService.cpp — see ConnectionService.h.

#include "services/data_sources/ConnectionService.h"

#include "core/logging/Logger.h"
#include "storage/repositories/DataSourceRepository.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QMetaObject>
#include <QThreadPool>
#include <QtConcurrent/QtConcurrent>

namespace fincept::services::datasources {

namespace {

constexpr const char* kConnSvcLogTag = "DataSources";

QString cs_tr(const char* text) {
    return QCoreApplication::translate("ConnectionService", text);
}

const char* cs_op_name(SourceOp op) {
    switch (op) {
        case SourceOp::Test:
            return "test";
        case SourceOp::List:
            return "list";
        case SourceOp::Fetch:
            return "fetch";
    }
    return "?";
}

} // namespace

ConnectionService& ConnectionService::instance() {
    static ConnectionService s;
    return s;
}

ConnectionService::ConnectionService(QObject* parent) : QObject(parent) {
    pool_ = new QThreadPool(this);
    pool_->setObjectName(QStringLiteral("DataSourceConnections"));
    // Few at a time: each request may hold a socket / database connection for its whole timeout.
    pool_->setMaxThreadCount(4);
    pool_->setExpiryTimeout(60 * 1000);
}

quint64 ConnectionService::test_connection(const QString& connection_id, QObject* context, Callback cb) {
    SourceRequest req;
    req.op = SourceOp::Test;
    req.timeout_ms = 20000;
    return start(connection_id, req, true, context, std::move(cb));
}

quint64 ConnectionService::list_connection(const QString& connection_id, QObject* context, Callback cb, int limit) {
    SourceRequest req;
    req.op = SourceOp::List;
    req.limit = limit;
    return start(connection_id, req, false, context, std::move(cb));
}

quint64 ConnectionService::fetch_connection(const QString& connection_id, const QString& entity, const QString& query,
                                            int limit, QObject* context, Callback cb) {
    SourceRequest req;
    req.op = SourceOp::Fetch;
    req.entity = entity;
    req.query = query;
    req.limit = limit;
    req.timeout_ms = 60000;
    return start(connection_id, req, false, context, std::move(cb));
}

quint64 ConnectionService::run(const SourceRequest& request, QObject* context, Callback cb) {
    return start(QString(), request, false, context, std::move(cb));
}

void ConnectionService::cancel(quint64 job) {
    const auto it = jobs_.constFind(job);
    if (it != jobs_.constEnd() && it.value())
        it.value()->store(true);
}

quint64 ConnectionService::start(const QString& connection_id, SourceRequest request, bool persist_health,
                                 QObject* context, Callback cb) {
    const quint64 job = ++next_job_;
    CancelToken token = std::make_shared<std::atomic<bool>>(false);
    jobs_.insert(job, token);
    QPointer<QObject> ctx(context);
    QPointer<ConnectionService> self(this);
    (void)QtConcurrent::run(pool_, [self, ctx, job, token, connection_id, request, persist_health, cb]() mutable {
        SourceReply reply;
        QString provider = request.type_id;
        if (!connection_id.isEmpty()) {
            // Secrets are read here, on the worker: SecureStorage and the DB never block the GUI.
            const auto ds = fincept::DataSourceRepository::instance().get(connection_id);
            if (ds.is_err()) {
                reply.error = SourceError::make(ErrorCode::BadArgs, cs_tr("The connection no longer exists"));
            } else {
                request.type_id = ds.value().provider;
                provider = request.type_id;
                request.config = QJsonDocument::fromJson(ds.value().config.toUtf8()).object();
            }
        }
        if (reply.error.code == ErrorCode::None)
            reply = execute_source_request(request, token);

        if (persist_health && !connection_id.isEmpty() && !reply.error.cancelled) {
            const QString status = reply.ok ? QStringLiteral("ok") : QStringLiteral("error");
            const QString message = reply.ok ? reply.summary : reply.error.message;
            fincept::DataSourceRepository::instance().update_health(connection_id, status, error_code_name(reply.error.code),
                                                                    message, reply.ok ? reply.latency_ms : -1);
        }
        // Never log credentials: provider + outcome only (messages are already redacted).
        LOG_INFO(kConnSvcLogTag, QStringLiteral("%1 %2: %3 (%4 ms)")
                                     .arg(QLatin1String(cs_op_name(request.op)), provider,
                                          reply.ok ? QStringLiteral("ok") : error_code_name(reply.error.code))
                                     .arg(reply.latency_ms));

        ConnectionService* svc = self.data();
        if (!svc)
            return;
        QMetaObject::invokeMethod(
            svc,
            [self, ctx, job, connection_id, persist_health, reply = std::move(reply), cb = std::move(cb)]() {
                if (!self)
                    return;
                self->jobs_.remove(job);
                if (persist_health && !connection_id.isEmpty() && !reply.error.cancelled)
                    emit self->health_changed(connection_id, reply.ok, reply.ok ? reply.summary : reply.error.message);
                if (ctx && cb)
                    cb(reply);
            },
            Qt::QueuedConnection);
    });
    return job;
}

} // namespace fincept::services::datasources
