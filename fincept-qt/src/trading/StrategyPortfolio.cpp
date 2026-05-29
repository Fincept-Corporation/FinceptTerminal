// StrategyPortfolio.cpp — saved-strategy persistence (§16) + qty-freeze (§17).
#include "trading/StrategyPortfolio.h"

#include "core/logging/Logger.h"
#include "storage/sqlite/Database.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlQuery>
#include <QUuid>

namespace fincept::trading {

namespace {
constexpr const char* kLog = "StrategyPortfolio";

QJsonObject leg_to_json(const OptionsLeg& leg) {
    QJsonObject j;
    j["symbol"] = leg.symbol;
    j["exchange"] = leg.exchange;
    j["side"] = leg.side == OrderSide::Buy ? "BUY" : "SELL";
    j["quantity"] = leg.quantity;
    j["strike"] = leg.strike;
    j["option_type"] = leg.option_type;
    j["expiry"] = leg.expiry;
    return j;
}

OptionsLeg leg_from_json(const QJsonObject& j) {
    OptionsLeg leg;
    leg.symbol = j.value("symbol").toString();
    leg.exchange = j.value("exchange").toString();
    leg.side = j.value("side").toString().toUpper() == "SELL" ? OrderSide::Sell : OrderSide::Buy;
    leg.quantity = j.value("quantity").toDouble();
    leg.strike = j.value("strike").toDouble();
    leg.option_type = j.value("option_type").toString();
    leg.expiry = j.value("expiry").toString();
    return leg;
}

QString legs_to_json_str(const QVector<OptionsLeg>& legs) {
    QJsonArray arr;
    for (const auto& leg : legs)
        arr.append(leg_to_json(leg));
    return QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

QVector<OptionsLeg> legs_from_json_str(const QString& s) {
    QVector<OptionsLeg> out;
    const QJsonArray arr = QJsonDocument::fromJson(s.toUtf8()).array();
    out.reserve(arr.size());
    for (const auto& v : arr)
        out.append(leg_from_json(v.toObject()));
    return out;
}

SavedStrategy map_row(QSqlQuery& q) {
    SavedStrategy s;
    s.id = q.value("id").toString();
    s.name = q.value("name").toString();
    s.underlying = q.value("underlying").toString();
    s.notes = q.value("notes").toString();
    s.legs = legs_from_json_str(q.value("legs_json").toString());
    s.created_at = q.value("created_at").toString();
    return s;
}
} // namespace

// ── StrategyPortfolio ────────────────────────────────────────────────────────

StrategyPortfolio& StrategyPortfolio::instance() {
    static StrategyPortfolio s;
    return s;
}

QString StrategyPortfolio::save(const SavedStrategy& in) {
    SavedStrategy s = in;
    if (s.id.isEmpty())
        s.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (s.created_at.isEmpty())
        s.created_at = QDateTime::currentDateTime().toString(Qt::ISODate);

    // Upsert by primary key so save() is idempotent for a given id.
    auto r = Database::instance().execute(
        "INSERT INTO saved_strategies (id, name, underlying, notes, legs_json, created_at) "
        "VALUES (?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(id) DO UPDATE SET name=excluded.name, underlying=excluded.underlying, "
        "notes=excluded.notes, legs_json=excluded.legs_json",
        {s.id, s.name, s.underlying, s.notes, legs_to_json_str(s.legs), s.created_at});
    if (r.is_err()) {
        LOG_ERROR(kLog, QString("save failed: %1").arg(QString::fromStdString(r.error())));
        return {};
    }
    LOG_INFO(kLog, QString("Saved strategy '%1' (%2, %3 legs)").arg(s.name, s.id).arg(s.legs.size()));
    return s.id;
}

bool StrategyPortfolio::update(const SavedStrategy& s) {
    if (s.id.isEmpty())
        return false;
    auto r = Database::instance().execute(
        "UPDATE saved_strategies SET name=?, underlying=?, notes=?, legs_json=? WHERE id=?",
        {s.name, s.underlying, s.notes, legs_to_json_str(s.legs), s.id});
    if (r.is_err()) {
        LOG_ERROR(kLog, QString("update failed: %1").arg(QString::fromStdString(r.error())));
        return false;
    }
    return r.value().numRowsAffected() > 0;
}

bool StrategyPortfolio::remove(const QString& id) {
    if (id.isEmpty())
        return false;
    auto r = Database::instance().execute("DELETE FROM saved_strategies WHERE id=?", {id});
    if (r.is_err()) {
        LOG_ERROR(kLog, QString("remove failed: %1").arg(QString::fromStdString(r.error())));
        return false;
    }
    return r.value().numRowsAffected() > 0;
}

std::optional<SavedStrategy> StrategyPortfolio::get(const QString& id) const {
    auto r = Database::instance().execute("SELECT * FROM saved_strategies WHERE id=?", {id});
    if (r.is_err())
        return std::nullopt;
    auto& q = r.value();
    if (!q.next())
        return std::nullopt;
    return map_row(q);
}

QVector<SavedStrategy> StrategyPortfolio::list() const {
    QVector<SavedStrategy> out;
    auto r = Database::instance().execute(
        "SELECT * FROM saved_strategies ORDER BY created_at DESC");
    if (r.is_err())
        return out;
    auto& q = r.value();
    while (q.next())
        out.append(map_row(q));
    return out;
}

// ── Quantity freeze (§17) ────────────────────────────────────────────────────

int qty_freeze_limit(const QString& symbol, const QString& exchange) {
    if (symbol.isEmpty())
        return 0;
    auto r = Database::instance().execute(
        "SELECT freeze_qty FROM qty_freeze WHERE symbol=? AND exchange=?",
        {symbol, exchange});
    if (r.is_err())
        return 0;
    auto& q = r.value();
    if (!q.next())
        return 0;
    const int limit = q.value(0).toInt();
    return limit > 0 ? limit : 0;
}

bool set_qty_freeze_limit(const QString& symbol, const QString& exchange, int freeze_qty) {
    if (symbol.isEmpty() || exchange.isEmpty())
        return false;

    if (freeze_qty <= 0) {
        auto r = Database::instance().execute(
            "DELETE FROM qty_freeze WHERE symbol=? AND exchange=?", {symbol, exchange});
        return r.is_ok();
    }

    auto r = Database::instance().execute(
        "INSERT INTO qty_freeze (symbol, exchange, freeze_qty) VALUES (?, ?, ?) "
        "ON CONFLICT(symbol, exchange) DO UPDATE SET freeze_qty=excluded.freeze_qty",
        {symbol, exchange, freeze_qty});
    if (r.is_err()) {
        LOG_ERROR(kLog, QString("set_qty_freeze_limit failed: %1").arg(QString::fromStdString(r.error())));
        return false;
    }
    return true;
}

} // namespace fincept::trading
