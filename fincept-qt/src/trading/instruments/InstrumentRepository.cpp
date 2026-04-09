#include "trading/instruments/InstrumentRepository.h"

#include "core/logging/Logger.h"

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
    return i;
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
                        " expiry, strike, lot_size, instrument_type, tick_size, broker_id) "
                        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

    for (const auto& inst : instruments) {
        auto ri = exec_write(sql, {
                                      inst.instrument_token,
                                      inst.exchange_token,
                                      inst.symbol,
                                      inst.brsymbol,
                                      inst.name,
                                      inst.exchange,
                                      inst.brexchange,
                                      inst.expiry,
                                      inst.strike,
                                      inst.lot_size,
                                      QString(instrument_type_str(inst.instrument_type)),
                                      inst.tick_size,
                                      broker_id,
                                  });
        if (ri.is_err()) {
            db().rollback();
            return ri;
        }
    }

    auto rc2 = db().commit();
    if (rc2.is_err()) {
        db().rollback();
        return rc2;
    }

    LOG_INFO("InstrumentRepo", QString("Replaced %1 instruments for %2").arg(instruments.size()).arg(broker_id));
    return fincept::Result<void>::ok();
}

fincept::Result<void> InstrumentRepository::clear(const QString& broker_id) {
    return exec_write("DELETE FROM instruments WHERE broker_id = ?", {broker_id});
}

// ── Read ──────────────────────────────────────────────────────────────────────

std::optional<Instrument> InstrumentRepository::find(const QString& symbol, const QString& exchange,
                                                     const QString& broker_id) const {
    return query_optional("SELECT instrument_token, exchange_token, symbol, brsymbol, name, exchange, brexchange, "
                          "expiry, strike, lot_size, instrument_type, tick_size, broker_id "
                          "FROM instruments WHERE symbol = ? AND exchange = ? AND broker_id = ? LIMIT 1",
                          {symbol, exchange, broker_id}, map_row);
}

std::optional<Instrument> InstrumentRepository::find_by_token(qint64 instrument_token, const QString& broker_id) const {
    return query_optional("SELECT instrument_token, exchange_token, symbol, brsymbol, name, exchange, brexchange, "
                          "expiry, strike, lot_size, instrument_type, tick_size, broker_id "
                          "FROM instruments WHERE instrument_token = ? AND broker_id = ? LIMIT 1",
                          {instrument_token, broker_id}, map_row);
}

std::optional<Instrument> InstrumentRepository::find_by_brsymbol(const QString& brsymbol, const QString& brexchange,
                                                                 const QString& broker_id) const {
    return query_optional("SELECT instrument_token, exchange_token, symbol, brsymbol, name, exchange, brexchange, "
                          "expiry, strike, lot_size, instrument_type, tick_size, broker_id "
                          "FROM instruments WHERE brsymbol = ? AND brexchange = ? AND broker_id = ? LIMIT 1",
                          {brsymbol, brexchange, broker_id}, map_row);
}

QVector<Instrument> InstrumentRepository::search(const QString& query, const QString& exchange,
                                                 const QString& broker_id, int limit) const {
    QString pattern = "%" + query.toUpper() + "%";
    QString sql = "SELECT instrument_token, exchange_token, symbol, brsymbol, name, exchange, brexchange, "
                  "expiry, strike, lot_size, instrument_type, tick_size, broker_id "
                  "FROM instruments "
                  "WHERE broker_id = ? "
                  "AND (exchange = ? OR ? = '') "
                  "AND (UPPER(symbol) LIKE ? OR UPPER(brsymbol) LIKE ? OR UPPER(name) LIKE ?) "
                  "ORDER BY CASE instrument_type "
                  "  WHEN 'EQ' THEN 0 WHEN 'INDEX' THEN 1 WHEN 'FUT' THEN 2 ELSE 3 END, symbol "
                  "LIMIT ?";
    auto r = db().execute(sql, {broker_id, exchange, exchange, pattern, pattern, pattern, limit});
    if (r.is_err())
        return {};
    QVector<Instrument> results;
    auto& q = r.value();
    while (q.next())
        results.append(map_row(q));
    return results;
}

QVector<Instrument> InstrumentRepository::list(const QString& exchange, const QString& broker_id,
                                               InstrumentType type) const {
    QString sql = "SELECT instrument_token, exchange_token, symbol, brsymbol, name, exchange, brexchange, "
                  "expiry, strike, lot_size, instrument_type, tick_size, broker_id "
                  "FROM instruments WHERE exchange = ? AND broker_id = ?";
    QVariantList params = {exchange, broker_id};
    if (type != InstrumentType::UNKNOWN) {
        sql += " AND instrument_type = ?";
        params.append(QString(instrument_type_str(type)));
    }
    sql += " ORDER BY symbol";
    auto r = db().execute(sql, params);
    if (r.is_err())
        return {};
    QVector<Instrument> results;
    auto& q = r.value();
    while (q.next())
        results.append(map_row(q));
    return results;
}

int InstrumentRepository::count(const QString& broker_id) const {
    auto r = db().execute("SELECT COUNT(*) FROM instruments WHERE broker_id = ?", {broker_id});
    if (r.is_err())
        return 0;
    auto& q = r.value();
    return q.next() ? q.value(0).toInt() : 0;
}

} // namespace fincept::trading
