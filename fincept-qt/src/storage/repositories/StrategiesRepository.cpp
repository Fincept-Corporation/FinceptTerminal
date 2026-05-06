#include "storage/repositories/StrategiesRepository.h"

#include "core/logging/Logger.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSqlError>

namespace fincept {

using fincept::services::options::Strategy;
using fincept::services::options::StrategyLeg;
using fincept::trading::InstrumentType;

namespace {

QString iso_now() { return QDateTime::currentDateTimeUtc().toString(Qt::ISODate); }

const char* type_to_str(InstrumentType t) {
    switch (t) {
        case InstrumentType::CE:    return "CE";
        case InstrumentType::PE:    return "PE";
        case InstrumentType::FUT:   return "FUT";
        case InstrumentType::EQ:    return "EQ";
        case InstrumentType::INDEX: return "INDEX";
        default:                    return "UNKNOWN";
    }
}

InstrumentType type_from_str(const QString& s) {
    if (s == "CE")    return InstrumentType::CE;
    if (s == "PE")    return InstrumentType::PE;
    if (s == "FUT")   return InstrumentType::FUT;
    if (s == "EQ")    return InstrumentType::EQ;
    if (s == "INDEX") return InstrumentType::INDEX;
    return InstrumentType::UNKNOWN;
}

}  // namespace

StrategiesRepository& StrategiesRepository::instance() {
    static StrategiesRepository s;
    return s;
}

QString StrategiesRepository::legs_to_json(const QVector<StrategyLeg>& legs) {
    QJsonArray arr;
    for (const auto& leg : legs) {
        QJsonObject o;
        o["instrument_token"] = QJsonValue(qint64(leg.instrument_token));
        o["symbol"] = leg.symbol;
        o["type"] = QString::fromLatin1(type_to_str(leg.type));
        o["strike"] = leg.strike;
        o["expiry"] = leg.expiry;
        o["lot_size"] = leg.lot_size;
        o["lots"] = leg.lots;
        o["entry_price"] = leg.entry_price;
        o["iv_at_entry"] = leg.iv_at_entry;
        o["is_active"] = leg.is_active;
        arr.append(o);
    }
    return QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

QVector<StrategyLeg> StrategiesRepository::legs_from_json(const QString& json) {
    QVector<StrategyLeg> out;
    if (json.isEmpty())
        return out;
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isArray()) {
        LOG_WARN("StrategiesRepo", QString("legs_json parse failed: %1").arg(pe.errorString()));
        return out;
    }
    for (const auto& v : doc.array()) {
        const QJsonObject o = v.toObject();
        StrategyLeg leg;
        leg.instrument_token = qint64(o.value("instrument_token").toDouble());
        leg.symbol = o.value("symbol").toString();
        leg.type = type_from_str(o.value("type").toString());
        leg.strike = o.value("strike").toDouble();
        leg.expiry = o.value("expiry").toString();
        leg.lot_size = o.value("lot_size").toInt();
        leg.lots = o.value("lots").toInt();
        leg.entry_price = o.value("entry_price").toDouble();
        leg.iv_at_entry = o.value("iv_at_entry").toDouble();
        leg.is_active = o.value("is_active").toBool(true);
        out.append(leg);
    }
    return out;
}

SavedStrategyRow StrategiesRepository::map_row(QSqlQuery& q) {
    SavedStrategyRow r;
    r.id = q.value(0).toLongLong();
    r.strategy.name = q.value(1).toString();
    r.strategy.underlying = q.value(2).toString();
    r.strategy.expiry = q.value(3).toString();
    r.strategy.created = QDateTime::fromString(q.value(4).toString(), Qt::ISODate);
    r.strategy.modified = QDateTime::fromString(q.value(5).toString(), Qt::ISODate);
    r.strategy.notes = q.value(6).toString();
    r.strategy.legs = legs_from_json(q.value(7).toString());
    return r;
}

Result<qint64> StrategiesRepository::save(const Strategy& s) {
    const QString now = iso_now();
    return exec_insert(
        "INSERT INTO strategies (name, underlying, expiry, created_at, modified_at, notes, legs_json) "
        "VALUES (?, ?, ?, ?, ?, ?, ?)",
        {s.name, s.underlying, s.expiry, now, now, s.notes, legs_to_json(s.legs)});
}

Result<void> StrategiesRepository::update(qint64 id, const Strategy& s) {
    return exec_write(
        "UPDATE strategies SET name=?, underlying=?, expiry=?, modified_at=?, notes=?, legs_json=? "
        "WHERE id=?",
        {s.name, s.underlying, s.expiry, iso_now(), s.notes, legs_to_json(s.legs), id});
}

Result<SavedStrategyRow> StrategiesRepository::get(qint64 id) {
    return query_one(
        "SELECT id, name, underlying, expiry, created_at, modified_at, notes, legs_json "
        "FROM strategies WHERE id=?",
        {id}, &StrategiesRepository::map_row);
}

Result<QVector<SavedStrategyRow>> StrategiesRepository::list_all() {
    return query_list(
        "SELECT id, name, underlying, expiry, created_at, modified_at, notes, legs_json "
        "FROM strategies ORDER BY modified_at DESC",
        {}, &StrategiesRepository::map_row);
}

Result<QVector<SavedStrategyRow>> StrategiesRepository::list_by_underlying(const QString& underlying) {
    return query_list(
        "SELECT id, name, underlying, expiry, created_at, modified_at, notes, legs_json "
        "FROM strategies WHERE underlying=? ORDER BY modified_at DESC",
        {underlying}, &StrategiesRepository::map_row);
}

Result<void> StrategiesRepository::remove(qint64 id) {
    return exec_write("DELETE FROM strategies WHERE id=?", {id});
}

} // namespace fincept
