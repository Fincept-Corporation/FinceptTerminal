// src/storage/repositories/CustomIndexRepository.cpp
#include "storage/repositories/CustomIndexRepository.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

namespace fincept {

CustomIndexRepository& CustomIndexRepository::instance() {
    static CustomIndexRepository s;
    return s;
}

// ── JSON helpers ──────────────────────────────────────────────────────────────

QString CustomIndexRepository::constituents_to_json(const QVector<CustomIndexConstituent>& cs) {
    QJsonArray arr;
    for (const auto& c : cs) {
        QJsonObject obj;
        obj["symbol"]           = c.symbol;
        obj["weight"]           = c.weight;
        obj["price_at_create"]  = c.price_at_create;
        arr.append(obj);
    }
    return QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

QVector<CustomIndexConstituent> CustomIndexRepository::json_to_constituents(const QString& json) {
    QVector<CustomIndexConstituent> out;
    const auto doc = QJsonDocument::fromJson(json.toUtf8());
    if (!doc.isArray()) return out;
    for (const auto& v : doc.array()) {
        const auto obj = v.toObject();
        CustomIndexConstituent c;
        c.symbol           = obj["symbol"].toString();
        c.weight           = obj["weight"].toDouble();
        c.price_at_create  = obj["price_at_create"].toDouble();
        out.append(c);
    }
    return out;
}

// ── Row mappers ───────────────────────────────────────────────────────────────

CustomIndex CustomIndexRepository::map_index(QSqlQuery& q) {
    CustomIndex idx;
    idx.id           = q.value(0).toString();
    idx.name         = q.value(1).toString();
    idx.method       = q.value(2).toString();
    idx.base_value   = q.value(3).toDouble();
    idx.portfolio_id = q.value(4).toString();
    idx.constituents = json_to_constituents(q.value(5).toString());
    idx.created_at   = q.value(6).toString();
    idx.updated_at   = q.value(7).toString();
    return idx;
}

CustomIndexValue CustomIndexRepository::map_value(QSqlQuery& q) {
    CustomIndexValue v;
    v.id       = q.value(0).toLongLong();
    v.index_id = q.value(1).toString();
    v.date     = q.value(2).toString();
    v.value    = q.value(3).toDouble();
    return v;
}

// ── CRUD ──────────────────────────────────────────────────────────────────────

Result<QString> CustomIndexRepository::create(const CustomIndex& idx) {
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    auto r = exec_write(
        "INSERT INTO custom_indices (id, name, method, base_value, portfolio_id, constituents_json) "
        "VALUES (?, ?, ?, ?, ?, ?)",
        {id, idx.name, idx.method, idx.base_value,
         idx.portfolio_id.isEmpty() ? QVariant() : QVariant(idx.portfolio_id),
         constituents_to_json(idx.constituents)});
    if (r.is_err()) return Result<QString>::err(r.error());
    return Result<QString>::ok(id);
}

Result<QVector<CustomIndex>> CustomIndexRepository::list_all() {
    return query_list(
        "SELECT id, name, method, base_value, portfolio_id, constituents_json, created_at, updated_at "
        "FROM custom_indices ORDER BY created_at DESC",
        {}, map_index);
}

Result<CustomIndex> CustomIndexRepository::get(const QString& id) {
    return query_one(
        "SELECT id, name, method, base_value, portfolio_id, constituents_json, created_at, updated_at "
        "FROM custom_indices WHERE id = ?",
        {id}, map_index);
}

Result<void> CustomIndexRepository::remove(const QString& id) {
    return exec_write("DELETE FROM custom_indices WHERE id = ?", {id});
}

// ── Index values ──────────────────────────────────────────────────────────────

Result<void> CustomIndexRepository::save_value(const QString& index_id, const QString& date, double value) {
    return exec_write(
        "INSERT OR REPLACE INTO custom_index_values (index_id, date, value) VALUES (?, ?, ?)",
        {index_id, date, value});
}

Result<QVector<CustomIndexValue>> CustomIndexRepository::get_values(const QString& index_id, int limit) {
    return query_list_as<CustomIndexValue>(
        "SELECT id, index_id, date, value FROM custom_index_values "
        "WHERE index_id = ? ORDER BY date ASC LIMIT ?",
        {index_id, limit},
        std::function<CustomIndexValue(QSqlQuery&)>(map_value));
}

double CustomIndexRepository::latest_value(const QString& index_id) {
    auto r = db().execute(
        "SELECT id, index_id, date, value FROM custom_index_values "
        "WHERE index_id = ? ORDER BY date DESC LIMIT 1",
        {index_id});
    if (r.is_err()) return 0.0;
    auto& q = r.value();
    if (!q.next()) return 0.0;
    return map_value(q).value;
}

} // namespace fincept
