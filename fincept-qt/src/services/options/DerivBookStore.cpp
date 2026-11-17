#include "services/options/DerivBookStore.h"

#include "core/logging/Logger.h"
#include "storage/repositories/DerivBookRepository.h"
#include "storage/sqlite/Database.h"

#include <QMetaObject>
#include <QPointer>
#include <QtConcurrent/QtConcurrentRun>

namespace fincept::services::options {

namespace {

/// A database failure in words a user can act on (the raw driver text goes to the log only).
QString dbs_error(const std::string& raw) {
    const QString r = QString::fromStdString(raw);
    LOG_WARN("DerivBook", QStringLiteral("Book statement failed reason=%1").arg(r.left(160)));
    if (r.contains(QLatin1String("no such table"), Qt::CaseInsensitive))
        return QObject::tr("The position book is not available yet: this version's database update has not run. "
                           "Restart the terminal to apply it.");
    if (r.contains(QLatin1String("locked"), Qt::CaseInsensitive) || r.contains(QLatin1String("busy"), Qt::CaseInsensitive))
        return QObject::tr("The database is busy. Try again in a moment.");
    if (r.contains(QLatin1String("empty name")))
        return QObject::tr("Give the strategy a name.");
    return QObject::tr("The position book could not be read or written.");
}

/// Runs `work` on the store's thread and `finish` on the GUI thread (unless `ctx` died).
template <typename R>
void dbs_run(QThreadPool* pool, QObject* host, QObject* ctx, std::function<R()> work, std::function<void(const R&)> finish) {
    QPointer<QObject> guard = ctx;
    const bool has_ctx = ctx != nullptr;
    QPointer<QObject> host_guard = host;
    (void)QtConcurrent::run(pool, [work = std::move(work), finish = std::move(finish), guard, has_ctx, host_guard]() {
        R result = work();
        if (!host_guard)
            return;
        QMetaObject::invokeMethod(
            host_guard.data(),
            [result = std::move(result), finish, guard, has_ctx]() {
                if (has_ctx && !guard)
                    return;
                finish(result);
            },
            Qt::QueuedConnection);
    });
}

DerivBookRepository dbs_repo() {
    return DerivBookRepository(Database::instance().connection());
}

struct DbsPositions {
    QVector<book::BookPosition> rows;
    QString error;
};
struct DbsPosition {
    book::BookPosition row;
    QString error;
};
struct DbsStrategies {
    QVector<book::SavedStrategy> rows;
    QString error;
};
struct DbsStrategy {
    book::SavedStrategy row;
    QString error;
};
struct DbsBooks {
    QStringList books;
    QString error;
};

} // namespace

DerivBookStore& DerivBookStore::instance() {
    static DerivBookStore s;
    return s;
}

DerivBookStore::DerivBookStore(QObject* parent) : QObject(parent) {
    pool_.setMaxThreadCount(1);
    pool_.setObjectName(QStringLiteral("DerivBookStore"));
}

void DerivBookStore::load_positions(const QString& book, QObject* ctx, PositionsDone done) {
    dbs_run<DbsPositions>(
        &pool_, this, ctx,
        [book]() {
            DbsPositions out;
            auto r = dbs_repo().positions(book);
            if (r.is_err())
                out.error = dbs_error(r.error());
            else
                out.rows = r.value();
            return out;
        },
        [done](const DbsPositions& r) {
            if (done)
                done(r.rows, r.error);
        });
}

void DerivBookStore::load_books(QObject* ctx, BooksDone done) {
    dbs_run<DbsBooks>(
        &pool_, this, ctx,
        []() {
            DbsBooks out;
            auto r = dbs_repo().books();
            if (r.is_err())
                out.error = dbs_error(r.error());
            else
                out.books = r.value();
            return out;
        },
        [done](const DbsBooks& r) {
            if (done)
                done(r.books, r.error);
        });
}

void DerivBookStore::save_position(const book::BookPosition& p, QObject* ctx, PositionDone done) {
    QPointer<DerivBookStore> self = this;
    dbs_run<DbsPosition>(
        &pool_, this, ctx,
        [p]() {
            DbsPosition out;
            auto r = dbs_repo().save_position(p);
            if (r.is_err())
                out.error = dbs_error(r.error());
            else
                out.row = r.value();
            return out;
        },
        [done, self](const DbsPosition& r) {
            if (r.error.isEmpty() && self)
                emit self->positions_changed();
            if (done)
                done(r.row, r.error);
        });
}

void DerivBookStore::remove_position(const QString& id, QObject* ctx, Done done) {
    QPointer<DerivBookStore> self = this;
    dbs_run<QString>(
        &pool_, this, ctx,
        [id]() -> QString {
            auto r = dbs_repo().remove_position(id);
            return r.is_err() ? dbs_error(r.error()) : QString();
        },
        [done, self](const QString& error) {
            if (error.isEmpty() && self)
                emit self->positions_changed();
            if (done)
                done(error);
        });
}

void DerivBookStore::load_strategies(QObject* ctx, StrategiesDone done) {
    dbs_run<DbsStrategies>(
        &pool_, this, ctx,
        []() {
            DbsStrategies out;
            auto r = dbs_repo().strategies();
            if (r.is_err())
                out.error = dbs_error(r.error());
            else
                out.rows = r.value();
            return out;
        },
        [done](const DbsStrategies& r) {
            if (done)
                done(r.rows, r.error);
        });
}

void DerivBookStore::save_strategy(const book::SavedStrategy& s, QObject* ctx, StrategyDone done) {
    QPointer<DerivBookStore> self = this;
    dbs_run<DbsStrategy>(
        &pool_, this, ctx,
        [s]() {
            DbsStrategy out;
            auto r = dbs_repo().save_strategy(s);
            if (r.is_err())
                out.error = dbs_error(r.error());
            else
                out.row = r.value();
            return out;
        },
        [done, self](const DbsStrategy& r) {
            if (r.error.isEmpty() && self)
                emit self->strategies_changed();
            if (done)
                done(r.row, r.error);
        });
}

void DerivBookStore::remove_strategy(const QString& id, QObject* ctx, Done done) {
    QPointer<DerivBookStore> self = this;
    dbs_run<QString>(
        &pool_, this, ctx,
        [id]() -> QString {
            auto r = dbs_repo().remove_strategy(id);
            return r.is_err() ? dbs_error(r.error()) : QString();
        },
        [done, self](const QString& error) {
            if (error.isEmpty() && self)
                emit self->strategies_changed();
            if (done)
                done(error);
        });
}

} // namespace fincept::services::options
