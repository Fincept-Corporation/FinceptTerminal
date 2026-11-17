// src/services/algo_trading/AlgoMonitorStore.cpp
#include "services/algo_trading/AlgoMonitorStore.h"

#include "core/logging/Logger.h"
#include "storage/sqlite/Database.h"

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QTimeZone>
#include <QVariant>

namespace fincept::services::algo::monitor_store {

namespace {

// created_at is SQLite datetime('now') — "yyyy-MM-dd HH:mm:ss" in UTC.
qint64 ams_parse_created(const QString& s) {
    QDateTime dt = QDateTime::fromString(s, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    if (!dt.isValid())
        return 0;
    dt.setTimeZone(QTimeZone::UTC);
    return dt.toMSecsSinceEpoch();
}

QVariant ams_field(const QSqlQuery& q, const QSqlRecord& rec, const char* name) {
    const int i = rec.indexOf(QLatin1String(name));
    return i >= 0 ? q.value(i) : QVariant();
}

} // namespace

QHash<QString, QVector<AlgoEquityPoint>> equity_curves(const QStringList& ids, qint64 since_ms, int max_points) {
    QHash<QString, QVector<AlgoEquityPoint>> out;
    if (ids.isEmpty())
        return out;
    auto db = fincept::Database::instance().connection();
    for (const QString& id : ids) {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT ts_ms, equity, realized, unrealized FROM algo_equity_snapshots "
                                 "WHERE deployment_id = ? AND ts_ms >= ? ORDER BY ts_ms"));
        q.addBindValue(id);
        q.addBindValue(since_ms);
        if (!q.exec()) {
            LOG_DEBUG("AlgoMonitorStore", logf("Equity history could not be read",
                                               {{"deployment", id}, {"error", q.lastError().text()}}));
            continue; // schema v057 not applied yet
        }
        QVector<AlgoEquityPoint> pts;
        while (q.next()) {
            AlgoEquityPoint p;
            p.t_ms = q.value(0).toLongLong();
            p.equity = q.value(1).toDouble();
            p.realized = q.value(2).toDouble();
            p.unrealized = q.value(3).toDouble();
            pts.append(p);
        }
        if (max_points > 0 && pts.size() > max_points) {
            QVector<AlgoEquityPoint> thin;
            const int step = static_cast<int>(pts.size()) / max_points + 1;
            for (int i = 0; i < pts.size(); i += step)
                thin.append(pts[i]);
            if (thin.last().t_ms != pts.last().t_ms)
                thin.append(pts.last());
            pts = thin;
        }
        if (!pts.isEmpty())
            out.insert(id, pts);
    }
    return out;
}

QVector<AlgoTradeRow> trades(const QString& deployment_id, int limit) {
    QVector<AlgoTradeRow> out;
    auto db = fincept::Database::instance().connection();
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT * FROM algo_trades WHERE deployment_id = ? ORDER BY created_at DESC LIMIT ?"));
    q.addBindValue(deployment_id);
    q.addBindValue(limit);
    if (!q.exec()) {
        // The monitor refreshes on a timer: one line a minute at most.
        LOG_WARN_RL("AlgoMonitorStore", QStringLiteral("trades"), 60000,
                    logf("Algo trades could not be read",
                         {{"deployment", deployment_id}, {"error", q.lastError().text()}}));
        return out;
    }
    const QSqlRecord rec = q.record();
    while (q.next()) {
        AlgoTradeRow r;
        r.t_ms = ams_field(q, rec, "ts_ms").toLongLong();
        if (r.t_ms <= 0)
            r.t_ms = ams_parse_created(ams_field(q, rec, "created_at").toString());
        r.symbol = ams_field(q, rec, "symbol").toString();
        r.leg_symbol = ams_field(q, rec, "leg_symbol").toString();
        r.side = ams_field(q, rec, "side").toString();
        r.reason = ams_field(q, rec, "reason").toString();
        if (r.reason.isEmpty())
            r.reason = ams_field(q, rec, "signal_reason").toString(); // pre-v057 rows
        r.mode = ams_field(q, rec, "mode").toString();
        r.broker_order_id = ams_field(q, rec, "broker_order_id").toString();
        r.quantity = ams_field(q, rec, "quantity").toDouble();
        r.price = ams_field(q, rec, "price").toDouble();
        r.ref_price = ams_field(q, rec, "ref_price").toDouble();
        r.pnl = ams_field(q, rec, "pnl").toDouble();
        r.gross_pnl = ams_field(q, rec, "gross_pnl").toDouble();
        r.fees = ams_field(q, rec, "fees").toDouble();
        r.latency_ms = ams_field(q, rec, "latency_ms").toLongLong();
        out.append(r);
    }
    return out;
}

QVector<AlgoEventRow> events(const QString& deployment_id, int limit) {
    QVector<AlgoEventRow> out;
    auto db = fincept::Database::instance().connection();
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT ts_ms, kind, message FROM algo_events WHERE deployment_id = ? "
                             "ORDER BY ts_ms DESC LIMIT ?"));
    q.addBindValue(deployment_id);
    q.addBindValue(limit);
    if (!q.exec()) {
        LOG_DEBUG("AlgoMonitorStore",
                  logf("Algo events could not be read", {{"deployment", deployment_id}, {"error", q.lastError().text()}}));
        return out;
    }
    while (q.next()) {
        AlgoEventRow e;
        e.t_ms = q.value(0).toLongLong();
        e.kind = q.value(1).toString();
        e.message = q.value(2).toString();
        out.append(e);
    }
    return out;
}

} // namespace fincept::services::algo::monitor_store
