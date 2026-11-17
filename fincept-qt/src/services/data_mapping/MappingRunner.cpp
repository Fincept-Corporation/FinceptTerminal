// MappingRunner.cpp — see MappingRunner.h.

#include "services/data_mapping/MappingRunner.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "services/connectors/ConnectorService.h"
#include "services/data_sources/ConnectionService.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <QSaveFile>
#include <QSqlDatabase>
#include <QSqlDriver>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <QtConcurrent/QtConcurrent>

namespace fincept::services::datamapping {

namespace ds = fincept::services::datasources;

namespace {

QString mr_tr(const char* text) {
    return QCoreApplication::translate("MappingRunner", text);
}

/// The pre-v062 auth type words → the REST connection's authType.
QString mr_auth_type(const QString& legacy) {
    const QString t = legacy.toLower();
    if (t.contains(QLatin1String("bearer")) || t.contains(QLatin1String("oauth")))
        return QStringLiteral("bearer");
    if (t.contains(QLatin1String("basic")))
        return QStringLiteral("basic");
    if (t.contains(QLatin1String("key")))
        return QStringLiteral("apikey");
    return QStringLiteral("none");
}

void mr_deliver(QPointer<QObject> ctx, const MappingRunner::SourceCallback& cb, SourceData data) {
    QMetaObject::invokeMethod(
        &MappingRunner::instance(),
        [ctx, cb, data = std::move(data)]() {
            if (ctx && cb)
                cb(data);
        },
        Qt::QueuedConnection);
}

ds::SourceError mr_write(const QString& path, const QByteArray& bytes) {
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return ds::SourceError::make(ds::ErrorCode::BadArgs, mr_tr("Cannot write %1: %2").arg(path, f.errorString()));
    f.write(bytes);
    if (!f.commit())
        return ds::SourceError::make(ds::ErrorCode::BadArgs, mr_tr("Cannot write %1: %2").arg(path, f.errorString()));
    return ds::SourceError();
}

void mr_finish(QPointer<QObject> ctx, MappingRunner::DoneCallback done, ds::SourceError e) {
    QMetaObject::invokeMethod(
        &MappingRunner::instance(),
        [ctx, done = std::move(done), e = std::move(e)]() {
            if (ctx && done)
                done(e);
        },
        Qt::QueuedConnection);
}

} // namespace

MappingRunner& MappingRunner::instance() {
    static MappingRunner s;
    return s;
}

MappingRunner::MappingRunner(QObject* parent) : QObject(parent) {}

quint64 MappingRunner::load_source(const SourceRef& source, const QString& auth_token, int limit, QObject* context,
                                   SourceCallback cb) {
    const quint64 job = ++next_;
    QPointer<QObject> ctx(context);
    const QString table = source.table;
    auto pick = [table](SourceData d) {
        // A multi-table result keeps every table; the mapping reads the one it names (or the first).
        if (d.ok && !table.isEmpty()) {
            const int idx = d.data.index_of(table);
            if (idx > 0)
                std::swap(d.data.tables[0], d.data.tables[idx]);
            else if (idx < 0)
                d.notes << mr_tr("Table \"%1\" is not in this result; the first table is used").arg(table);
        }
        return d;
    };
    auto from_reply = [pick](const ds::SourceReply& r) {
        SourceData d;
        d.ok = r.ok;
        d.error = r.error;
        d.data = r.data;
        d.notes = r.notes;
        return pick(d);
    };

    if (source.kind == QLatin1String("connection")) {
        auto& svc = ds::ConnectionService::instance();
        const quint64 sj = svc.fetch_connection(source.ref, source.entity, source.params.value("query").toString(), limit,
                                                this, [ctx, cb, from_reply](const ds::SourceReply& r) {
                                                    if (ctx && cb)
                                                        cb(from_reply(r));
                                                });
        cancels_.insert(job, [sj]() { ds::ConnectionService::instance().cancel(sj); });
        return job;
    }
    if (source.kind == QLatin1String("connector")) {
        namespace conn = fincept::services::connectors;
        const QString rid = conn::ConnectorService::instance().run_with(
            source.ref, source.entity, source.args, conn::ConnectorService::RunOptions{},
            [ctx, cb, pick](const conn::ConnectorResult& r) {
                SourceData d;
                d.ok = r.success;
                d.error = r.failure;
                if (!r.success && d.error.code == ds::ErrorCode::None)
                    d.error = ds::SourceError::make(ds::ErrorCode::Unknown, r.error);
                d.data = r.tabular;
                if (ctx && cb)
                    cb(pick(d));
            });
        cancels_.insert(job, [rid]() { fincept::services::connectors::ConnectorService::instance().cancel(rid); });
        return job;
    }

    ds::SourceRequest req;
    req.op = ds::SourceOp::Fetch;
    req.limit = limit;
    req.timeout_ms = 60000;
    if (source.kind == QLatin1String("file")) {
        // Parquet / Feather / Arrow (and .xls) are read by the local file connector behind those types.
        const QString ext = QFileInfo(source.ref).suffix().toLower();
        const auto is = [&ext](std::initializer_list<const char*> list) {
            for (const char* e : list)
                if (ext == QLatin1String(e))
                    return true;
            return false;
        };
        req.type_id = is({"xlsx", "xlsm", "xls"})                 ? QStringLiteral("excel")
                      : is({"json", "ndjson", "jsonl", "jsonlines"}) ? QStringLiteral("json")
                      : is({"xml"})                               ? QStringLiteral("xml")
                      : is({"parquet", "parq", "pq"})             ? QStringLiteral("parquet")
                      : is({"feather", "arrow", "ipc", "arrows"}) ? QStringLiteral("arrow-ipc")
                                                                  : QStringLiteral("csv");
        req.config = source.params;
        req.config.insert(QStringLiteral("filepath"), source.ref);
        if (!source.params.value("rows_path").toString().isEmpty()) {
            req.config.insert(QStringLiteral("jsonPath"), source.params.value("rows_path").toString());
            req.config.insert(QStringLiteral("xpath"), source.params.value("rows_path").toString());
        }
        req.entity = source.entity; // sheet
    } else if (source.kind == QLatin1String("http")) {
        const QJsonObject h = source.http;
        auto with_auth = [&auth_token](QString s, bool url) {
            return s.replace(QStringLiteral("{AUTH}"), url ? QString::fromUtf8(QUrl::toPercentEncoding(auth_token)) : auth_token);
        };
        req.type_id = QStringLiteral("rest-api");
        QJsonObject cfg;
        cfg.insert(QStringLiteral("baseUrl"), with_auth(h.value("base_url").toString(), true));
        cfg.insert(QStringLiteral("path"), with_auth(h.value("endpoint").toString(), true));
        cfg.insert(QStringLiteral("method"), h.value("method").toString(QStringLiteral("GET")));
        cfg.insert(QStringLiteral("body"), with_auth(h.value("body").toString(), false));
        cfg.insert(QStringLiteral("headers"), with_auth(h.value("headers").toString(), false));
        const QString auth = mr_auth_type(h.value("auth_type").toString());
        const bool placeholder = h.value("base_url").toString().contains(QLatin1String("{AUTH}")) ||
                                 h.value("endpoint").toString().contains(QLatin1String("{AUTH}")) ||
                                 h.value("headers").toString().contains(QLatin1String("{AUTH}"));
        cfg.insert(QStringLiteral("authType"), placeholder || auth_token.isEmpty() ? QStringLiteral("none") : auth);
        cfg.insert(QStringLiteral("apiKey"), auth_token);
        cfg.insert(QStringLiteral("format"), QStringLiteral("json"));
        cfg.insert(QStringLiteral("rowsPath"),
                   h.value("document_mode").toBool() ? QStringLiteral("$document") : h.value("rows_path").toString());
        req.config = cfg;
    } else {
        SourceData d;
        d.error = ds::SourceError::make(ds::ErrorCode::BadArgs, mr_tr("Choose a source for this mapping first"));
        mr_deliver(ctx, cb, d);
        return job;
    }
    const quint64 sj = ds::ConnectionService::instance().run(req, this, [ctx, cb, from_reply](const ds::SourceReply& r) {
        if (ctx && cb)
            cb(from_reply(r));
    });
    cancels_.insert(job, [sj]() { ds::ConnectionService::instance().cancel(sj); });
    return job;
}

void MappingRunner::cancel(quint64 job) {
    const auto it = cancels_.find(job);
    if (it == cancels_.end())
        return;
    if (it.value())
        it.value()();
    cancels_.erase(it);
}

void MappingRunner::apply_async(const ds::DataTable& input, const MappingSpec& spec, int max_rows, QObject* context,
                                MapCallback cb) {
    QPointer<QObject> ctx(context);
    (void)QtConcurrent::run([input, spec, max_rows, ctx, cb]() {
        MappingOutput out = apply_mapping(input, spec, max_rows);
        QMetaObject::invokeMethod(
            &MappingRunner::instance(),
            [ctx, cb, out = std::move(out)]() {
                if (ctx && cb)
                    cb(out);
            },
            Qt::QueuedConnection);
    });
}

void MappingRunner::store_output(const QString& mapping_id, int revision, const QString& schema, const MappingOutput& out,
                                 const QString& source_label) {
    if (mapping_id.isEmpty())
        return;
    const QString status = out.rows_out == 0 ? QStringLiteral("error")
                           : out.clean()     ? QStringLiteral("ok")
                                             : QStringLiteral("issues");
    QJsonArray types;
    for (ds::ColumnType t : out.table.types)
        types.append(ds::column_type_name(t));
    QJsonArray rows;
    for (const QJsonArray& r : out.table.rows)
        rows.append(r);
    fincept::MappedDataset d;
    d.mapping_id = mapping_id;
    d.revision = revision;
    d.schema_name = schema;
    d.columns_json = QString::fromUtf8(QJsonDocument(QJsonArray::fromStringList(out.table.columns)).toJson(QJsonDocument::Compact));
    d.types_json = QString::fromUtf8(QJsonDocument(types).toJson(QJsonDocument::Compact));
    d.rows_json = QString::fromUtf8(QJsonDocument(rows).toJson(QJsonDocument::Compact));
    d.row_count = out.rows_out;
    d.error_cells = out.error_cells;
    d.source = source_label;
    const int errors = out.error_cells;
    const int nrows = out.rows_out;
    (void)QtConcurrent::run([d, mapping_id, status, nrows, errors]() {
        auto& repo = fincept::DataMappingRepository::instance();
        repo.save_output(d);
        repo.update_run_stats(mapping_id, status, nrows, errors);
    });
    // Live consumers (dashboard widgets, workflows) subscribe to "mapping:<id>".
    fincept::datahub::DataHub::instance().publish(
        topic_for(mapping_id),
        QVariant(QJsonObject{{"mapping_id", mapping_id},
                             {"revision", revision},
                             {"schema", schema},
                             {"columns", QJsonArray::fromStringList(out.table.columns)},
                             {"types", types},
                             {"rows", rows},
                             {"error_cells", errors},
                             {"produced_at", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}}));
    LOG_INFO("DataMapping", QStringLiteral("Run stored: %1 rows, %2 cell issue(s)").arg(nrows).arg(errors));
}

void MappingRunner::export_csv(const ds::DataTable& table, const QString& path, QObject* context, DoneCallback done) {
    QPointer<QObject> ctx(context);
    (void)QtConcurrent::run([table, path, ctx, done]() { mr_finish(ctx, done, mr_write(path, ds::table_to_csv(table))); });
}

void MappingRunner::export_json(const ds::DataTable& table, const QString& path, QObject* context, DoneCallback done) {
    QPointer<QObject> ctx(context);
    (void)QtConcurrent::run([table, path, ctx, done]() {
        mr_finish(ctx, done, mr_write(path, QJsonDocument(ds::table_to_records(table)).toJson(QJsonDocument::Indented)));
    });
}

void MappingRunner::export_sqlite(const ds::DataTable& table, const QString& path, const QString& table_name,
                                  QObject* context, DoneCallback done) {
    QPointer<QObject> ctx(context);
    (void)QtConcurrent::run([table, path, table_name, ctx, done]() {
        const QString conn = QStringLiteral("dm_export_") + QUuid::createUuid().toString(QUuid::Id128);
        ds::SourceError err;
        {
            QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), conn);
            db.setDatabaseName(path);
            if (!db.open()) {
                err = ds::SourceError::make(ds::ErrorCode::BadArgs, mr_tr("Cannot open %1: %2").arg(path, db.lastError().text()));
            } else {
                QSqlDriver* drv = db.driver();
                const QString name = drv->escapeIdentifier(table_name.isEmpty() ? QStringLiteral("mapped") : table_name,
                                                           QSqlDriver::TableName);
                QStringList defs;
                for (int c = 0; c < table.columns.size(); ++c) {
                    const ds::ColumnType t = table.types.value(c);
                    const char* sql_type = t == ds::ColumnType::Integer || t == ds::ColumnType::Bool ? "INTEGER"
                                           : t == ds::ColumnType::Number                             ? "REAL"
                                                                                                     : "TEXT";
                    defs << drv->escapeIdentifier(table.columns[c], QSqlDriver::FieldName) + QLatin1Char(' ') +
                                QLatin1String(sql_type);
                }
                QSqlQuery q(db);
                db.transaction();
                bool ok = q.exec(QStringLiteral("DROP TABLE IF EXISTS %1").arg(name)) &&
                          q.exec(QStringLiteral("CREATE TABLE %1 (%2)").arg(name, defs.join(QStringLiteral(", "))));
                if (ok) {
                    QStringList marks;
                    for (int c = 0; c < table.columns.size(); ++c)
                        marks << QStringLiteral("?");
                    ok = q.prepare(QStringLiteral("INSERT INTO %1 VALUES (%2)").arg(name, marks.join(QLatin1Char(','))));
                    for (int r = 0; ok && r < table.row_count(); ++r) {
                        for (int c = 0; c < table.columns.size(); ++c) {
                            const QJsonValue v = table.rows[r].at(c);
                            q.bindValue(c, v.isNull() || v.isUndefined() ? QVariant()
                                           : v.isDouble()                ? QVariant(v.toDouble())
                                           : v.isBool()                  ? QVariant(v.toBool() ? 1 : 0)
                                                                         : QVariant(ds::cell_text(v)));
                        }
                        ok = q.exec();
                    }
                }
                if (ok) {
                    db.commit();
                } else {
                    err = ds::SourceError::make(ds::ErrorCode::Unknown, mr_tr("SQLite export failed: %1").arg(q.lastError().text()));
                    db.rollback();
                }
                q = QSqlQuery();
                db.close();
            }
        }
        QSqlDatabase::removeDatabase(conn);
        mr_finish(ctx, done, err);
    });
}

} // namespace fincept::services::datamapping
