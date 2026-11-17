#include "trading/instruments/InstrumentRepository.h"

#include "core/logging/Logger.h"
#include "trading/instruments/InstrumentSearchSql.h"

#include <QSqlQuery>

namespace fincept::trading {

InstrumentRepository& InstrumentRepository::instance() {
    static InstrumentRepository inst;
    return inst;
}

// ── Row mapper ────────────────────────────────────────────────────────────────

Instrument InstrumentRepository::map_row(QSqlQuery& q) {
    Instrument i;
    i.instrument_token = q.value(0).toLongLong();
    i.exchange_token = q.value(1).toLongLong();
    i.symbol = q.value(2).toString();
    i.brsymbol = q.value(3).toString();
    i.name = q.value(4).toString();
    i.exchange = q.value(5).toString();
    i.brexchange = q.value(6).toString();
    i.expiry = q.value(7).toString();
    i.strike = q.value(8).toDouble();
    i.lot_size = q.value(9).toInt();
    i.instrument_type = parse_instrument_type(q.value(10).toString());
    i.tick_size = q.value(11).toDouble();
    i.broker_id = q.value(12).toString();
    i.broker_token = q.value(13).toString();
    return i;
}

Instrument InstrumentRepository::map_row_static(QSqlQuery& q) {
    return map_row(q);
}

// ── Write ─────────────────────────────────────────────────────────────────────

fincept::Result<void> InstrumentRepository::replace_all(const QString& broker_id,
                                                        const QVector<Instrument>& instruments) {
    auto r = db().begin_transaction();
    if (r.is_err())
        return r;

    // Clear existing rows for this broker
    auto rc = exec_write("DELETE FROM instruments WHERE broker_id = ?", {broker_id});
    if (rc.is_err()) {
        db().rollback();
        return rc;
    }

    // Bulk insert — one prepared statement, many binds
    const QString sql = "INSERT OR IGNORE INTO instruments "
                        "(instrument_token, exchange_token, symbol, brsymbol, name, exchange, brexchange, "
                        " expiry, strike, lot_size, instrument_type, tick_size, broker_id, broker_token) "
                        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

    // A default-constructed QString is *null*, and QtSql binds a null QString as
    // SQL NULL. Every text column here is NOT NULL, so a null bind (e.g. an unset
    // broker_token, which numeric-token brokers like Fyers never populate) makes
    // INSERT OR IGNORE silently drop the row — the whole catalog vanishes while the
    // transaction still "succeeds". Coalesce nulls to "" so the row persists.
    auto nn = [](const QString& s) { return s.isNull() ? QStringLiteral("") : s; };

    // One prepare, N binds. The previous loop called exec_write() per instrument,
    // and exec_write() → Database::execute() re-prepares the statement every
    // call — a broker master contract is ~100k rows, so an import paid 100k
    // sqlite3_prepare_v2 compilations of this one identical INSERT.
    // execute_many() opens no transaction of its own, so it runs inside the
    // begin_transaction() above and the whole import is still a single commit.
    QVector<QVariantList> rows;
    rows.reserve(instruments.size());
    for (const auto& inst : instruments) {
        rows.append(QVariantList{
            inst.instrument_token,
            inst.exchange_token,
            nn(inst.symbol),
            nn(inst.brsymbol),
            nn(inst.name),
            nn(inst.exchange),
            nn(inst.brexchange),
            nn(inst.expiry),
            inst.strike,
            inst.lot_size,
            QString(instrument_type_str(inst.instrument_type)),
            inst.tick_size,
            broker_id,
            nn(inst.broker_token),
        });
    }

    auto ri = db().execute_many(sql, rows);
    if (ri.is_err()) {
        LOG_ERROR("InstrumentRepo", QString("replace_all bulk insert failed for %1: %2")
                                        .arg(broker_id, QString::fromStdString(ri.error())));
        db().rollback();
        return ri;
    }

    auto rc2 = db().commit();
    if (rc2.is_err()) {
        db().rollback();
        return rc2;
    }

    // Verify the write actually persisted on the same connection. INSERT OR IGNORE
    // returns success even when every row is dropped, so a "successful" commit can
    // still leave the table empty. Surface that loudly — but do NOT fail the
    // refresh: the caller still builds the in-memory cache from `instruments`,
    // which is what powers symbol search and resolution this session.
    const int persisted = count(broker_id);
    if (persisted == 0 && !instruments.isEmpty()) {
        LOG_ERROR("InstrumentRepo", QString("replace_all persisted 0 rows for %1 despite %2 parsed instruments — "
                                            "commit succeeded but nothing landed on disk (in-memory cache still used)")
                                        .arg(broker_id)
                                        .arg(instruments.size()));
    } else {
        LOG_INFO("InstrumentRepo", QString("Replaced %1 instruments for %2 (verified %3 rows persisted)")
                                       .arg(instruments.size())
                                       .arg(broker_id)
                                       .arg(persisted));
    }
    return fincept::Result<void>::ok();
}

fincept::Result<void> InstrumentRepository::clear(const QString& broker_id) {
    return exec_write("DELETE FROM instruments WHERE broker_id = ?", {broker_id});
}

// ── Read ──────────────────────────────────────────────────────────────────────

std::optional<Instrument> InstrumentRepository::find(const QString& symbol, const QString& exchange,
                                                     const QString& broker_id) const {
    return query_optional("SELECT instrument_token, exchange_token, symbol, brsymbol, name, exchange, brexchange, "
                          "expiry, strike, lot_size, instrument_type, tick_size, broker_id, broker_token "
                          "FROM instruments WHERE symbol = ? AND exchange = ? AND broker_id = ? LIMIT 1",
                          {symbol, exchange, broker_id}, map_row);
}

std::optional<Instrument> InstrumentRepository::find_by_token(qint64 instrument_token, const QString& broker_id) const {
    return query_optional("SELECT instrument_token, exchange_token, symbol, brsymbol, name, exchange, brexchange, "
                          "expiry, strike, lot_size, instrument_type, tick_size, broker_id, broker_token "
                          "FROM instruments WHERE instrument_token = ? AND broker_id = ? LIMIT 1",
                          {instrument_token, broker_id}, map_row);
}

std::optional<Instrument> InstrumentRepository::find_by_brsymbol(const QString& brsymbol, const QString& brexchange,
                                                                 const QString& broker_id) const {
    return query_optional("SELECT instrument_token, exchange_token, symbol, brsymbol, name, exchange, brexchange, "
                          "expiry, strike, lot_size, instrument_type, tick_size, broker_id, broker_token "
                          "FROM instruments WHERE brsymbol = ? AND brexchange = ? AND broker_id = ? LIMIT 1",
                          {brsymbol, brexchange, broker_id}, map_row);
}

QVector<Instrument> InstrumentRepository::search(const QString& query, const QString& exchange,
                                                 const QString& broker_id, int limit) const {
    // One broker is search_all() over a one-element list: same match rules, same
    // order (the "active broker first" term is constant for a single broker).
    return search_all(query, exchange, QStringList{broker_id}, limit);
}

QVector<Instrument> InstrumentRepository::search_all(const QString& query, const QString& exchange,
                                                     const QStringList& broker_ids, int limit) const {
    namespace iss = instrument_search_sql;
    const QString text = query.trimmed();
    if (text.isEmpty() || limit <= 0)
        return {};
    // Exchange stored upper-case; normalise the filter. Empty = any exchange.
    const QString exch = exchange.trimmed().toUpper();

    QString sql = "SELECT instrument_token, exchange_token, symbol, brsymbol, name, exchange, brexchange, "
                  "expiry, strike, lot_size, instrument_type, tick_size, broker_id, broker_token "
                  "FROM instruments "
                  "WHERE (exchange = ? OR ? = '') ";
    QVariantList params = {exch, exch};

    // User text is bound and its LIKE wildcards are escaped (see InstrumentSearchSql.h).
    // A short query ("N", "RE") matches symbol prefixes only, as a range on the
    // symbol index; "%N%" over three columns read every instrument row per keystroke.
    const bool prefix_only = iss::prefix_only(text);
    if (prefix_only) {
        QString lo, hi;
        if (iss::prefix_range(text.toUpper(), lo, hi)) {
            sql += "AND symbol >= ? AND symbol < ? ";
            params << lo << hi;
        } else {
            sql += "AND symbol LIKE ? ESCAPE '\\' ";
            params << iss::starts_with_pattern(text);
        }
    } else {
        const QString pattern = iss::contains_pattern(text);
        sql += "AND (UPPER(symbol) LIKE ? ESCAPE '\\' OR UPPER(brsymbol) LIKE ? ESCAPE '\\' "
               "OR UPPER(name) LIKE ? ESCAPE '\\') ";
        params << pattern << pattern << pattern;
    }

    // Scope to the connected brokers. Empty list = all brokers in the catalog.
    if (!broker_ids.isEmpty()) {
        QStringList placeholders;
        for (const QString& b : broker_ids) {
            placeholders << "?";
            params.append(b);
        }
        // For the prefix range the unary '+' keeps the planner on the symbol index:
        // an equality on broker_id would otherwise win (idx_instruments_broker_updated)
        // and read every row of the broker again.
        sql += QString(prefix_only ? "AND +broker_id IN (%1) " : "AND broker_id IN (%1) ").arg(placeholders.join(","));
    }

    // Active broker (first in the list) first; then symbol-prefix hits before other
    // hits (the in-memory search's order); then EQ → INDEX → FUT → other; symbol.
    const QString active = broker_ids.isEmpty() ? QString() : broker_ids.first();
    sql += "ORDER BY CASE WHEN broker_id = ? THEN 0 ELSE 1 END, "
           "CASE WHEN symbol LIKE ? ESCAPE '\\' THEN 0 ELSE 1 END, "
           "CASE instrument_type WHEN 'EQ' THEN 0 WHEN 'INDEX' THEN 1 WHEN 'FUT' THEN 2 ELSE 3 END, "
           "symbol, broker_id "
           "LIMIT ?";
    params.append(active);
    params.append(iss::starts_with_pattern(text));
    params.append(limit);

    auto r = db().execute(sql, params);
    if (r.is_err()) {
        // Not the typed text (policy B.6): its length.
        LOG_WARN("InstrumentRepo", logf("Instrument search failed", {{"query_chars", text.size()},
                                                                     {"error", QString::fromStdString(r.error())}}));
        return {};
    }
    QVector<Instrument> results;
    auto& q = r.value();
    while (q.next())
        results.append(map_row(q));
    return results;
}

QVector<Instrument> InstrumentRepository::list(const QString& exchange, const QString& broker_id,
                                               InstrumentType type) const {
    QString sql = "SELECT instrument_token, exchange_token, symbol, brsymbol, name, exchange, brexchange, "
                  "expiry, strike, lot_size, instrument_type, tick_size, broker_id, broker_token "
                  "FROM instruments WHERE exchange = ? AND broker_id = ?";
    QVariantList params = {exchange, broker_id};
    if (type != InstrumentType::UNKNOWN) {
        sql += " AND instrument_type = ?";
        params.append(QString(instrument_type_str(type)));
    }
    sql += " ORDER BY symbol";
    auto r = db().execute(sql, params);
    if (r.is_err()) {
        LOG_WARN("InstrumentRepo", logf("Instrument query failed", {{"broker", broker_id},
                                                                    {"exchange", exchange},
                                                                    {"error", QString::fromStdString(r.error())}}));
        return {};
    }
    QVector<Instrument> results;
    auto& q = r.value();
    while (q.next())
        results.append(map_row(q));
    return results;
}

int InstrumentRepository::count(const QString& broker_id) const {
    auto r = db().execute("SELECT COUNT(*) FROM instruments WHERE broker_id = ?", {broker_id});
    if (r.is_err()) {
        // 0 makes the caller download the whole master again.
        LOG_WARN("InstrumentRepo", logf("Instrument count failed", {{"broker", broker_id},
                                                                    {"error", QString::fromStdString(r.error())}}));
        return 0;
    }
    auto& q = r.value();
    return q.next() ? q.value(0).toInt() : 0;
}

QDateTime InstrumentRepository::last_updated(const QString& broker_id) const {
    auto r = db().execute("SELECT MAX(updated_at) FROM instruments WHERE broker_id = ?", {broker_id});
    if (r.is_err()) {
        LOG_WARN("InstrumentRepo", logf("Instrument last-updated lookup failed",
                                        {{"broker", broker_id}, {"error", QString::fromStdString(r.error())}}));
        return {};
    }
    auto& q = r.value();
    if (!q.next() || q.value(0).isNull())
        return {};
    QDateTime dt = QDateTime::fromString(q.value(0).toString(), "yyyy-MM-dd HH:mm:ss");
    dt.setTimeZone(QTimeZone::UTC); // updated_at is written as datetime('now') (UTC)
    return dt;
}

} // namespace fincept::trading
