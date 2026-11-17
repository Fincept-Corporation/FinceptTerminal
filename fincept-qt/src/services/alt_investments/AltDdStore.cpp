#include "services/alt_investments/AltDdStore.h"

#include "core/logging/Logger.h"
#include "storage/sqlite/Database.h"

#include <QDateTime>
#include <QFutureWatcher>
#include <QJsonDocument>
#include <QPointer>
#include <QtConcurrent/QtConcurrentRun>

namespace fincept::services::alt {

namespace {

/// Run `work` on a worker thread and hand its result to `done` on `ctx`'s thread (dropped when
/// `ctx` is gone; with no context the store itself is the target, which lives for the app).
template <typename T>
void alt_dd_async(QObject* ctx, std::function<T()> work, std::function<void(const T&)> done) {
    QObject* target = ctx ? ctx : qApp;
    auto* watcher = new QFutureWatcher<T>(target);
    QObject::connect(watcher, &QFutureWatcher<T>::finished, target, [watcher, done = std::move(done)]() {
        const T result = watcher->result();
        watcher->deleteLater();
        if (done)
            done(result);
    });
    watcher->setFuture(QtConcurrent::run(std::move(work)));
}

struct AltDdListResult {
    QVector<DdChecklist> lists;
    QString error;
};

struct AltDdOpResult {
    bool ok = false;
    QString error;
};

} // namespace

void AltDdStore::list(QObject* ctx, ListDone done) {
    alt_dd_async<AltDdListResult>(
        ctx,
        []() {
            AltDdListResult out;
            auto r = Database::instance().execute(
                QStringLiteral("SELECT id, name, template, items_json, created_at, updated_at FROM alt_dd_checklists "
                               "ORDER BY updated_at DESC"));
            if (r.is_err()) {
                out.error = QString::fromStdString(r.error());
                return out;
            }
            auto& q = r.value();
            while (q.next()) {
                DdChecklist c;
                c.id = q.value(0).toString();
                c.name = q.value(1).toString();
                c.template_id = q.value(2).toString();
                c.items = QJsonDocument::fromJson(q.value(3).toString().toUtf8()).array();
                c.created_at = q.value(4).toLongLong();
                c.updated_at = q.value(5).toLongLong();
                out.lists << c;
            }
            return out;
        },
        [done](const AltDdListResult& r) {
            if (!r.error.isEmpty())
                LOG_WARN("AltInvestments", QStringLiteral("Checklists could not be read reason=%1").arg(r.error.left(160)));
            if (done)
                done(r.lists, r.error);
        });
}

void AltDdStore::save(const DdChecklist& checklist, QObject* ctx, Done done) {
    DdChecklist c = checklist;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    c.updated_at = now;
    if (c.created_at == 0)
        c.created_at = now;
    alt_dd_async<AltDdOpResult>(
        ctx,
        [c]() {
            AltDdOpResult out;
            auto r = Database::instance().execute(
                QStringLiteral("INSERT OR REPLACE INTO alt_dd_checklists (id, name, template, items_json, created_at, "
                               "updated_at) VALUES (?, ?, ?, ?, ?, ?)"),
                {c.id, c.name, c.template_id,
                 QString::fromUtf8(QJsonDocument(c.items).toJson(QJsonDocument::Compact)), c.created_at, c.updated_at});
            out.ok = r.is_ok();
            if (!out.ok)
                out.error = QString::fromStdString(r.error());
            return out;
        },
        [done](const AltDdOpResult& r) {
            if (!r.ok)
                LOG_WARN("AltInvestments", QStringLiteral("Checklist could not be saved reason=%1").arg(r.error.left(160)));
            if (done)
                done(r.ok, r.error);
        });
}

void AltDdStore::remove(const QString& id, QObject* ctx, Done done) {
    alt_dd_async<AltDdOpResult>(
        ctx,
        [id]() {
            AltDdOpResult out;
            auto r = Database::instance().execute(QStringLiteral("DELETE FROM alt_dd_checklists WHERE id = ?"), {id});
            out.ok = r.is_ok();
            if (!out.ok)
                out.error = QString::fromStdString(r.error());
            return out;
        },
        [done](const AltDdOpResult& r) {
            if (done)
                done(r.ok, r.error);
        });
}

} // namespace fincept::services::alt
