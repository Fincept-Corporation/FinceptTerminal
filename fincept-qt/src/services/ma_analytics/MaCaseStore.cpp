#include "services/ma_analytics/MaCaseStore.h"

#include "core/logging/Logger.h"
#include "storage/sqlite/Database.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QMetaObject>
#include <QPointer>
#include <QSqlQuery>
#include <QUuid>
#include <QtConcurrent/QtConcurrentRun>

namespace fincept::services::ma {

namespace {

QString ma_cs_json(const QJsonObject& o) {
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

QJsonObject ma_cs_obj(const QString& s) {
    return QJsonDocument::fromJson(s.toUtf8()).object();
}

/// Run `work` on a worker thread, then `deliver` with its value on `ctx`'s thread (dropped when ctx is gone).
template <typename T>
void ma_cs_async(QObject* ctx, std::function<T()> work, std::function<void(const T&)> deliver) {
    QPointer<QObject> guard = ctx;
    const bool has_ctx = ctx != nullptr;
    QObject* target = ctx ? ctx : &MaCaseStore::instance();
    QPointer<QObject> target_guard = target;
    (void)QtConcurrent::run([work = std::move(work), deliver = std::move(deliver), guard, has_ctx, target_guard]() {
        T value = work();
        if (!target_guard)
            return;
        QMetaObject::invokeMethod(
            target_guard.data(),
            [deliver, value = std::move(value), guard, has_ctx]() {
                if (has_ctx && !guard)
                    return;
                deliver(value);
            },
            Qt::QueuedConnection);
    });
}

struct MaCsList {
    bool ok = false;
    QVector<MaCase> cases;
    QString error;
};
struct MaCsSave {
    bool ok = false;
    MaCase saved;
    QString error;
};
struct MaCsRemove {
    bool ok = false;
    QString error;
};

} // namespace

MaCaseStore& MaCaseStore::instance() {
    static MaCaseStore s;
    return s;
}

MaCaseStore::MaCaseStore(QObject* parent) : QObject(parent) {}

QString MaCaseStore::plain_error(const QString& raw) {
    if (raw.contains(QLatin1String("no such table"), Qt::CaseInsensitive))
        return tr("Saved cases need the latest database update; restart the terminal once to apply it.");
    if (raw.contains(QLatin1String("locked"), Qt::CaseInsensitive))
        return tr("The database is busy; try again in a moment.");
    return tr("Could not read or write saved cases: %1").arg(raw.left(200));
}

void MaCaseStore::list(const QString& module, QObject* ctx, ListDone done) {
    ma_cs_async<MaCsList>(
        ctx,
        [module]() {
            MaCsList out;
            const QString sql =
                module.isEmpty()
                    ? QStringLiteral("SELECT id, name, module, context, inputs_json, result_json, notes, created_at, "
                                     "updated_at FROM ma_cases ORDER BY updated_at DESC LIMIT 500")
                    : QStringLiteral("SELECT id, name, module, context, inputs_json, result_json, notes, created_at, "
                                     "updated_at FROM ma_cases WHERE module = ? ORDER BY updated_at DESC LIMIT 500");
            auto r = Database::instance().execute(sql, module.isEmpty() ? QVariantList{} : QVariantList{module});
            if (r.is_err()) {
                out.error = QString::fromStdString(r.error());
                return out;
            }
            auto& q = r.value();
            while (q.next()) {
                MaCase c;
                c.id = q.value(0).toString();
                c.name = q.value(1).toString();
                c.module = q.value(2).toString();
                c.context = q.value(3).toString();
                c.inputs = ma_cs_obj(q.value(4).toString());
                c.result = ma_cs_obj(q.value(5).toString());
                c.notes = q.value(6).toString();
                c.created_at = q.value(7).toLongLong();
                c.updated_at = q.value(8).toLongLong();
                out.cases.append(c);
            }
            out.ok = true;
            return out;
        },
        [done](const MaCsList& r) {
            if (!r.ok)
                LOG_WARN("MAAnalytics", QStringLiteral("Saved cases could not be listed reason=%1").arg(r.error.left(160)));
            if (done)
                done(r.ok, r.cases, r.ok ? QString() : plain_error(r.error));
        });
}

void MaCaseStore::save(const MaCase& c, QObject* ctx, SaveDone done) {
    ma_cs_async<MaCsSave>(
        ctx,
        [c]() {
            MaCsSave out;
            MaCase saved = c;
            const qint64 now = QDateTime::currentMSecsSinceEpoch();
            auto& db = Database::instance();
            if (saved.id.isEmpty()) {
                // The same name for the same analysis replaces the earlier case.
                auto f = db.execute(QStringLiteral("SELECT id, created_at FROM ma_cases WHERE module = ? AND context = ? "
                                                   "AND name = ?"),
                                    {saved.module, saved.context, saved.name});
                if (f.is_err()) {
                    out.error = QString::fromStdString(f.error());
                    return out;
                }
                if (f.value().next()) {
                    saved.id = f.value().value(0).toString();
                    saved.created_at = f.value().value(1).toLongLong();
                }
            }
            if (saved.id.isEmpty()) {
                saved.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
                saved.created_at = now;
            }
            if (saved.created_at == 0)
                saved.created_at = now;
            saved.updated_at = now;
            auto r = db.execute(QStringLiteral("INSERT OR REPLACE INTO ma_cases (id, name, module, context, inputs_json, "
                                               "result_json, notes, created_at, updated_at) VALUES (?,?,?,?,?,?,?,?,?)"),
                                {saved.id, saved.name, saved.module, saved.context, ma_cs_json(saved.inputs),
                                 ma_cs_json(saved.result), saved.notes, saved.created_at, saved.updated_at});
            if (r.is_err()) {
                out.error = QString::fromStdString(r.error());
                return out;
            }
            out.ok = true;
            out.saved = saved;
            return out;
        },
        [done](const MaCsSave& r) {
            if (r.ok)
                LOG_INFO("MAAnalytics", QStringLiteral("Case saved module=%1 context=%2").arg(r.saved.module, r.saved.context));
            else
                LOG_WARN("MAAnalytics", QStringLiteral("Case could not be saved reason=%1").arg(r.error.left(160)));
            if (done)
                done(r.ok, r.saved, r.ok ? QString() : plain_error(r.error));
        });
}

void MaCaseStore::remove(const QString& id, QObject* ctx, RemoveDone done) {
    ma_cs_async<MaCsRemove>(
        ctx,
        [id]() {
            MaCsRemove out;
            auto r = Database::instance().execute(QStringLiteral("DELETE FROM ma_cases WHERE id = ?"), {id});
            out.ok = !r.is_err();
            if (!out.ok)
                out.error = QString::fromStdString(r.error());
            return out;
        },
        [done](const MaCsRemove& r) {
            if (done)
                done(r.ok, r.ok ? QString() : plain_error(r.error));
        });
}

} // namespace fincept::services::ma
