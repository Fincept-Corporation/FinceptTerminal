// AltFundStore.cpp — see AltFundStore.h.
#include "services/alt_investments/AltFundStore.h"

#include "core/logging/Logger.h"
#include "storage/sqlite/Database.h"

#include <QDate>
#include <QDateTime>
#include <QPointer>
#include <QSqlQuery>
#include <QUuid>
#include <QtConcurrent/QtConcurrentRun>

#include <cmath>

namespace fincept::services::alt {

namespace {

/// A database failure in words a user can act on (the raw driver text goes to the log only).
QString alt_store_error(const std::string& raw) {
    const QString r = QString::fromStdString(raw);
    LOG_WARN("AltFundStore", QStringLiteral("Fund book statement failed reason=%1").arg(r.left(160)));
    if (r.contains(QLatin1String("no such table"), Qt::CaseInsensitive))
        return QObject::tr("The fund book is not available yet: this version's database update has not run. Restart "
                           "the terminal to apply it.");
    if (r.contains(QLatin1String("locked"), Qt::CaseInsensitive) || r.contains(QLatin1String("busy"), Qt::CaseInsensitive))
        return QObject::tr("The database is busy. Try again in a moment.");
    return QObject::tr("The fund book could not be read or written.");
}

AltFund alt_store_fund_from(QSqlQuery& q) {
    AltFund f;
    f.id = q.value(0).toString();
    f.name = q.value(1).toString();
    f.strategy = q.value(2).toString();
    f.vintage_year = q.value(3).toInt();
    f.commitment = q.value(4).toDouble();
    f.currency = q.value(5).toString();
    f.benchmark = q.value(6).toString();
    f.notes = q.value(7).toString();
    f.created_at = q.value(8).toLongLong();
    f.updated_at = q.value(9).toLongLong();
    return f;
}

const char* const kAltStoreFundCols =
    "id, name, strategy, vintage_year, commitment, currency, benchmark, notes, created_at, updated_at";

/// Runs `work` on the store's thread and `finish` on the GUI thread (unless `ctx` died).
template <typename R>
void alt_store_run(QThreadPool* pool, QObject* host, QObject* ctx, std::function<R()> work,
                   std::function<void(const R&)> finish) {
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

struct AltStoreFunds {
    QVector<AltFund> funds;
    QString error;
};
struct AltStoreFlows {
    QJsonArray rows;
    QString error;
};
struct AltStoreBook {
    QVector<AltFund> funds;
    QHash<QString, QJsonArray> flows;
    QString error;
};
struct AltStoreFund {
    AltFund fund;
    QString error;
};

AltStoreFunds alt_store_list() {
    AltStoreFunds out;
    auto r = Database::instance().execute(
        QStringLiteral("SELECT %1 FROM alt_funds ORDER BY created_at DESC").arg(QLatin1String(kAltStoreFundCols)));
    if (r.is_err()) {
        out.error = alt_store_error(r.error());
        return out;
    }
    auto& q = r.value();
    while (q.next())
        out.funds << alt_store_fund_from(q);
    return out;
}

AltStoreFlows alt_store_flows(const QString& fund_id) {
    AltStoreFlows out;
    auto r = Database::instance().execute(
        QStringLiteral("SELECT flow_date, kind, amount, note FROM alt_fund_flows WHERE fund_id = ? "
                       "ORDER BY flow_date, id"),
        {fund_id});
    if (r.is_err()) {
        out.error = alt_store_error(r.error());
        return out;
    }
    auto& q = r.value();
    while (q.next()) {
        QJsonObject o;
        o.insert(QStringLiteral("date"), q.value(0).toString());
        o.insert(QStringLiteral("kind"), q.value(1).toString());
        o.insert(QStringLiteral("amount"), q.value(2).toDouble());
        const QString note = q.value(3).toString();
        if (!note.isEmpty())
            o.insert(QStringLiteral("note"), note);
        out.rows.append(o);
    }
    return out;
}

} // namespace

AltFundStore& AltFundStore::instance() {
    static AltFundStore s;
    return s;
}

AltFundStore::AltFundStore(QObject* parent) : QObject(parent) {
    pool_.setMaxThreadCount(1);
    pool_.setObjectName(QStringLiteral("AltFundStore"));
}

void AltFundStore::list_funds(QObject* ctx, FundsDone done) {
    alt_store_run<AltStoreFunds>(&pool_, this, ctx, []() { return alt_store_list(); },
                                 [done](const AltStoreFunds& r) {
                                     if (done)
                                         done(r.funds, r.error);
                                 });
}

void AltFundStore::load_flows(const QString& fund_id, QObject* ctx, FlowsDone done) {
    alt_store_run<AltStoreFlows>(&pool_, this, ctx, [fund_id]() { return alt_store_flows(fund_id); },
                                 [done, fund_id](const AltStoreFlows& r) {
                                     if (done)
                                         done(fund_id, r.rows, r.error);
                                 });
}

void AltFundStore::load_book(QObject* ctx, BookDone done) {
    alt_store_run<AltStoreBook>(
        &pool_, this, ctx,
        []() {
            AltStoreBook out;
            const AltStoreFunds f = alt_store_list();
            if (!f.error.isEmpty()) {
                out.error = f.error;
                return out;
            }
            out.funds = f.funds;
            for (const AltFund& fund : f.funds) {
                const AltStoreFlows fl = alt_store_flows(fund.id);
                if (!fl.error.isEmpty()) {
                    out.error = fl.error;
                    return out;
                }
                out.flows.insert(fund.id, fl.rows);
            }
            return out;
        },
        [done](const AltStoreBook& r) {
            if (done)
                done(r.funds, r.flows, r.error);
        });
}

void AltFundStore::save_fund(const AltFund& fund_in, QObject* ctx, FundDone done) {
    QPointer<AltFundStore> self = this;
    alt_store_run<AltStoreFund>(
        &pool_, this, ctx,
        [fund_in]() {
            AltStoreFund out;
            AltFund f = fund_in;
            const qint64 now = QDateTime::currentMSecsSinceEpoch();
            f.updated_at = now;
            f.name = f.name.trimmed();
            f.currency = f.currency.trimmed().toUpper();
            if (f.currency.isEmpty())
                f.currency = QStringLiteral("USD");
            if (f.id.isEmpty()) {
                f.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
                f.created_at = now;
                auto r = Database::instance().execute(
                    QStringLiteral("INSERT INTO alt_funds (%1) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)")
                        .arg(QLatin1String(kAltStoreFundCols)),
                    {f.id, f.name, f.strategy, f.vintage_year, f.commitment, f.currency, f.benchmark, f.notes,
                     f.created_at, f.updated_at});
                if (r.is_err())
                    out.error = alt_store_error(r.error());
            } else {
                auto r = Database::instance().execute(
                    QStringLiteral("UPDATE alt_funds SET name = ?, strategy = ?, vintage_year = ?, commitment = ?, "
                                   "currency = ?, benchmark = ?, notes = ?, updated_at = ? WHERE id = ?"),
                    {f.name, f.strategy, f.vintage_year, f.commitment, f.currency, f.benchmark, f.notes, f.updated_at,
                     f.id});
                if (r.is_err())
                    out.error = alt_store_error(r.error());
            }
            out.fund = f;
            return out;
        },
        [done, self](const AltStoreFund& r) {
            if (r.error.isEmpty() && self)
                emit self->funds_changed();
            if (done)
                done(r.fund, r.error);
        });
}

void AltFundStore::save_flows(const QString& fund_id, const QJsonArray& rows, QObject* ctx, Done done) {
    QPointer<AltFundStore> self = this;
    alt_store_run<QString>(
        &pool_, this, ctx,
        [fund_id, rows]() -> QString {
            static const QStringList kKinds = {QStringLiteral("call"), QStringLiteral("distribution"),
                                               QStringLiteral("nav"), QStringLiteral("fee")};
            QVector<QVariantList> params;
            params.reserve(rows.size());
            for (const QJsonValue& v : rows) {
                const QJsonObject o = v.toObject();
                const QString date = o.value(QStringLiteral("date")).toString().left(10);
                const QString kind = o.value(QStringLiteral("kind")).toString().toLower();
                const double amount = std::fabs(o.value(QStringLiteral("amount")).toDouble(NAN));
                if (!QDate::fromString(date, Qt::ISODate).isValid() || !kKinds.contains(kind) || !std::isfinite(amount))
                    continue;
                params << QVariantList{fund_id, date, kind, amount, o.value(QStringLiteral("note")).toString()};
            }
            auto& db = Database::instance();
            auto b = db.begin_transaction();
            if (b.is_err())
                return alt_store_error(b.error());
            auto del = db.execute(QStringLiteral("DELETE FROM alt_fund_flows WHERE fund_id = ?"), {fund_id});
            if (del.is_err()) {
                (void)db.rollback();
                return alt_store_error(del.error());
            }
            if (!params.isEmpty()) {
                auto ins = db.execute_many(
                    QStringLiteral("INSERT INTO alt_fund_flows (fund_id, flow_date, kind, amount, note) "
                                   "VALUES (?, ?, ?, ?, ?)"),
                    params);
                if (ins.is_err()) {
                    (void)db.rollback();
                    return alt_store_error(ins.error());
                }
            }
            auto touch = db.execute(QStringLiteral("UPDATE alt_funds SET updated_at = ? WHERE id = ?"),
                                    {QDateTime::currentMSecsSinceEpoch(), fund_id});
            if (touch.is_err()) {
                (void)db.rollback();
                return alt_store_error(touch.error());
            }
            auto c = db.commit();
            if (c.is_err()) {
                (void)db.rollback();
                return alt_store_error(c.error());
            }
            return QString();
        },
        [done, self](const QString& error) {
            if (error.isEmpty() && self)
                emit self->funds_changed();
            if (done)
                done(error);
        });
}

void AltFundStore::delete_fund(const QString& fund_id, QObject* ctx, Done done) {
    QPointer<AltFundStore> self = this;
    alt_store_run<QString>(
        &pool_, this, ctx,
        [fund_id]() -> QString {
            auto& db = Database::instance();
            auto b = db.begin_transaction();
            if (b.is_err())
                return alt_store_error(b.error());
            {
                auto d1 = db.execute(QStringLiteral("DELETE FROM alt_fund_flows WHERE fund_id = ?"), {fund_id});
                if (d1.is_err()) {
                    (void)db.rollback();
                    return alt_store_error(d1.error());
                }
            }
            {
                auto d2 = db.execute(QStringLiteral("DELETE FROM alt_funds WHERE id = ?"), {fund_id});
                if (d2.is_err()) {
                    (void)db.rollback();
                    return alt_store_error(d2.error());
                }
            }
            auto c = db.commit();
            if (c.is_err()) {
                (void)db.rollback();
                return alt_store_error(c.error());
            }
            return QString();
        },
        [done, self](const QString& error) {
            if (error.isEmpty() && self)
                emit self->funds_changed();
            if (done)
                done(error);
        });
}

} // namespace fincept::services::alt
